// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file native_policy.h
 *  \brief Controller-owned native target-operation policy profiles.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <libpkgplan/policy.h>

namespace pkgctl {

/*! \brief Complete native target-operation policy profiles owned by pkgctl. */
enum class native_operation_policy_profile : std::uint8_t {
  strict_exclusive = 1,
  exact_compatible_sharing = 2,
};

/*! \brief Return the stable command vocabulary for one complete profile. */
[[nodiscard]] std::string_view native_operation_policy_profile_name(
    native_operation_policy_profile profile) noexcept;

/*! \brief Resolve one exact stable command profile name. */
[[nodiscard]] std::optional<native_operation_policy_profile>
native_operation_policy_profile_from_name(std::string_view name) noexcept;

/*! \brief One sealed controller-owned policy and its planner projection. */
class native_operation_policy final {
public:
  /*! \brief Seal one complete immutable native policy profile. */
  [[nodiscard]] static native_operation_policy seal(
      native_operation_policy_profile profile);

  [[nodiscard]] native_operation_policy_profile profile() const noexcept;
  [[nodiscard]] const pkgplan::package_policy_snapshot& snapshot() const noexcept;

private:
  native_operation_policy(native_operation_policy_profile profile,
                          pkgplan::package_policy_snapshot snapshot);

  native_operation_policy_profile profile_;
  pkgplan::package_policy_snapshot snapshot_;
};

/*! \brief Current private encoding generation for native policy authority. */
inline constexpr std::uint16_t native_operation_policy_encoding_version = 1;
/*! \brief Hard upper bound for one encoded native policy authority value. */
inline constexpr std::size_t maximum_native_operation_policy_encoding_size = 256U;

using native_operation_policy_encoding = std::vector<std::uint8_t>;

enum class native_operation_policy_error_code : std::uint8_t {
  invalid_profile = 1,
  corrupt_encoding = 2,
  unsupported_encoding = 3,
};

class native_operation_policy_error final : public std::runtime_error {
public:
  native_operation_policy_error(native_operation_policy_error_code code,
                                std::string message);

  [[nodiscard]] native_operation_policy_error_code code() const noexcept;

private:
  native_operation_policy_error_code code_;
};

/*! \brief Encode one controller-owned complete target policy value. */
[[nodiscard]] native_operation_policy_encoding encode_native_operation_policy(
    const native_operation_policy& policy);

/*! \brief Decode, reseal, identity-check, and canonicality-check one policy. */
[[nodiscard]] native_operation_policy decode_native_operation_policy(
    const native_operation_policy_encoding& encoding);

} // namespace pkgctl
