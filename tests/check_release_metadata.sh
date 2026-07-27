#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
version=0.2.0

require_line()
{
  file=$1
  line=$2
  grep -F -x "$line" "$file" >/dev/null || {
    echo "missing release metadata in $file: $line" >&2
    exit 1
  }
}

require_line "$srcdir/meson.build" "  version: '$version',"
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_major = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_minor = 2;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_patch = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr const char* version_string = "0.2.0";'

grep -F '## 0.2.0 - 2026-07-27' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Release 0.2.0' "$srcdir/README.md" >/dev/null
grep -F 'Version 0.2.0' "$srcdir/man/pkgctl.1.scd" >/dev/null

for contract in \
  'libpkgsource >= 1.1.0' \
  'libpkgcatalog >= 1.1.0' \
  'libpkgstate >= 2.1.0' \
  'libpkgresolve >= 1.0.0' \
  'libpkgtransaction >= 1.0.0'; do
  grep -F "$contract" "$srcdir/CHANGELOG.md" >/dev/null || {
    echo "missing dependency floor in changelog: $contract" >&2
    exit 1
  }
done

[ "$(sed -n 's/^## \([0-9][0-9.]*\) - .*/\1/p' \
    "$srcdir/CHANGELOG.md" | head -n 1)" = "$version" ]
