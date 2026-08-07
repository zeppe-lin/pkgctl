#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-direct.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

core_modules='libcrypto libpkgsource libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-posix libpkgstate-plan libpkgstate-apply libpkgfetch libpkgbuild libpkgbuild-exec libpkgbuild-image libpkgsource-plan libpkgbuild-plan libpkgimage libpkgplan libpkgexec libpkgapply libpkgapply-posix libpkgapply-exec libpkgresolve libpkgtransaction libpkgcheck libpkgcheck-exec'
cli_modules='libpkgsource-yaml libpkgcatalog-codec libpkgexec-linux'
pkg-config --exists \
  'libpkgbuild-exec >= 2.2.0' 'libpkgbuild-exec < 3.0.0' \
  'libpkgcheck-exec >= 0.4.0' 'libpkgcheck-exec < 1.0.0'
core_cflags=$(pkg-config --cflags $core_modules)
core_libs=$(pkg-config --libs $core_modules)
cli_cflags=$(pkg-config --cflags $cli_modules)
cli_libs=$(pkg-config --libs $cli_modules)
flags="-std=c++17 -Wall -Wextra -Wpedantic -Werror -I$srcdir/include -I$srcdir/tests $core_cflags"
objects=
for source in "$srcdir"/src/*.cpp; do
  object="$tmp/$(basename "$source").o"
  # shellcheck disable=SC2086
  "$cxx" $flags -c "$source" -o "$object"
  objects="$objects $object"
done

for test_source in check_test construction_test dispatch_test run_journal_test run_progress_test run_locator_test request_test session_test effect_journal_test effect_inspect_test effect_test report_test version_test; do
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/unit/$test_source.cpp" $objects $core_libs \
    -o "$tmp/$test_source"
  "$tmp/$test_source"
done

for fixture in state_fixture state_inspect_fixture run_store_fixture effect_store_fixture; do
  name=$(printf '%s\n' "${fixture%_fixture}" | tr '_' '-')
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/fixtures/$fixture.cpp" $objects $core_libs \
    -o "$tmp/$name-fixture"
done

"$cxx" -nostdlib -static -Wl,-e,_start \
  "$srcdir/tests/fixtures/native_interpreter_x86_64.S" \
  -o "$tmp/native-test-interpreter"

# shellcheck disable=SC2086
"$cxx" $flags $cli_cflags \
  "$srcdir/cli/main.cpp" "$srcdir/cli/options.cpp" \
  "$srcdir/cli/run_command.cpp" $objects $core_libs $cli_libs -o "$tmp/pkgctl"
version=$(sed -n 's/^inline constexpr const char\* version_string = "\([^"]*\)";$/\1/p' \
  "$srcdir/include/pkgctl/version.h")
[ -n "$version" ] || {
  echo 'cannot determine pkgctl version from version.h' >&2
  exit 1
}
"$srcdir/tests/integration/cli_readonly_test.sh" "$tmp/pkgctl" "$tmp/state-fixture" \
  "$tmp/run-store-fixture" "$tmp/effect-store-fixture" "$version"

"$srcdir/tests/integration/cli_run_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" \
  "$srcdir/tests/fixtures/collections/simple-install"

for header in "$srcdir"/include/pkgctl/*.h; do
  base=$(basename "$header")
  cat >"$tmp/header.cpp" <<EOF_INNER
#include <pkgctl/$base>
int main() { return 0; }
EOF_INNER
  # shellcheck disable=SC2086
  "$cxx" $flags -fsyntax-only "$tmp/header.cpp"
done

for contract in "$srcdir"/tests/contracts/*.sh; do
  "$contract" "$srcdir"
done
