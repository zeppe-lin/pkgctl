#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_build_archive_source_test.sh"
recipe="$srcdir/tests/fixtures/collections/archive-source-check/archive-probe/recipe.yml"
archive="$srcdir/tests/fixtures/collections/archive-source-check/archive-probe/files/archive-probe.tar"

for path in "$meson" "$test_source" "$recipe" "$archive"; do
  [ -s "$path" ] || {
    echo "archive-source qualification source is absent: $path" >&2
    exit 1
  }
done

expected_archive=fe950834e818090a0ad63e431bd0157215bc33a2e9e44ef8f8517292abf84ce9
actual_archive=$(sha256sum "$archive" | awk '{print $1}')
[ "$actual_archive" = "$expected_archive" ] || {
  echo "archive-source fixture digest changed: $actual_archive" >&2
  exit 1
}
[ "$(tar -xOf "$archive" tree/source.txt)" = archive-source ] || {
  echo 'archive-source fixture payload changed' >&2
  exit 1
}

for required in \
  'name: archive-probe.tar' \
  'unpack: archive' \
  'sha256: fe950834e818090a0ad63e431bd0157215bc33a2e9e44ef8f8517292abf84ce9' \
  '$PKG_SOURCE_ROOT/archive-probe.tar' \
  '! -e "$PKG_SOURCE_ROOT/tree/source.txt"' \
  'read -r source <tree/source.txt' \
  '$PKG_DESTDIR/archive-result' \
  '$PKG_PACKAGE_ROOT/archive-result' \
  '/tmp/archive-check-ran'; do
  grep -F -- "$required" "$recipe" >/dev/null || {
    echo "archive-source recipe omits: $required" >&2
    exit 1
  }
done

for required in \
  'build archive-probe --check' \
  '--max-steps 2' \
  'durable-steps 2' \
  'artifacts 2' \
  '.package archive-dep' \
  '.package archive-probe' \
  'rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"' \
  'dep-token' \
  'archive-payload archive-source+archive-dependency' \
  'check-payload checked:archive-source+archive-dependency'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "archive-source process proof omits: $required" >&2
    exit 1
  }
done

for required in \
  "'cli-build-archive-source'" \
  "'integration/cli_build_archive_source_test.sh'" \
  "'tests/fixtures/collections/archive-source-check'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "archive-source Meson qualification omits: $required" >&2
    exit 1
  }
done
