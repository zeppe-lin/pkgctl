// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/native_policy.h>

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)
#define EXPECT_POLICY_ERROR(code_value, expression) do { bool caught = false; try { (void)(expression); } catch (const pkgctl::native_operation_policy_error& value) { caught = value.code() == (code_value); } CHECK(caught); } while (false)

void check_profile(
    pkgctl::native_operation_policy_profile profile,
    pkgplan::shared_ownership_policy sharing,
    const char* expected_identity)
{
  const auto policy = pkgctl::native_operation_policy::seal(profile);
  const auto& snapshot = policy.snapshot();
  CHECK(policy.profile() == profile);
  CHECK(snapshot.defaults().incoming() == pkgplan::incoming_path_policy::activate());
  CHECK(snapshot.defaults().obsolete() == pkgplan::obsolete_path_policy::remove());
  CHECK(snapshot.defaults().shared() == sharing);
  CHECK(snapshot.defaults().directory_cleanup() ==
        pkgplan::directory_cleanup_policy::remove_if_empty);
  CHECK(snapshot.overrides().empty());
  CHECK(snapshot.identity().string() == expected_identity);

  const auto encoded = pkgctl::encode_native_operation_policy(policy);
  const auto decoded = pkgctl::decode_native_operation_policy(encoded);
  CHECK(decoded.profile() == profile);
  CHECK(decoded.snapshot().identity() == snapshot.identity());
  CHECK(decoded.snapshot().defaults() == snapshot.defaults());
  CHECK(decoded.snapshot().overrides().empty());
  CHECK(pkgctl::encode_native_operation_policy(decoded) == encoded);
}

} // namespace

int main()
{
  using pkgctl::native_operation_policy_profile;

  CHECK(pkgctl::native_operation_policy_profile_name(
            native_operation_policy_profile::strict_exclusive) ==
        "strict-exclusive");
  CHECK(pkgctl::native_operation_policy_profile_name(
            native_operation_policy_profile::exact_compatible_sharing) ==
        "exact-compatible-sharing");
  CHECK(pkgctl::native_operation_policy_profile_from_name("strict-exclusive") ==
        native_operation_policy_profile::strict_exclusive);
  CHECK(pkgctl::native_operation_policy_profile_from_name(
            "exact-compatible-sharing") ==
        native_operation_policy_profile::exact_compatible_sharing);
  CHECK(!pkgctl::native_operation_policy_profile_from_name("forbid"));

  check_profile(
      native_operation_policy_profile::strict_exclusive,
      pkgplan::shared_ownership_policy::forbid,
      "v1:sha256:5e841191ed89be9b1c52e50a84776200b964f1c97b1120314ff69a183716aa0c");
  check_profile(
      native_operation_policy_profile::exact_compatible_sharing,
      pkgplan::shared_ownership_policy::allow_compatible,
      "v1:sha256:b99b513f1fd747200c7a882a748e019e42f2504f1b076b7660aeb6967c0767ca");

  const auto strict = pkgctl::native_operation_policy::seal(
      native_operation_policy_profile::strict_exclusive);
  const auto sharing = pkgctl::native_operation_policy::seal(
      native_operation_policy_profile::exact_compatible_sharing);
  CHECK(strict.snapshot().identity() != sharing.snapshot().identity());

  EXPECT_POLICY_ERROR(
      pkgctl::native_operation_policy_error_code::invalid_profile,
      pkgctl::native_operation_policy::seal(
          static_cast<native_operation_policy_profile>(99U)));

  const auto encoded = pkgctl::encode_native_operation_policy(strict);
  auto corrupted = encoded;
  corrupted.front() ^= 0x01U;
  EXPECT_POLICY_ERROR(pkgctl::native_operation_policy_error_code::corrupt_encoding,
                      pkgctl::decode_native_operation_policy(corrupted));

  corrupted = encoded;
  corrupted[8] = 0U;
  corrupted[9] = 2U;
  EXPECT_POLICY_ERROR(pkgctl::native_operation_policy_error_code::corrupt_encoding,
                      pkgctl::decode_native_operation_policy(corrupted));

  corrupted = encoded;
  corrupted.pop_back();
  EXPECT_POLICY_ERROR(pkgctl::native_operation_policy_error_code::corrupt_encoding,
                      pkgctl::decode_native_operation_policy(corrupted));

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
