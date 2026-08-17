// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/operation_codec.h>

#include "operation_admission.h"

#include <array>
#include <charconv>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <openssl/evp.h>

#include <libpkgplan/object_fact.h>

namespace pkgctl {
namespace {

constexpr std::array<std::uint8_t, 8> encoding_magic{
    'P', 'K', 'G', 'O', 'S', 'E', '0', '1'};
constexpr std::size_t checksum_size = 32U;
constexpr std::uint32_t maximum_observation_count = 1'000'000U;
constexpr std::uint32_t maximum_policy_override_count = 1'000'000U;
constexpr std::uint32_t maximum_lifecycle_count = 1'000'000U;
constexpr std::uint32_t maximum_supplementary_group_count = 1'000'000U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw operation_codec_error(
      operation_codec_error_code::corrupt_encoding,
      "operation-session encoding: " + message);
}

[[noreturn]] void unsupported(const std::string& message)
{
  throw operation_codec_error(
      operation_codec_error_code::unsupported_encoding,
      "operation-session encoding: " + message);
}

[[noreturn]] void mismatch(const std::string& message)
{
  throw operation_codec_error(
      operation_codec_error_code::authority_mismatch,
      "operation-session authority: " + message);
}

std::string sha256_hex(const std::uint8_t* bytes, std::size_t size)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (size != 0U && EVP_DigestUpdate(context.get(), bytes, size) != 1))
    corrupt("cannot hash encoding");

  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_size = 0U;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1 ||
      digest_size != checksum_size)
    corrupt("cannot finalize checksum");

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
    if (size > maximum_operation_session_encoding_size - output_.size())
      corrupt("encoding exceeds maximum size");
    if (size != 0U)
      output_.insert(output_.end(), bytes, bytes + size);
  }

  template<std::size_t Size>
  void raw(const std::array<std::uint8_t, Size>& bytes)
  {
    raw(bytes.data(), bytes.size());
  }

  void byte(std::uint8_t value) { raw(&value, 1U); }

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

  void optional_u64(const std::optional<std::uint64_t>& value)
  {
    byte(value ? 1U : 0U);
    if (value)
      u64(*value);
  }

  [[nodiscard]] operation_session_encoding finish()
  {
    const auto checksum = sha256_hex(output_.data(), output_.size());
    identity(checksum);
    if (output_.size() > maximum_operation_session_encoding_size)
      corrupt("encoding exceeds maximum size");
    return std::move(output_);
  }

private:
  operation_session_encoding output_;
};

/*
 * This reader owns a mutable cursor.  Under C++17, function arguments are not
 * evaluated left-to-right, so callers must sequence distinct field reads in
 * separate statements before passing the resulting values onward.
 */
class reader final {
public:
  reader(const operation_session_encoding& input, std::size_t limit)
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

  const operation_session_encoding& input_;
  std::size_t limit_;
  std::size_t offset_ = 0U;
};

void validate_checksum(const operation_session_encoding& encoding)
{
  if (encoding.size() < encoding_magic.size() + 2U + checksum_size)
    corrupt("encoding is truncated");
  if (encoding.size() > maximum_operation_session_encoding_size)
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

void write_magic(writer& output)
{
  output.raw(encoding_magic);
  output.u16(operation_session_encoding_version);
}

void read_magic(reader& input)
{
  for (const auto value : encoding_magic)
    if (input.byte() != value)
      corrupt("invalid magic");
  if (input.u16() != operation_session_encoding_version)
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

std::int64_t decode_i64(std::string value, std::string_view field)
{
  std::int64_t result = 0;
  const auto parsed = std::from_chars(
      value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    corrupt(std::string(field) + " is not a canonical signed integer");
  return result;
}

template<typename Enum>
Enum checked_enum(std::uint8_t value, std::uint8_t maximum,
                  std::string_view field)
{
  if (value > maximum)
    corrupt(std::string(field) + " has an invalid enum value");
  return static_cast<Enum>(value);
}

void encode_observation(writer& output,
                        const pkgplan::target_path_observation& observation)
{
  output.text(observation.path().string());
  output.byte(observation.is_present() ? 1U : 0U);
  if (!observation.is_present())
    return;

  const auto* object = observation.object();
  if (object == nullptr)
    corrupt("present target observation lacks object metadata");
  output.byte(static_cast<std::uint8_t>(object->kind()));
  output.u32(object->mode());
  output.u64(object->uid());
  output.u64(object->gid());
  output.optional_u64(object->size());
  output.byte(object->mtime() ? 1U : 0U);
  if (object->mtime())
  {
    output.text(std::to_string(object->mtime()->seconds()));
    output.u32(object->mtime()->nanoseconds());
  }
  output.byte(object->regular_content() ? 1U : 0U);
  if (object->regular_content())
    output.text(object->regular_content()->string());
  output.byte(object->symlink_target() ? 1U : 0U);
  if (object->symlink_target())
    output.text(*object->symlink_target());
  output.byte(object->device() ? 1U : 0U);
  if (object->device())
  {
    output.u64(object->device()->major());
    output.u64(object->device()->minor());
  }
}

pkgplan::target_path_observation decode_observation(reader& input)
{
  auto path = pkgplan::package_path::parse(input.text());
  const auto present = input.byte();
  if (present > 1U)
    corrupt("target observation presence is not canonical");
  if (present == 0U)
    return pkgplan::target_path_observation::absent(std::move(path));

  const auto kind = checked_enum<pkgplan::filesystem_object_kind>(
      input.byte(), static_cast<std::uint8_t>(pkgplan::filesystem_object_kind::other),
      "filesystem object kind");
  const auto mode = input.u32();
  const auto uid = input.u64();
  const auto gid = input.u64();
  auto size = input.optional_u64("filesystem object size");

  std::optional<pkgplan::object_timestamp> mtime;
  const auto has_mtime = input.byte();
  if (has_mtime > 1U)
    corrupt("filesystem timestamp presence is not canonical");
  if (has_mtime != 0U)
  {
    const auto seconds =
        decode_i64(input.text(), "filesystem timestamp");
    const auto nanoseconds = input.u32();
    mtime.emplace(seconds, nanoseconds);
  }

  std::optional<pkgplan::filesystem_regular_content_identity> content;
  const auto has_content = input.byte();
  if (has_content > 1U)
    corrupt("filesystem content presence is not canonical");
  if (has_content != 0U)
    content = pkgplan::filesystem_regular_content_identity::parse(input.text());

  std::optional<std::string> symlink;
  const auto has_symlink = input.byte();
  if (has_symlink > 1U)
    corrupt("symlink-target presence is not canonical");
  if (has_symlink != 0U)
    symlink = input.text();

  std::optional<pkgplan::device_number> device;
  const auto has_device = input.byte();
  if (has_device > 1U)
    corrupt("device-number presence is not canonical");
  if (has_device != 0U)
  {
    const auto major = input.u64();
    const auto minor = input.u64();
    device.emplace(major, minor);
  }

  return pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          std::move(path),
          pkgplan::filesystem_object_metadata(
              kind, mode, uid, gid, std::move(size), std::move(mtime),
              std::move(content), std::move(symlink), std::move(device))));
}

void encode_observations(writer& output,
                         const pkgplan::target_observation_set& observations)
{
  output.text(observations.identity().string());
  output.text(observations.target().string());
  output.byte(static_cast<std::uint8_t>(observations.completeness()));
  if (observations.observations().size() > maximum_observation_count)
    corrupt("target-observation cardinality exceeds maximum size");
  output.u32(static_cast<std::uint32_t>(observations.observations().size()));
  for (const auto& observation : observations.observations())
    encode_observation(output, observation);
}

pkgplan::target_observation_set decode_observations(reader& input)
{
  auto identity = pkgplan::observation_set_identity::parse(input.text());
  auto target = pkgplan::target_system_context_identity::parse(input.text());
  const auto completeness = checked_enum<pkgplan::fact_set_completeness>(
      input.byte(), static_cast<std::uint8_t>(pkgplan::fact_set_completeness::partial),
      "fact-set completeness");
  const auto count = input.u32();
  if (count > maximum_observation_count)
    corrupt("target-observation cardinality exceeds maximum size");
  std::vector<pkgplan::target_path_observation> observations;
  observations.reserve(count);
  for (std::uint32_t index = 0U; index < count; ++index)
    observations.push_back(decode_observation(input));
  return pkgplan::target_observation_set(
      std::move(identity), std::move(target), completeness,
      std::move(observations));
}

void encode_target(writer& output,
                   const pkgapply::application_target_context& target)
{
  output.text(target.identity().string());
  output.text(target.target().string());
  output.text(target.managed_target().string());
  output.text(target.root_view().string());
  output.text(target.observation_backend().string());
  output.text(target.mutation_backend().string());
  output.text(target.mutation_exclusion_domain().string());
  output.text(target.active_namespace().string());
  output.text(target.rejected_store().string());
  output.text(target.staging_namespace().string());
  output.text(target.journal_namespace().string());
  output.text(target.capabilities().string());
  output.byte(target.lifecycle_executor() ? 1U : 0U);
  if (target.lifecycle_executor())
    output.text(target.lifecycle_executor()->string());
}

pkgapply::application_target_context decode_target(reader& input)
{
  const auto retained = pkgapply::application_target_context_identity::parse(
      input.text());
  auto target_system =
      pkgplan::target_system_context_identity::parse(input.text());
  auto managed_target = pkgapply::managed_target_identity::parse(input.text());
  auto root_view = pkgapply::root_view_identity::parse(input.text());
  auto observation_backend =
      pkgapply::observation_backend_identity::parse(input.text());
  auto mutation_backend =
      pkgapply::mutation_backend_identity::parse(input.text());
  auto mutation_exclusion_domain =
      pkgapply::mutation_exclusion_domain_identity::parse(input.text());
  auto active_namespace =
      pkgapply::active_object_namespace_identity::parse(input.text());
  auto rejected_store =
      pkgapply::rejected_object_store_identity::parse(input.text());
  auto staging_namespace =
      pkgapply::staging_namespace_identity::parse(input.text());
  auto journal_namespace =
      pkgapply::journal_namespace_identity::parse(input.text());
  auto capabilities =
      pkgapply::execution_capability_profile_identity::parse(input.text());

  std::optional<pkgapply::lifecycle_executor_identity> lifecycle_executor;
  const auto has_lifecycle_executor = input.byte();
  if (has_lifecycle_executor > 1U)
    corrupt("lifecycle-executor presence is not canonical");
  if (has_lifecycle_executor != 0U)
    lifecycle_executor =
        pkgapply::lifecycle_executor_identity::parse(input.text());

  auto target = pkgapply::application_target_context::make(
      std::move(target_system), std::move(managed_target),
      std::move(root_view), std::move(observation_backend),
      std::move(mutation_backend), std::move(mutation_exclusion_domain),
      std::move(active_namespace), std::move(rejected_store),
      std::move(staging_namespace), std::move(journal_namespace),
      std::move(capabilities), std::move(lifecycle_executor));
  if (target.identity() != retained)
    corrupt("application-target identity mismatch");
  return target;
}

void encode_control(writer& output,
                    const pkgapply::application_execution_control& control)
{
  output.text(control.identity().string());
  output.byte(static_cast<std::uint8_t>(control.recovery()));
  output.byte(static_cast<std::uint8_t>(control.durability()));
  output.byte(static_cast<std::uint8_t>(control.cancellation()));
  output.optional_u64(control.maximum_staging_bytes());
  output.optional_u64(control.maximum_recovery_bytes());
}

pkgapply::application_execution_control decode_control(reader& input)
{
  const auto retained = pkgapply::application_execution_control_identity::parse(
      input.text());
  const auto recovery = checked_enum<pkgapply::application_recovery_requirement>(
      input.byte(),
      static_cast<std::uint8_t>(pkgapply::application_recovery_requirement::exact_prior_state),
      "application recovery requirement");
  const auto durability = checked_enum<pkgapply::application_durability_requirement>(
      input.byte(),
      static_cast<std::uint8_t>(pkgapply::application_durability_requirement::all_application_domains),
      "application durability requirement");
  const auto cancellation = checked_enum<pkgapply::application_cancellation_policy>(
      input.byte(),
      static_cast<std::uint8_t>(pkgapply::application_cancellation_policy::recover_after_target_mutation),
      "application cancellation policy");
  auto maximum_staging_bytes =
      input.optional_u64("maximum staging bytes");
  auto maximum_recovery_bytes =
      input.optional_u64("maximum recovery bytes");
  auto control = pkgapply::application_execution_control::make(
      recovery, durability, cancellation, maximum_staging_bytes,
      maximum_recovery_bytes);
  if (control.identity() != retained)
    corrupt("application-control identity mismatch");
  return control;
}

void encode_lifecycle_order(writer& output, const lifecycle_order& lifecycle)
{
  if (lifecycle.before().size() > maximum_lifecycle_count ||
      lifecycle.after().size() > maximum_lifecycle_count)
    corrupt("lifecycle cardinality exceeds maximum size");
  output.u32(static_cast<std::uint32_t>(lifecycle.before().size()));
  for (const auto& node : lifecycle.before())
    output.identity(node.hex());
  output.u32(static_cast<std::uint32_t>(lifecycle.after().size()));
  for (const auto& node : lifecycle.after())
    output.identity(node.hex());
}

lifecycle_order decode_lifecycle_order(reader& input)
{
  const auto before_count = input.u32();
  if (before_count > maximum_lifecycle_count)
    corrupt("pre-lifecycle cardinality exceeds maximum size");
  std::vector<pkgtransaction::transaction_node_identity> before;
  before.reserve(before_count);
  for (std::uint32_t index = 0U; index < before_count; ++index)
    before.push_back(pkgtransaction::transaction_node_identity::from_sha256(
        input.identity()));

  const auto after_count = input.u32();
  if (after_count > maximum_lifecycle_count)
    corrupt("post-lifecycle cardinality exceeds maximum size");
  std::vector<pkgtransaction::transaction_node_identity> after;
  after.reserve(after_count);
  for (std::uint32_t index = 0U; index < after_count; ++index)
    after.push_back(pkgtransaction::transaction_node_identity::from_sha256(
        input.identity()));
  return lifecycle_order::make(std::move(before), std::move(after));
}

void encode_installation_reason(
    writer& output,
    const std::optional<pkgstate::installation_reason>& reason)
{
  output.byte(reason ? 1U : 0U);
  if (!reason)
    return;
  output.byte(static_cast<std::uint8_t>(reason->kind()));
  switch (reason->kind())
  {
    case pkgstate::installation_reason_kind::explicit_request:
      break;
    case pkgstate::installation_reason_kind::runtime_dependency:
      output.text(reason->issuer_package()->name());
      break;
    case pkgstate::installation_reason_kind::profile_membership:
      output.text(reason->issuer_profile()->name());
      output.text(reason->issuer_profile_identity()->string());
      break;
    case pkgstate::installation_reason_kind::system_policy:
      output.text(*reason->policy());
      break;
  }
}

std::optional<pkgstate::installation_reason> decode_installation_reason(
    reader& input)
{
  const auto present = input.byte();
  if (present > 1U)
    corrupt("installation-reason presence is not canonical");
  if (present == 0U)
    return std::nullopt;

  const auto kind = checked_enum<pkgstate::installation_reason_kind>(
      input.byte(),
      static_cast<std::uint8_t>(pkgstate::installation_reason_kind::system_policy),
      "installation reason kind");
  switch (kind)
  {
    case pkgstate::installation_reason_kind::explicit_request:
      return pkgstate::installation_reason::explicit_request();
    case pkgstate::installation_reason_kind::runtime_dependency:
      return pkgstate::installation_reason::runtime_dependency(
          pkgstate::package_reference(input.text()));
    case pkgstate::installation_reason_kind::profile_membership:
    {
      auto profile = pkgstate::profile_reference(input.text());
      auto profile_identity = pkgstate::source_profile_identity::parse(
          input.text());
      return pkgstate::installation_reason::profile_membership(
          std::move(profile), std::move(profile_identity));
    }
    case pkgstate::installation_reason_kind::system_policy:
      return pkgstate::installation_reason::system_policy(input.text());
  }
  corrupt("installation reason kind is invalid");
}

void encode_specification(
    writer& output,
    const native_transaction_operation_specification& specification)
{
  output.byte(static_cast<std::uint8_t>(specification.kind()));
  output.identity(specification.action_node().hex());
  encode_target(output, specification.target());
  encode_control(output, specification.control());
  encode_observations(output, specification.observations());
  output.byte(specification.runtime_closure() ? 1U : 0U);
  if (specification.runtime_closure())
    output.text(specification.runtime_closure()->string());
  encode_lifecycle_order(output, specification.lifecycle());
  encode_installation_reason(output, specification.installation_reason());
}

native_transaction_operation_specification decode_specification(reader& input)
{
  const auto kind = checked_enum<pkgplan::operation_kind>(
      input.byte(), static_cast<std::uint8_t>(pkgplan::operation_kind::remove),
      "operation kind");
  auto action = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  auto target = decode_target(input);
  auto control = decode_control(input);
  auto observations = decode_observations(input);
  const auto has_closure = input.byte();
  if (has_closure > 1U)
    corrupt("runtime-closure presence is not canonical");
  std::optional<pkgplan::runtime_dependency_closure_identity> closure;
  if (has_closure != 0U)
    closure = pkgplan::runtime_dependency_closure_identity::parse(input.text());
  auto lifecycle = decode_lifecycle_order(input);
  auto reason = decode_installation_reason(input);

  switch (kind)
  {
    case pkgplan::operation_kind::install:
      if (!closure || !reason)
        corrupt("installation specification lacks required authority");
      return native_transaction_operation_specification::install(
          std::move(action), std::move(target), std::move(control),
          std::move(observations), *closure, std::move(lifecycle), *reason);
    case pkgplan::operation_kind::upgrade:
      if (!closure || reason)
        corrupt("upgrade specification has invalid optional authority");
      return native_transaction_operation_specification::upgrade(
          std::move(action), std::move(target), std::move(control),
          std::move(observations), *closure, std::move(lifecycle));
    case pkgplan::operation_kind::remove:
      if (closure || reason)
        corrupt("removal specification has invalid optional authority");
      return native_transaction_operation_specification::remove(
          std::move(action), std::move(target), std::move(control),
          std::move(observations), std::move(lifecycle));
  }
  corrupt("operation kind is invalid");
}

void encode_lifecycle_configuration(
    writer& output,
    const native_transaction_lifecycle_configuration& lifecycle)
{
  output.identity(lifecycle.execution_root.hex());
  output.text(lifecycle.execution_root_path.string());
  output.text(lifecycle.target_root_path.string());
  output.text(lifecycle.session_root.string());
  output.identity(lifecycle.execution_identity.interpreter.hex());
  output.u64(lifecycle.execution_identity.user_id);
  output.u64(lifecycle.execution_identity.group_id);
  if (lifecycle.execution_identity.supplementary_groups.size() >
      maximum_supplementary_group_count)
    corrupt("supplementary-group cardinality exceeds maximum size");
  output.u32(static_cast<std::uint32_t>(
      lifecycle.execution_identity.supplementary_groups.size()));
  for (const auto group : lifecycle.execution_identity.supplementary_groups)
    output.u64(group);
}

native_transaction_lifecycle_configuration decode_lifecycle_configuration(
    reader& input)
{
  auto execution_root =
      pkgexec::root_view_identity::from_sha256(input.identity());
  auto execution_root_path =
      decode_path(input.text(), "lifecycle execution root");
  auto target_root_path =
      decode_path(input.text(), "lifecycle target root");
  auto session_root = decode_path(input.text(), "lifecycle session root");
  auto interpreter = pkgexec::interpreter_identity::from_sha256(
      input.identity());
  const auto user_id = input.u64();
  const auto group_id = input.u64();

  native_transaction_lifecycle_configuration lifecycle{
      std::move(execution_root), std::move(execution_root_path),
      std::move(target_root_path), std::move(session_root),
      {std::move(interpreter), user_id, group_id, {}}};
  const auto count = input.u32();
  if (count > maximum_supplementary_group_count)
    corrupt("supplementary-group cardinality exceeds maximum size");
  lifecycle.execution_identity.supplementary_groups.reserve(count);
  for (std::uint32_t index = 0U; index < count; ++index)
    lifecycle.execution_identity.supplementary_groups.push_back(input.u64());
  return lifecycle;
}

} // namespace

operation_codec_error::operation_codec_error(
    operation_codec_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

operation_codec_error_code operation_codec_error::code() const noexcept
{
  return code_;
}

operation_session_encoding encode_operation_session(
    const transaction_dispatch& dispatch,
    const native_transaction_operation_specification& specification,
    const native_transaction_lifecycle_configuration& lifecycle,
    const effectful_operation_session& session)
{
  writer output;
  write_magic(output);
  output.identity(session.identity().hex());
  output.identity(session.request().transaction().identity().hex());
  output.identity(dispatch.identity().hex());
  output.identity(dispatch.unit().primary_node().hex());
  output.identity(session.request().identity().hex());
  encode_specification(output, specification);
  encode_lifecycle_configuration(output, lifecycle);
  return output.finish();
}

effectful_operation_session decode_operation_session(
    const operation_session_encoding& encoding,
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgplan::package_policy_snapshot& policy)
{
  validate_checksum(encoding);
  const auto payload_size = encoding.size() - checksum_size;
  reader input(encoding, payload_size);
  read_magic(input);

  const auto retained_session = session_identity::from_hex(input.identity());
  const auto retained_transaction = session_identity::from_hex(input.identity());
  const auto retained_dispatch = session_identity::from_hex(input.identity());
  const auto retained_node = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  const auto retained_request = session_identity::from_hex(input.identity());
  auto specification = decode_specification(input);
  auto lifecycle = decode_lifecycle_configuration(input);
  input.finish();

  if (progress.transaction().identity() != retained_transaction ||
      dispatch.identity() != retained_dispatch ||
      dispatch.unit().primary_node() != retained_node)
    mismatch("supplied transaction/dispatch differs from retained authority");

  try
  {
    auto session = detail::admit_native_operation_session(
        record, progress, dispatch, specification, policy, lifecycle);
    if (session.request().identity() != retained_request ||
        session.identity() != retained_session)
      mismatch("retained operation session differs from supplied semantics");
    if (encode_operation_session(
            dispatch, specification, lifecycle, session) != encoding)
      corrupt("encoding is not canonical");
    return session;
  }
  catch (const operation_codec_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    corrupt(std::string("cannot admit retained session: ") + problem.what());
  }
}

} // namespace pkgctl
