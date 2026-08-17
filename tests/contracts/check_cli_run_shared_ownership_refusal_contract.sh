#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
test_source=$root/tests/integration/cli_run_shared_ownership_refusal_test.sh
hostile_fixture=$root/tests/fixtures/shared_ownership_hostile_image_fixture.cpp
hostile_recipe=$root/tests/fixtures/collections/shared-ownership-hostile-image/runtime-lib-hostile/recipe.yml
hostile_collection=$root/tests/fixtures/collections/shared-ownership-hostile-image
meson=$root/tests/meson.build
operation_source=$root/src/run_operation.cpp

fail()
{
  echo "shared-ownership refusal contract: $*" >&2
  exit 1
}

for file in "$test_source" "$hostile_fixture" "$hostile_recipe" "$meson" \
            "$operation_source"; do
  [ -s "$file" ] || fail "qualification source is absent: $file"
done
[ ! -e "$hostile_collection/profiles.yml" ] || \
  fail 'hostile image collection must not define operation policy'
recipe_count=$(find "$hostile_collection" -name recipe.yml -type f | wc -l | tr -d ' ')
[ "$recipe_count" -eq 1 ] || \
  fail "hostile image collection must contain exactly one recipe, got $recipe_count"

for required in \
  'name: runtime-lib-hostile' \
  'runtime-lib-hostile-source' \
  'mkdir -p "$PKG_DESTDIR/usr/lib" || exit 1' \
  'hostile-authority >"$PKG_DESTDIR/usr/lib/shared-ownership-marker" || exit 1'; do
  grep -F -- "$required" "$hostile_recipe" >/dev/null || \
    fail "hostile recipe authority omits: $required"
done

for required in \
  '<libpkgimage/libarchive_backend.h>' \
  'expected_archive_digest' \
  'libarchive_backend().open(request)' \
  'package_path::parse(marker_path)' \
  'marker->mode != 0644U' \
  'marker->regular_content->string() != marker_digest' \
  'entry_selection::from_ids' \
  'archive->replay(selection, sink)' \
  'sink.payload() != marker_payload' \
  'hostile-authority\\n' \
  '724f65e1fb4870e360aeea5c62e71c41fa94578e90dce25ed64a425096fbf9cb'; do
  grep -F -- "$required" "$hostile_fixture" >/dev/null || \
    fail "hostile owner-side image proof omits: $required"
done

for required in \
  'setup_case strict' \
  'run_package_success base "$compatible_collection" base-files strict-exclusive 61' \
  'run_package_refusal forbidden "$compatible_collection" runtime-lib strict-exclusive 62' \
  'setup_case incompatible' \
  'build_qualified_image compatible-build "$compatible_collection" runtime-lib 60' \
  'build_qualified_image hostile-build "$hostile_collection" runtime-lib-hostile 71' \
  'run_package_success base "$compatible_collection" base-files strict-exclusive 72' \
  'run_package_refusal incompatible "$hostile_collection" runtime-lib-hostile' \
  'exact-compatible-sharing 73' \
  'native operation planning refused' \
  "'with code '" \
  'fail "$package image authority is not qualified"' \
  'v1:sha256:724f65e1fb4870e360aeea5c62e71c41fa94578e90dce25ed64a425096fbf9cb' \
  'content $expected_content' \
  "stat -c 'meta %n|%F|%f|%u|%g|%s|%y|%z'" \
  'sha256sum' \
  'cmp -s "$case_root/target-before.out" "$case_root/target-after.out"' \
  'selected installed package is absent' \
  'published ownership for refused package' \
  'mutated the target before refusal' \
  'activated rejected package payload' \
  'packages 1' \
  'findings 0' \
  'failures 0'; do
  grep -F -- "$required" "$test_source" >/dev/null || \
    fail "refusal process proof omits: $required"
done

# The CLI may state the controller-level outcome, but refusal-code vocabulary is
# owned by libpkgplan. Numeric ordinals or locally named mappings are forbidden.
grep -F -- '"native operation planning refused"' "$operation_source" >/dev/null || \
  fail 'operation authority lacks generic planning-refusal diagnostic'
for forbidden in \
  'prepared.refusal()->code()' \
  'planning_refusal_name' \
  'with code '; do
  if grep -F -- "$forbidden" "$operation_source" >/dev/null 2>&1; then
    fail "pkgctl renders foreign refusal vocabulary: $forbidden"
  fi
done

# This layer correlates owner authority with hostile physical non-mutation. It
# must not decode pkgctl/private application, session, publication, or tar bytes.
for forbidden in \
  'tar -t' \
  'tar -x' \
  'operation-session-' \
  'publication-request-' \
  'publication-receipt-' \
  'application-*.bin' \
  'completed-*.bin' \
  'effect-bodies/'; do
  if grep -F -- "$forbidden" "$test_source" >/dev/null 2>&1; then
    fail "refusal proof decodes private transport instead of owner authority: $forbidden"
  fi
done

for required in \
  "'shared-ownership-image-fixture'" \
  "'shared-ownership-hostile-image-fixture'" \
  "'cli-run-shared-ownership-refusal'" \
  "'integration/cli_run_shared_ownership_refusal_test.sh'" \
  "'tests/fixtures/collections/shared-ownership-image'" \
  "'tests/fixtures/collections/shared-ownership-hostile-image'" \
  "suite: 'integration-privileged'" \
  "'contracts/check_cli_run_shared_ownership_refusal_contract.sh'"; do
  grep -F -- "$required" "$meson" >/dev/null || \
    fail "refusal Meson qualification omits: $required"
done
