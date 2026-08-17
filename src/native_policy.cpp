// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/identity.h>
#include <pkgctl/native_policy.h>

#include <array>
#include <string>
#include <utility>

namespace pkgctl {
namespace {

constexpr std::array<std::uint8_t, 8> encoding_magic{
    'P', 'C', 'P', 'O', 'L', '1', 0, 0};
constexpr std::size_t retained_identity_size = 74U;
constexpr std::size_t checksum_size = 64U;
constexpr std::string_view policy_identity_domain =
    "pkgctl/native-operation-policy/1";
constexpr std::string_view encoding_checksum_domain =
    "pkgctl/native-operation-policy-encoding-checksum/1";

struct native_policy_definition final {
  native_operation_policy_profile profile;
  std::string_view versioned_name;
  std::string_view semantic_body;
  pkgplan::shared_ownership_policy sharing;
};

[[noreturn]] void invalid_profile(const std::string& message)
{
  throw native_operation_policy_error(
      native_operation_policy_error_code::invalid_profile, message);
}

[[noreturn]] void corrupt(const std::string& message)
{
  throw native_operation_policy_error(
      native_operation_policy_error_code::corrupt_encoding, message);
}

[[nodiscard]] native_policy_definition definition(
    native_operation_policy_profile profile)
{
  switch (profile)
  {
    case native_operation_policy_profile::strict_exclusive:
      return {
          profile, "strict-exclusive/v1",
          "incoming=activate;obsolete=remove;shared=forbid;"
          "directory-cleanup=remove-if-empty;overrides=none",
          pkgplan::shared_ownership_policy::forbid};
    case native_operation_policy_profile::exact_compatible_sharing:
      return {
          profile, "exact-compatible-sharing/v1",
          "incoming=activate;obsolete=remove;shared=allow-compatible;"
          "directory-cleanup=remove-if-empty;overrides=none",
          pkgplan::shared_ownership_policy::allow_compatible};
  }
  invalid_profile("native operation policy profile is unsupported");
}

[[nodiscard]] pkgplan::policy_snapshot_identity policy_identity(
    const native_policy_definition& value)
{
  return pkgplan::policy_snapshot_identity::parse(
      "v1:sha256:" +
      make_session_identity(
          policy_identity_domain,
          {std::string(value.versioned_name), std::string(value.semantic_body)})
          .hex());
}

void append_u16(native_operation_policy_encoding& output, std::uint16_t value)
{
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

[[nodiscard]] std::uint16_t read_u16(
    const native_operation_policy_encoding& input, std::size_t& offset)
{
  if (offset > input.size() || input.size() - offset < 2U)
    corrupt("native operation policy encoding is truncated");
  const auto value = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(input[offset]) << 8U) |
      static_cast<std::uint16_t>(input[offset + 1U]));
  offset += 2U;
  return value;
}

[[nodiscard]] std::string bytes_as_string(
    const native_operation_policy_encoding& input,
    std::size_t begin,
    std::size_t size)
{
  if (begin > input.size() || size > input.size() - begin)
    corrupt("native operation policy encoding is truncated");
  return std::string(
      reinterpret_cast<const char*>(input.data() + begin), size);
}

[[nodiscard]] std::string checksum(
    const native_operation_policy_encoding& input, std::size_t size)
{
  return make_session_identity(
      encoding_checksum_domain,
      {bytes_as_string(input, 0U, size)}).hex();
}

[[nodiscard]] native_operation_policy_profile profile_from_tag(
    std::uint8_t value)
{
  switch (value)
  {
    case 1U: return native_operation_policy_profile::strict_exclusive;
    case 2U:
      return native_operation_policy_profile::exact_compatible_sharing;
    default:
      corrupt("native operation policy encoding contains unknown profile");
  }
}

} // namespace

std::string_view native_operation_policy_profile_name(
    native_operation_policy_profile profile) noexcept
{
  switch (profile)
  {
    case native_operation_policy_profile::strict_exclusive:
      return "strict-exclusive";
    case native_operation_policy_profile::exact_compatible_sharing:
      return "exact-compatible-sharing";
  }
  return "invalid";
}

std::optional<native_operation_policy_profile>
native_operation_policy_profile_from_name(std::string_view name) noexcept
{
  if (name == "strict-exclusive")
    return native_operation_policy_profile::strict_exclusive;
  if (name == "exact-compatible-sharing")
    return native_operation_policy_profile::exact_compatible_sharing;
  return std::nullopt;
}

native_operation_policy::native_operation_policy(
    native_operation_policy_profile profile,
    pkgplan::package_policy_snapshot snapshot)
    : profile_(profile), snapshot_(std::move(snapshot))
{
}

native_operation_policy native_operation_policy::seal(
    native_operation_policy_profile profile)
{
  const auto value = definition(profile);
  pkgplan::normalized_path_policy defaults(
      pkgplan::incoming_path_policy::activate(),
      pkgplan::obsolete_path_policy::remove(), value.sharing,
      pkgplan::directory_cleanup_policy::remove_if_empty);
  return native_operation_policy(
      profile,
      pkgplan::package_policy_snapshot(
          policy_identity(value), std::move(defaults), {}));
}

native_operation_policy_profile native_operation_policy::profile() const noexcept
{
  return profile_;
}

const pkgplan::package_policy_snapshot&
native_operation_policy::snapshot() const noexcept
{
  return snapshot_;
}

native_operation_policy_error::native_operation_policy_error(
    native_operation_policy_error_code code, std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

native_operation_policy_error_code
native_operation_policy_error::code() const noexcept
{
  return code_;
}

native_operation_policy_encoding encode_native_operation_policy(
    const native_operation_policy& policy)
{
  native_operation_policy_encoding output;
  output.reserve(maximum_native_operation_policy_encoding_size);
  output.insert(output.end(), encoding_magic.begin(), encoding_magic.end());
  append_u16(output, native_operation_policy_encoding_version);
  output.push_back(static_cast<std::uint8_t>(policy.profile()));
  const auto identity = policy.snapshot().identity().string();
  if (identity.size() != retained_identity_size)
    corrupt("native operation policy identity has unexpected size");
  output.insert(output.end(), identity.begin(), identity.end());
  const auto digest = checksum(output, output.size());
  output.insert(output.end(), digest.begin(), digest.end());
  if (output.size() > maximum_native_operation_policy_encoding_size)
    corrupt("native operation policy encoding exceeds maximum size");
  return output;
}

native_operation_policy decode_native_operation_policy(
    const native_operation_policy_encoding& encoding)
{
  if (encoding.size() > maximum_native_operation_policy_encoding_size)
    corrupt("native operation policy encoding exceeds maximum size");
  const auto minimum_size = encoding_magic.size() + 2U + 1U +
      retained_identity_size + checksum_size;
  if (encoding.size() != minimum_size)
    corrupt("native operation policy encoding has invalid size");

  const auto payload_size = encoding.size() - checksum_size;
  const auto retained_checksum =
      bytes_as_string(encoding, payload_size, checksum_size);
  if (retained_checksum != checksum(encoding, payload_size))
    corrupt("native operation policy encoding checksum mismatch");

  std::size_t offset = 0U;
  for (const auto expected : encoding_magic)
  {
    if (encoding[offset++] != expected)
      corrupt("native operation policy encoding has invalid magic");
  }
  const auto version = read_u16(encoding, offset);
  if (version != native_operation_policy_encoding_version)
  {
    throw native_operation_policy_error(
        native_operation_policy_error_code::unsupported_encoding,
        "native operation policy encoding version is unsupported");
  }
  const auto profile = profile_from_tag(encoding[offset++]);
  const auto retained_identity = pkgplan::policy_snapshot_identity::parse(
      bytes_as_string(encoding, offset, retained_identity_size));
  offset += retained_identity_size;
  if (offset != payload_size)
    corrupt("native operation policy encoding contains trailing payload bytes");

  auto decoded = native_operation_policy::seal(profile);
  if (decoded.snapshot().identity() != retained_identity)
    corrupt("native operation policy identity contradicts retained profile");
  if (encode_native_operation_policy(decoded) != encoding)
    corrupt("native operation policy encoding is not canonical");
  return decoded;
}

} // namespace pkgctl
