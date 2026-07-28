#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-direct.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

modules='libpkgsource libpkgsource-yaml libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-apply libpkgimage libpkgexec libpkgapply libpkgapply-exec libpkgresolve libpkgtransaction libcrypto'
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

for test_source in request_test session_test effect_test report_test version_test; do
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/$test_source.cpp" $objects $libs \
    -o "$tmp/$test_source"
  "$tmp/$test_source"
done

# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/state_fixture.cpp" $objects $libs \
  -o "$tmp/state-fixture"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/cli/main.cpp" "$srcdir/cli/options.cpp" \
  $objects $libs -o "$tmp/pkgctl"
"$srcdir/tests/cli_test.sh" "$tmp/pkgctl" "$tmp/state-fixture"

for header in "$srcdir"/include/pkgctl/*.h; do
  base=$(basename "$header")
  cat >"$tmp/header.cpp" <<EOF_INNER
#include <pkgctl/$base>
int main() { return 0; }
EOF_INNER
  # shellcheck disable=SC2086
  "$cxx" $flags -fsyntax-only "$tmp/header.cpp"
done

"$srcdir/tests/check_source_contract.sh" "$srcdir"
"$srcdir/tests/check_effect_contract.sh" "$srcdir"
"$srcdir/tests/check_manual_contract.sh" "$srcdir"
"$srcdir/tests/check_release_metadata.sh" "$srcdir"
