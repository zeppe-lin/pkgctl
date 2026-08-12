// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/construction_codec.h>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/evp.h>

namespace pkgctl {
namespace {

constexpr std::array<std::uint8_t, 8> encoding_magic{
    'P', 'K', 'G', 'C', 'S', 'E', '0', '1'};
constexpr std::size_t checksum_size = 32U;
constexpr std::uint32_t maximum_package_input_count = 1'000'000U;
constexpr std::uint32_t maximum_supplementary_group_count = 1'000'000U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw construction_codec_error(
      construction_codec_error_code::corrupt_encoding,
      "construction-session encoding: " + message);
}

[[noreturn]] void unsupported(const std::string& message)
{
  throw construction_codec_error(
      construction_codec_error_code::unsupported_encoding,
      "construction-session encoding: " + message);
}

[[noreturn]] void mismatch(const std::string& message)
{
  throw construction_codec_error(
      construction_codec_error_code::authority_mismatch,
      "construction-session authority: " + message);
}

std::string sha256_hex(const std::uint8_t* bytes, std::size_t size)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (size != 0U && EVP_DigestUpdate(context.get(), bytes, size) != 1))
  {
    corrupt("cannot hash encoding");
  }

  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_size = 0U;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1 ||
      digest_size != checksum_size)
  {
    corrupt("cannot finalize checksum");
  }

  static constexpr char digits[] = "0123456789abcdef";
  std::string result(checksum_size * 2U, '0');
  for (std::size_t index = 0U; index < checksum_size; ++index)
  {
    result[index * 2U] = digits[(digest[index] >> 4U) & 0x0fU];
    result[index * 2U + 1U] = digits[digest[index] & 0x0fU];
  }
  return result;
}

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  corrupt("identity contains invalid hex");
}

class writer final {
public:
  void raw(const std::uint8_t* bytes, std::size_t size)
  {
    if (size > maximum_construction_session_encoding_size - output_.size())
      corrupt("encoding exceeds maximum size");
    if (size == 0U)
      return;
    output_.insert(output_.end(), bytes, bytes + size);
  }

  template<std::size_t Size>
  void raw(const std::array<std::uint8_t, Size>& bytes)
  {
    raw(bytes.data(), bytes.size());
  }

  void byte(std::uint8_t value)
  {
    raw(&value, 1U);
  }

  void u16(std::uint16_t value)
  {
    byte(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    byte(static_cast<std::uint8_t>(value & 0xffU));
  }

  void u32(std::uint32_t value)
  {
    for (int shift = 24; shift >= 0; shift -= 8)
      byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void identity(std::string_view hex)
  {
    if (hex.size() != 64U)
      corrupt("identity has invalid size");
    for (std::size_t index = 0U; index < hex.size(); index += 2U)
      byte(static_cast<std::uint8_t>(
          (hexadecimal_digit(hex[index]) << 4U) |
          hexadecimal_digit(hex[index + 1U])));
  }

  void text(std::string_view value)
  {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
      corrupt("text field exceeds maximum size");
    u32(static_cast<std::uint32_t>(value.size()));
    raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  [[nodiscard]] construction_session_encoding finish()
  {
    const auto checksum = sha256_hex(output_.data(), output_.size());
    identity(checksum);
    if (output_.size() > maximum_construction_session_encoding_size)
      corrupt("encoding exceeds maximum size");
    return std::move(output_);
  }

private:
  construction_session_encoding output_;
};

class reader final {
public:
  reader(const construction_session_encoding& input, std::size_t limit)
      : input_(input), limit_(limit)
  {
  }

  std::uint8_t byte()
  {
    require(1U);
    return input_[offset_++];
  }

  std::uint16_t u16()
  {
    std::uint16_t value = 0U;
    for (unsigned int index = 0U; index < 2U; ++index)
      value = static_cast<std::uint16_t>((value << 8U) | byte());
    return value;
  }

  std::uint32_t u32()
  {
    std::uint32_t value = 0U;
    for (unsigned int index = 0U; index < 4U; ++index)
      value = (value << 8U) | byte();
    return value;
  }

  std::uint64_t u64()
  {
    std::uint64_t value = 0U;
    for (unsigned int index = 0U; index < 8U; ++index)
      value = (value << 8U) | byte();
    return value;
  }

  std::string identity()
  {
    static constexpr char digits[] = "0123456789abcdef";
    require(32U);
    std::string value(64U, '0');
    for (std::size_t index = 0U; index < 32U; ++index)
    {
      const auto current = input_[offset_++];
      value[index * 2U] = digits[(current >> 4U) & 0x0fU];
      value[index * 2U + 1U] = digits[current & 0x0fU];
    }
    return value;
  }

  std::string text()
  {
    const auto size = static_cast<std::size_t>(u32());
    require(size);
    std::string value(
        reinterpret_cast<const char*>(input_.data() + offset_), size);
    offset_ += size;
    return value;
  }

  void finish() const
  {
    if (offset_ != limit_)
      corrupt("encoding has trailing data");
  }

private:
  void require(std::size_t size) const
  {
    if (offset_ > limit_ || size > limit_ - offset_)
      corrupt("encoding is truncated");
  }

  const construction_session_encoding& input_;
  std::size_t limit_;
  std::size_t offset_ = 0U;
};

void validate_checksum(const construction_session_encoding& encoding)
{
  if (encoding.size() < encoding_magic.size() + 2U + checksum_size)
    corrupt("encoding is truncated");
  if (encoding.size() > maximum_construction_session_encoding_size)
    corrupt("encoding exceeds maximum size");

  const auto payload_size = encoding.size() - checksum_size;
  const auto expected = sha256_hex(encoding.data(), payload_size);
  static constexpr char digits[] = "0123456789abcdef";
  std::string retained(checksum_size * 2U, '0');
  for (std::size_t index = 0U; index < checksum_size; ++index)
  {
    const auto current = encoding[payload_size + index];
    retained[index * 2U] = digits[(current >> 4U) & 0x0fU];
    retained[index * 2U + 1U] = digits[current & 0x0fU];
  }
  if (retained != expected)
    corrupt("checksum mismatch");
}

void read_magic(reader& input)
{
  for (const auto value : encoding_magic)
    if (input.byte() != value)
      corrupt("invalid magic");
  if (input.u16() != construction_session_encoding_version)
    unsupported("version is unsupported");
}

std::uint8_t encode_boolean(bool value)
{
  return value ? 1U : 0U;
}

bool decode_boolean(std::uint8_t value, std::string_view field)
{
  if (value > 1U)
    corrupt(std::string(field) + " is not a canonical boolean");
  return value != 0U;
}

std::filesystem::path decode_path(std::string value, std::string_view field)
{
  if (value.empty())
    corrupt(std::string(field) + " is empty");
  std::filesystem::path path(std::move(value));
  if (!path.is_absolute() || path.lexically_normal() != path)
    corrupt(std::string(field) + " is not canonical absolute authority");
  return path;
}

pkgbuild::artifact_compression decode_compression(std::uint8_t value)
{
  if (value != static_cast<std::uint8_t>(pkgbuild::artifact_compression::none))
    corrupt("construction compression is not admitted by this encoding");
  return pkgbuild::artifact_compression::none;
}

} // namespace

construction_codec_error::construction_codec_error(
    construction_codec_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

construction_codec_error_code construction_codec_error::code() const noexcept
{
  return code_;
}

construction_session_encoding encode_construction_session(
    const construction_session& session)
{
  const auto& request = session.request();
  const auto& build_policy = request.build_policy();
  const auto& environment = build_policy.environment();
  const auto& acquisition = request.acquisition_policy();
  const auto& paths = session.paths();
  const auto& inputs = session.package_inputs();
  const auto& execution = session.execution_identity();

  if (inputs.size() > maximum_package_input_count ||
      execution.supplementary_groups.size() > maximum_supplementary_group_count)
  {
    corrupt("resource cardinality exceeds maximum size");
  }

  writer output;
  output.raw(encoding_magic);
  output.u16(construction_session_encoding_version);
  output.identity(session.identity().hex());
  output.identity(request.transaction().identity().hex());
  output.identity(request.build_node().hex());
  output.identity(request.identity().hex());
  output.identity(request.build().identity().hex());
  output.identity(environment.identity().hex());
  output.identity(build_policy.identity().hex());
  output.u32(environment.parallelism());
  output.u32(environment.file_creation_mask());
  output.byte(encode_boolean(environment.source_date_epoch().has_value()));
  if (environment.source_date_epoch())
    output.u64(static_cast<std::uint64_t>(*environment.source_date_epoch()));
  output.byte(static_cast<std::uint8_t>(build_policy.output_layout()));
  output.u64(acquisition.max_object_bytes());
  output.u32(acquisition.max_redirects());
  output.byte(encode_boolean(acquisition.allow_http()));
  output.byte(encode_boolean(acquisition.allow_https()));
  output.text(paths.local_source_root.string());
  output.text(paths.content_store_root.string());
  output.identity(paths.build.root_view.hex());
  output.text(paths.build.root_view_path.string());
  output.text(paths.build.session_root.string());
  output.text(paths.build.package_output_root.string());
  output.text(paths.build.artifact_path.string());
  output.u32(static_cast<std::uint32_t>(inputs.size()));
  for (const auto& input : inputs)
  {
    output.identity(input.input.hex());
    output.identity(input.resource.hex());
    output.text(input.path.string());
  }
  output.identity(execution.interpreter.hex());
  output.u64(execution.user_id);
  output.u64(execution.group_id);
  output.u32(static_cast<std::uint32_t>(execution.supplementary_groups.size()));
  for (const auto group : execution.supplementary_groups)
    output.u64(group);
  output.byte(static_cast<std::uint8_t>(session.compression()));
  return output.finish();
}

construction_session decode_construction_session(
    const construction_session_encoding& encoding,
    transaction_session transaction,
    pkgtransaction::transaction_node_identity build_node)
{
  validate_checksum(encoding);
  const auto payload_size = encoding.size() - checksum_size;
  reader input(encoding, payload_size);
  read_magic(input);

  auto retained_session = session_identity::from_hex(input.identity());
  auto retained_transaction = session_identity::from_hex(input.identity());
  auto retained_node = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  auto retained_request = session_identity::from_hex(input.identity());
  auto retained_build = pkgbuild::build_request_identity::from_sha256(
      input.identity());
  auto retained_environment = pkgbuild::environment_policy_identity::from_sha256(
      input.identity());
  auto retained_build_policy = pkgbuild::build_policy_identity::from_sha256(
      input.identity());
  const auto parallelism = input.u32();
  const auto file_creation_mask = input.u32();
  const bool has_epoch = decode_boolean(input.byte(), "SOURCE_DATE_EPOCH presence");
  std::optional<std::int64_t> source_date_epoch;
  if (has_epoch)
  {
    const auto value = input.u64();
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
      corrupt("SOURCE_DATE_EPOCH exceeds signed range");
    source_date_epoch = static_cast<std::int64_t>(value);
  }
  const auto output_layout = input.byte();
  if (output_layout !=
      static_cast<std::uint8_t>(pkgbuild::output_layout_kind::package_root))
    corrupt("unknown build output layout");
  const auto max_object_bytes = input.u64();
  const auto max_redirects = input.u32();
  const bool allow_http = decode_boolean(input.byte(), "allow-http");
  const bool allow_https = decode_boolean(input.byte(), "allow-https");

  construction_paths paths{
      decode_path(input.text(), "local source root"),
      decode_path(input.text(), "content store root"),
      {
          pkgexec::root_view_identity::from_sha256(input.identity()),
          decode_path(input.text(), "root view"),
          decode_path(input.text(), "session root"),
          decode_path(input.text(), "package output root"),
          decode_path(input.text(), "artifact path"),
      },
  };

  const auto input_count = input.u32();
  if (input_count > maximum_package_input_count)
    corrupt("package-input cardinality exceeds maximum size");
  std::vector<pkgbuild_exec::package_input_resource> package_inputs;
  package_inputs.reserve(input_count);
  for (std::uint32_t index = 0U; index < input_count; ++index)
  {
    package_inputs.push_back({
        pkgbuild::build_input_identity::from_sha256(input.identity()),
        pkgexec::resource_identity::from_sha256(input.identity()),
        decode_path(input.text(), "package input resource"),
    });
  }

  pkgbuild_exec::execution_identity execution{
      pkgexec::interpreter_identity::from_sha256(input.identity()),
      input.u64(), input.u64(), {}};
  const auto group_count = input.u32();
  if (group_count > maximum_supplementary_group_count)
    corrupt("supplementary-group cardinality exceeds maximum size");
  execution.supplementary_groups.reserve(group_count);
  for (std::uint32_t index = 0U; index < group_count; ++index)
    execution.supplementary_groups.push_back(input.u64());
  const auto compression = decode_compression(input.byte());
  input.finish();

  if (transaction.identity() != retained_transaction ||
      build_node != retained_node)
  {
    mismatch("supplied transaction/node differs from retained authority");
  }

  try
  {
    auto environment = pkgbuild::environment_policy::hermetic(
        parallelism, file_creation_mask, source_date_epoch);
    if (environment.identity() != retained_environment)
      corrupt("environment-policy identity mismatch");
    auto build_policy = pkgbuild::build_policy::make(
        std::move(environment), pkgbuild::output_layout_kind::package_root);
    if (build_policy.identity() != retained_build_policy)
      corrupt("build-policy identity mismatch");
    pkgfetch::acquisition_policy acquisition(
        max_object_bytes, max_redirects, allow_http, allow_https);
    auto request = construction_request::make(
        std::move(transaction), std::move(build_node), std::move(build_policy),
        std::move(acquisition));
    if (request.identity() != retained_request ||
        request.build().identity() != retained_build)
    {
      mismatch("retained request differs from supplied transaction semantics");
    }

    auto session = construction_session::admit(
        std::move(request), std::move(paths), std::move(package_inputs),
        std::move(execution), compression);
    if (session.identity() != retained_session)
      corrupt("construction-session identity mismatch");
    if (encode_construction_session(session) != encoding)
      corrupt("encoding is not canonical");
    return session;
  }
  catch (const construction_codec_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    corrupt(std::string("cannot admit retained session: ") + problem.what());
  }
}

} // namespace pkgctl
