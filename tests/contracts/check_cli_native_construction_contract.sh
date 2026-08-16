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

for required in \
  'command -v chmod' \
  '"$runtime_root_fixture" "$build" /bin/sh "$chmod_program"'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "native construction does not realize required runtime tool: $required" >&2
    exit 1
  }
done
if grep -F -- 'native-test-interpreter' "$test_source" >/dev/null; then
  echo 'native construction regressed to the synthetic interpreter fixture' >&2
  exit 1
fi

for required in \
  "--goal 'build=tool'" \
  "--goal 'check=tool'" \
  '--build-parallelism 3' \
  '--build-source-date-epoch 123456789' \
  "'durable-steps 1'" \
  "'durable-steps 2'" \
  'constructions 1' \
  'checks 0' \
  'policy-parallelism-redeclaration' \
  'policy-epoch-redeclaration' \
  'policy redeclaration changed durable run/evidence history' \
  "package archives, expected 2" \
  'build-policy' \
  "dependency-payload dependency-source" \
  "tool-payload tool-source+dependency-source" \
  '"$run_evidence_inspect_fixture"' \
  'construction-evidence 2' \
  'check-evidence 1' \
  'terminal cleanup retained private realization under $directory'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "native construction process proof omits: $required" >&2
    exit 1
  }
done

for required in \
  'path: files/source.txt' \
  'sha256: 1d70be42fce0076cb450831f76ab01c73d9c2c136847646d8e43a6e48700d978' \
  '$PKG_SOURCE_ROOT/source.txt' \
  '$PKG_DESTDIR/dep-token' \
  '$PKG_DESTDIR/dep-tool' \
  'chmod 0555 "$PKG_DESTDIR/dep-tool"' \
  '[ "$PKG_JOBS" = 3 ]' \
  '[ "$SOURCE_DATE_EPOCH" = 123456789 ]' \
  '[ "$(umask)" = 0022 ]'; do
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
  '$PKG_BUILD_INPUT_ROOT/dep/dep-tool' \
  '$PKG_SOURCE_ROOT/source.txt' \
  '$PKG_PACKAGE_ROOT/tool-token' \
  '$PKG_DESTDIR/build-policy' \
  'parallelism=%s' \
  'source-date-epoch=%s' \
  'umask=%s' \
  '[ "$PKG_JOBS" = 3 ]' \
  '[ "$SOURCE_DATE_EPOCH" = 123456789 ]' \
  '[ "$(umask)" = 0022 ]' \
  '/tmp/check-ran'; do
  grep -F -- "$required" "$tool" >/dev/null || {
    echo "native dependent/check recipe omits: $required" >&2
    exit 1
  }
done

for required in \
  'program_interpreter' \
  'copy_runtime' \
  'copy_requested_runtime' \
  '[EXECUTABLE ...]' \
  'ldd ' \
  'copy_one(root, interpreter, interpreter)' \
  'std::cout << interpreter.string()'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "native runtime-root fixture omits: $required" >&2
    exit 1
  }
done
