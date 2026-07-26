#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-headers.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for header in "$srcdir"/include/pkgctl/*.h; do
  base=$(basename "$header")
  cat >"$tmp/test.cpp" <<EOF_INNER
#include <pkgctl/$base>
int main() { return 0; }
EOF_INNER
  "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
    -I"$srcdir/include" -fsyntax-only "$tmp/test.cpp"
done
