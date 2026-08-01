#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-direct.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

modules='libpkgsource libpkgsource-yaml libpkgsource-plan libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-plan libpkgstate-apply libpkgfetch libpkgbuild libpkgbuild-exec libpkgbuild-plan libpkgimage libpkgplan libpkgexec libpkgapply libpkgapply-posix libpkgapply-exec libpkgresolve libpkgtransaction libpkgcheck libpkgcheck-exec libcrypto'
cflags=$(pkg-config --cflags $modules)
libs=$(pkg-config --libs $modules)
flags="-std=c++17 -Wall -Wextra -Wpedantic -Werror -I$srcdir/include $cflags"
objects=
for source in "$srcdir"/src/*.cpp; do
  object="$tmp/$(basename "$source").o"
  # shellcheck disable=SC2086
  "$cxx" $flags -c "$source" -o "$object"
  objects="$objects $object"
done

for test_source in check_test construction_test dispatch_test run_journal_test request_test session_test effect_journal_test effect_inspect_test effect_test report_test version_test; do
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/$test_source.cpp" $objects $libs \
    -o "$tmp/$test_source"
  "$tmp/$test_source"
done

# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/state_fixture.cpp" $objects $libs \
  -o "$tmp/state-fixture"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/run_store_fixture.cpp" $objects $libs \
  -o "$tmp/run-store-fixture"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/effect_store_fixture.cpp" $objects $libs \
  -o "$tmp/effect-store-fixture"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/cli/main.cpp" "$srcdir/cli/options.cpp" \
  $objects $libs -o "$tmp/pkgctl"
version=$(sed -n 's/^inline constexpr const char\* version_string = "\([^"]*\)";$/\1/p' \
  "$srcdir/include/pkgctl/version.h")
[ -n "$version" ] || {
  echo 'cannot determine pkgctl version from version.h' >&2
  exit 1
}
"$srcdir/tests/cli_test.sh" "$tmp/pkgctl" "$tmp/state-fixture" \
  "$tmp/run-store-fixture" "$tmp/effect-store-fixture" "$version"

for header in "$srcdir"/include/pkgctl/*.h; do
  base=$(basename "$header")
  cat >"$tmp/header.cpp" <<EOF_INNER
#include <pkgctl/$base>
int main() { return 0; }
EOF_INNER
  # shellcheck disable=SC2086
  "$cxx" $flags -fsyntax-only "$tmp/header.cpp"
done

"$srcdir/tests/check_construction_contract.sh" "$srcdir"
"$srcdir/tests/check_preparation_contract.sh" "$srcdir"
"$srcdir/tests/check_progression_contract.sh" "$srcdir"
"$srcdir/tests/check_check_contract.sh" "$srcdir"
"$srcdir/tests/check_dispatch_contract.sh" "$srcdir"
"$srcdir/tests/check_run_journal_contract.sh" "$srcdir"
"$srcdir/tests/check_run_execute_contract.sh" "$srcdir"
"$srcdir/tests/check_run_reconcile_contract.sh" "$srcdir"
"$srcdir/tests/check_run_admit_contract.sh" "$srcdir"
"$srcdir/tests/check_run_authority_contract.sh" "$srcdir"
"$srcdir/tests/check_run_advance_contract.sh" "$srcdir"
"$srcdir/tests/check_run_driver_source_contract.sh" "$srcdir"
"$srcdir/tests/check_run_native_contract.sh" "$srcdir"
"$srcdir/tests/check_run_runtime_contract.sh" "$srcdir"
"$srcdir/tests/check_run_nonce_contract.sh" "$srcdir"
"$srcdir/tests/check_run_drive_contract.sh" "$srcdir"
"$srcdir/tests/check_run_launch_contract.sh" "$srcdir"
"$srcdir/tests/check_effect_inspect_contract.sh" "$srcdir"
"$srcdir/tests/check_effect_inspect_cli_contract.sh" "$srcdir"
"$srcdir/tests/check_run_inspect_contract.sh" "$srcdir"
"$srcdir/tests/check_run_inspect_cli_contract.sh" "$srcdir"
"$srcdir/tests/check_source_contract.sh" "$srcdir"
"$srcdir/tests/check_effect_contract.sh" "$srcdir"
"$srcdir/tests/check_restart_contract.sh" "$srcdir"
"$srcdir/tests/check_manual_contract.sh" "$srcdir"
"$srcdir/tests/check_release_metadata.sh" "$srcdir"
