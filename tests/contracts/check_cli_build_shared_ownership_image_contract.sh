#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
fixture=$root/tests/fixtures/shared_ownership_image_fixture.cpp
test_source=$root/tests/integration/cli_build_shared_ownership_image_test.sh
meson=$root/tests/meson.build
base=$root/tests/fixtures/collections/shared-ownership-image/base-files/recipe.yml
runtime=$root/tests/fixtures/collections/shared-ownership-image/runtime-lib/recipe.yml
collection=$root/tests/fixtures/collections/shared-ownership-image

for file in "$fixture" "$test_source" "$meson" "$base" "$runtime"; do
  [ -s "$file" ] || {
    echo "shared ownership image qualification source is absent: $file" >&2
    exit 1
  }
done

[ ! -e "$collection/profiles.yml" ] || {
  echo 'build/image fixture collection must not define operation profiles' >&2
  exit 1
}
recipe_count=$(find "$collection" -name recipe.yml -type f | wc -l | tr -d ' ')
[ "$recipe_count" -eq 2 ] || {
  echo "build/image fixture collection must contain exactly two recipes, got $recipe_count" >&2
  exit 1
}

for recipe in "$base" "$runtime"; do
  for required in \
    'mkdir -p "$PKG_DESTDIR/usr/lib" || exit 1' \
    'shared-authority >"$PKG_DESTDIR/usr/lib/shared-ownership-marker" || exit 1'; do
    grep -F -- "$required" "$recipe" >/dev/null || {
      echo "shared ownership recipe authority omits: $required: $recipe" >&2
      exit 1
    }
  done
done

for required in \
  '<libpkgimage/libarchive_backend.h>' \
  'archive_inspection_request' \
  'expected_archive_digest' \
  'libarchive_backend().open(request)' \
  'package_path::parse(marker_path)' \
  'entry_type::regular' \
  'marker->mode != 0644U' \
  'marker->regular_content->string() != marker_digest' \
  'entry_selection::from_ids' \
  'archive->replay(selection, sink)' \
  'sink.payload() != marker_payload'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "owner-side shared image proof omits: $required" >&2
    exit 1
  }
done

for forbidden in \
  'tar -t' \
  'tar -x' \
  'target-root' \
  'rootfs-audit' \
  'set -- run' \
  '--goal' \
  '--converge-exact' \
  '--operation-policy'; do
  if grep -F -- "$forbidden" "$test_source" >/dev/null 2>&1; then
    echo "build/image layer improperly depends on downstream authority: $forbidden" >&2
    exit 1
  fi
done

for required in \
  'build_one base-files' \
  'build_one runtime-lib' \
  'frontend build' \
  'artifacts 1' \
  'artifact.$index.image-identity' \
  'retained build-image authority is absent' \
  'mkdir_program=$(command -v mkdir)' \
  '"$runtime_root_fixture" "$build" /bin/sh "$mkdir_program"' \
  '"$image_fixture" "$artifact" "$sha256"' \
  'path usr/lib/shared-ownership-marker' \
  'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06' \
  'require_equal shared-marker-semantics' \
  'require_equal "$package canonical-state" "$initial_state" "$final_state"'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "shared build/image process proof omits: $required" >&2
    exit 1
  }
done

for required in \
  "'shared-ownership-image-fixture'" \
  "'cli-build-shared-ownership-image'" \
  "'integration/cli_build_shared_ownership_image_test.sh'" \
  "'tests/fixtures/collections/shared-ownership-image'" \
  "suite: 'integration-privileged'" \
  "'contracts/check_cli_build_shared_ownership_image_contract.sh'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "shared build/image Meson qualification omits: $required" >&2
    exit 1
  }
done
