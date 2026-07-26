#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-direct.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

flags="-std=c++17 -Wall -Wextra -Wpedantic -Werror -I$srcdir/include"
objects=
for source in "$srcdir"/src/*.cpp; do
  object="$tmp/$(basename "$source").o"
  # shellcheck disable=SC2086
  "$cxx" $flags -c "$source" -o "$object"
  objects="$objects $object"
done

# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/model_test.cpp" $objects -o "$tmp/model-test"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/version_test.cpp" $objects -o "$tmp/version-test"
# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/cli/main.cpp" $objects -o "$tmp/pkgctl"

"$tmp/model-test"
"$tmp/version-test"
"$srcdir/tests/cli_test.sh" "$tmp/pkgctl"
CXX="$cxx" "$srcdir/tests/public_headers.sh" "$srcdir"
"$srcdir/tests/check_source_contract.sh" "$srcdir"
