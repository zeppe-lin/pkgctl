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

expected_archive=4fd7f5659897a904b772628cf3de2f03104cf284c45d305138c809639120d2e9
actual_archive=$(sha256sum "$archive" | awk '{print $1}')
[ "$actual_archive" = "$expected_archive" ] || {
  echo "archive-source fixture digest changed: $actual_archive" >&2
  exit 1
}
[ "$(tar -xOf "$archive" tree/source.txt)" = archive-source ] || {
  echo 'archive-source fixture payload changed' >&2
  exit 1
}
[ "$(tar -xOf "$archive" tree/source-tool | tail -n 1)" = "printf '%s\\n' archive-source-tool" ] || {
  echo 'archive-source executable fixture changed' >&2
  exit 1
}

grep -F '  build:' "$recipe" >/dev/null || {
  echo 'archive-source recipe omits build-scoped dependency authority' >&2
  exit 1
}
grep -F '  check:' "$recipe" >/dev/null || {
  echo 'archive-source recipe omits check-scoped dependency authority' >&2
  exit 1
}
dependency_mentions=$(grep -F '    - package: archive-dep' "$recipe" | wc -l | tr -d ' ')
[ "$dependency_mentions" -eq 2 ] || {
  echo "archive-source recipe must declare archive-dep once for build and once for check" >&2
  exit 1
}

for required in \
  'name: archive-probe.tar' \
  'unpack: archive' \
  'sha256: 4fd7f5659897a904b772628cf3de2f03104cf284c45d305138c809639120d2e9' \
  '$PKG_SOURCE_ROOT/archive-probe.tar' \
  '! -e "$PKG_SOURCE_ROOT/tree/source.txt"' \
  'read -r source <tree/source.txt' \
  'source_tool=$(tree/source-tool)' \
  '$PKG_BUILD_INPUT_ROOT/archive-dep/dep-token' \
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
  'archive-payload archive-source+archive-dependency' \
  '"$run_evidence_inspect_fixture"' \
  'construction-evidence 2' \
  'check-evidence 1' \
  'terminal cleanup retained private realization under $directory'; do
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
