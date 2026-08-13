#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_run_native_construction_test.sh"
fixture="$srcdir/tests/fixtures/native_runtime_root_fixture.cpp"
dep="$srcdir/tests/fixtures/collections/native-construction/dep/recipe.yml"
tool="$srcdir/tests/fixtures/collections/native-construction/tool/recipe.yml"

for path in "$test_source" "$fixture" "$dep" "$tool"; do
  [ -s "$path" ] || {
    echo "native construction qualification source is absent: $path" >&2
    exit 1
  }
done

for required in \
  "'cli-run-native-construction'" \
  "'integration/cli_run_native_construction_test.sh'" \
  "'fixtures/native_runtime_root_fixture.cpp'" \
  "'tests/fixtures/collections/native-construction'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "native construction Meson qualification omits: $required" >&2
    exit 1
  }
done

grep -F -- '"$runtime_root_fixture" "$build" /bin/sh' "$test_source" >/dev/null || {
  echo 'native construction does not realize a real shell runtime' >&2
  exit 1
}
if grep -F -- 'native-test-interpreter' "$test_source" >/dev/null; then
  echo 'native construction regressed to the synthetic interpreter fixture' >&2
  exit 1
fi

for required in \
  "--goal 'check=tool'" \
  "'durable-steps 3'" \
  "package archives, expected 2" \
  "dependency-payload dependency-source" \
  "tool-payload tool-source+dependency-source" \
  "check-payload checked:tool-source+dependency-source"; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "native construction process proof omits: $required" >&2
    exit 1
  }
done

for required in \
  'path: files/source.txt' \
  'sha256: 1d70be42fce0076cb450831f76ab01c73d9c2c136847646d8e43a6e48700d978' \
  '$PKG_SOURCE_ROOT/source.txt' \
  '$PKG_DESTDIR/dep-token'; do
  grep -F -- "$required" "$dep" >/dev/null || {
    echo "native dependency recipe omits: $required" >&2
    exit 1
  }
done

for required in \
  'build:' \
  '- package: dep' \
  'path: files/source.txt' \
  'sha256: 42d26b2e82cf8ed42651ab63ec29927658d2e15f91c72d3ffd72a3755eb1f66f' \
  '$PKG_BUILD_INPUT_ROOT/dep/dep-token' \
  '$ZEPPE_LIN_CHECK_SOURCE/source.txt' \
  '$ZEPPE_LIN_CHECK_ROOT/tool-token' \
  '/tmp/check-ran'; do
  grep -F -- "$required" "$tool" >/dev/null || {
    echo "native dependent/check recipe omits: $required" >&2
    exit 1
  }
done

for required in \
  'program_interpreter' \
  'copy_runtime' \
  'ldd ' \
  'copy_one(root, interpreter, interpreter)' \
  'std::cout << interpreter.string()'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "native runtime-root fixture omits: $required" >&2
    exit 1
  }
done
