// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/check_codec.h>

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
    'P', 'K', 'G', 'K', 'S', 'E', '0', '1'};
constexpr std::size_t checksum_size = 32U;
constexpr std::uint32_t maximum_package_input_count = 1'000'000U;
constexpr std::uint32_t maximum_supplementary_group_count = 1'000'000U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw check_codec_error(
      check_codec_error_code::corrupt_encoding,
      "check-session encoding: " + message);
}

[[noreturn]] void unsupported(const std::string& message)
{
  throw check_codec_error(
      check_codec_error_code::unsupported_encoding,
      "check-session encoding: " + message);
}

[[noreturn]] void mismatch(const std::string& message)
{
  throw check_codec_error(
      check_codec_error_code::authority_mismatch,
      "check-session authority: " + message);
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
    if (size > maximum_check_session_encoding_size - output_.size())
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
    {
      byte(static_cast<std::uint8_t>(
          (hexadecimal_digit(hex[index]) << 4U) |
          hexadecimal_digit(hex[index + 1U])));
    }
  }

  void text(std::string_view value)
  {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
      corrupt("text field exceeds maximum size");
    u32(static_cast<std::uint32_t>(value.size()));
    raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void optional_u64(const std::optional<std::uint64_t>& value)
  {
    byte(value ? 1U : 0U);
    if (value)
      u64(*value);
  }

  [[nodiscard]] check_session_encoding finish()
  {
    const auto checksum = sha256_hex(output_.data(), output_.size());
    identity(checksum);
    if (output_.size() > maximum_check_session_encoding_size)
      corrupt("encoding exceeds maximum size");
    return std::move(output_);
  }

private:
  check_session_encoding output_;
};

class reader final {
public:
  reader(const check_session_encoding& input, std::size_t limit)
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

  std::optional<std::uint64_t> optional_u64(std::string_view field)
  {
    const auto present = byte();
    if (present > 1U)
      corrupt(std::string(field) + " presence is not canonical");
    if (present == 0U)
      return std::nullopt;
    return u64();
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

  const check_session_encoding& input_;
  std::size_t limit_;
  std::size_t offset_ = 0U;
};

void validate_checksum(const check_session_encoding& encoding)
{
  if (encoding.size() < encoding_magic.size() + 2U + checksum_size)
    corrupt("encoding is truncated");
  if (encoding.size() > maximum_check_session_encoding_size)
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
  if (input.u16() != check_session_encoding_version)
    unsupported("version is unsupported");
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

} // namespace

check_codec_error::check_codec_error(
    check_codec_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

check_codec_error_code check_codec_error::code() const noexcept
{
  return code_;
}

check_session_encoding encode_check_session(
    const transaction_check_session& session)
{
  const auto& request = session.request();
  const auto& execution = session.execution_session();
  const auto& source = execution.source();
  const auto& package = execution.package();
  const auto& inputs = execution.inputs();
  const auto& paths = execution.paths();
  const auto& identity = execution.identity();
  const auto& limits = execution.limits();

  if (inputs.size() > maximum_package_input_count ||
      identity.supplementary_groups.size() > maximum_supplementary_group_count)
  {
    corrupt("resource cardinality exceeds maximum size");
  }

  writer output;
  output.raw(encoding_magic);
  output.u16(check_session_encoding_version);
  output.identity(session.identity().hex());
  output.identity(request.transaction().identity().hex());
  output.identity(request.prepared_from_progress().hex());
  output.identity(request.check_node().hex());
  output.identity(request.identity().hex());
  output.identity(request.construction().identity().hex());
  output.identity(request.check().identity().hex());
  output.identity(session.execution_request().hex());
  output.identity(source.source.hex());
  output.identity(source.tree.hex());
  output.text(source.path.string());
  output.identity(package.artifact.hex());
  output.identity(package.tree.hex());
  output.text(package.path.string());
  output.u32(static_cast<std::uint32_t>(inputs.size()));
  for (const auto& input : inputs)
  {
    output.identity(input.input.hex());
    output.identity(input.resource.hex());
    output.text(input.path.string());
  }
  output.identity(paths.root_view.hex());
  output.text(paths.root_view_path.string());
  output.text(paths.temporary_root.string());
  output.identity(identity.interpreter.hex());
  output.u64(identity.user_id);
  output.u64(identity.group_id);
  output.u32(static_cast<std::uint32_t>(identity.supplementary_groups.size()));
  for (const auto group : identity.supplementary_groups)
    output.u64(group);
  output.identity(limits.identity().hex());
  output.optional_u64(limits.cpu_time_milliseconds());
  output.optional_u64(limits.address_space_bytes());
  output.optional_u64(limits.file_size_bytes());
  output.optional_u64(limits.open_files());
  output.optional_u64(limits.process_count());
  return output.finish();
}

transaction_check_session decode_check_session(
    const check_session_encoding& encoding,
    transaction_check_request request)
{
  validate_checksum(encoding);
  const auto payload_size = encoding.size() - checksum_size;
  reader input(encoding, payload_size);
  read_magic(input);

  auto retained_session = session_identity::from_hex(input.identity());
  auto retained_transaction = session_identity::from_hex(input.identity());
  auto retained_progress = session_identity::from_hex(input.identity());
  auto retained_node = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  auto retained_request = session_identity::from_hex(input.identity());
  auto retained_construction = session_identity::from_hex(input.identity());
  auto retained_check = pkgcheck::check_request_identity::from_sha256(
      input.identity());
  auto retained_execution = pkgexec::execution_request_identity::from_sha256(
      input.identity());

  pkgcheck_exec::source_tree source{
      pkgsource::source_snapshot_identity::from_sha256(input.identity()),
      pkgexec::resource_identity::from_sha256(input.identity()),
      decode_path(input.text(), "source tree"),
  };
  pkgcheck_exec::checked_package_tree package{
      pkgbuild::artifact_identity::from_sha256(input.identity()),
      pkgexec::resource_identity::from_sha256(input.identity()),
      decode_path(input.text(), "checked package tree"),
  };

  std::vector<pkgcheck_exec::package_input_resource> package_inputs;
  const auto input_count = input.u32();
  if (input_count > maximum_package_input_count)
    corrupt("package-input cardinality exceeds maximum size");
  package_inputs.reserve(input_count);
  for (std::uint32_t index = 0U; index < input_count; ++index)
  {
    package_inputs.push_back({
        pkgbuild::build_input_identity::from_sha256(input.identity()),
        pkgexec::resource_identity::from_sha256(input.identity()),
        decode_path(input.text(), "package input resource"),
    });
  }

  pkgcheck_exec::session_paths paths{
      pkgexec::root_view_identity::from_sha256(input.identity()),
      decode_path(input.text(), "root view"),
      decode_path(input.text(), "check temporary root"),
  };

  pkgcheck_exec::execution_identity execution_identity{
      pkgexec::interpreter_identity::from_sha256(input.identity()),
      input.u64(), input.u64(), {}};
  const auto group_count = input.u32();
  if (group_count > maximum_supplementary_group_count)
    corrupt("supplementary-group cardinality exceeds maximum size");
  execution_identity.supplementary_groups.reserve(group_count);
  for (std::uint32_t index = 0U; index < group_count; ++index)
    execution_identity.supplementary_groups.push_back(input.u64());

  auto retained_limits = pkgexec::resource_limits_identity::from_sha256(
      input.identity());
  auto cpu_time = input.optional_u64("CPU-time limit");
  auto address_space = input.optional_u64("address-space limit");
  auto file_size = input.optional_u64("file-size limit");
  auto open_files = input.optional_u64("open-files limit");
  auto process_count = input.optional_u64("process-count limit");
  input.finish();

  auto limits = [&]() {
    try
    {
      return pkgexec::resource_limits::make(
          cpu_time, address_space, file_size, open_files, process_count);
    }
    catch (const std::exception& problem)
    {
      corrupt(std::string("cannot admit retained resource limits: ") +
              problem.what());
    }
  }();

  if (request.transaction().identity() != retained_transaction ||
      request.prepared_from_progress() != retained_progress ||
      request.check_node() != retained_node ||
      request.identity() != retained_request ||
      request.construction().identity() != retained_construction ||
      request.check().identity() != retained_check)
  {
    mismatch("supplied request differs from retained authority");
  }
  if (limits.identity() != retained_limits)
    corrupt("resource-limits identity mismatch");

  transaction_check_resources resources{
      std::move(source), std::move(package), std::move(package_inputs),
      std::move(paths), std::move(execution_identity), std::move(limits)};

  try
  {
    auto session = transaction_check_session::admit(
        std::move(request), std::move(resources));
    if (session.execution_request() != retained_execution)
      corrupt("check execution-request identity mismatch");
    if (session.identity() != retained_session)
      corrupt("check-session identity mismatch");
    if (encode_check_session(session) != encoding)
      corrupt("encoding is not canonical");
    return session;
  }
  catch (const check_codec_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    corrupt(std::string("cannot admit retained session: ") + problem.what());
  }
}

} // namespace pkgctl
