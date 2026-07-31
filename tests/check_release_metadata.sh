#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
version=0.20.0

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
  'inline constexpr unsigned version_minor = 20;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_patch = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr const char* version_string = "0.20.0";'
require_line "$srcdir/src/core.cpp" \
  'static_assert(pkgctl::version_minor == 20);'

grep -F '## 0.20.0 - 2026-08-01' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Release 0.20.0' "$srcdir/README.md" >/dev/null
grep -F 'Version 0.20.0' "$srcdir/man/pkgctl.1.scd" >/dev/null

grep -F 'inspect_effect_attempt()' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'read-only inspection of one exact durable' \
  "$srcdir/README.md" >/dev/null
grep -F 'Version 0.20.0 can inspect one exact caller-selected durable effect attempt' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'shared read-only descriptor instead of creating or opening the writer lock' \
  "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Adds no CLI' "$srcdir/CHANGELOG.md" >/dev/null

for contract in \
  'libpkgsource >= 2.0.0' \
  'libpkgsource-yaml >= 2.0.0' \
  'libpkgsource-plan >= 2.0.0' \
  'libpkgcatalog >= 2.0.0' \
  'libpkgcatalog-acquire >= 2.0.0' \
  'libpkgstate >= 2.3.0' \
  'libpkgstate-plan >= 2.3.0' \
  'libpkgstate-apply >= 2.3.0' \
  'libpkgfetch >= 1.0.0' \
  'libpkgbuild >= 2.0.0' \
  'libpkgbuild-exec >= 1.0.0' \
  'libpkgbuild-plan >= 2.0.0' \
  'libpkgimage >= 0.3.0' \
  'libpkgplan >= 0.2.0' \
  'libpkgexec >= 1.3.0' \
  'libpkgapply >= 2.0.0' \
  'libpkgapply-exec >= 1.0.0' \
  'libpkgresolve >= 2.0.0' \
  'libpkgtransaction >= 2.1.0' \
  'libpkgcheck >= 0.1.0' \
  'libpkgcheck-exec >= 0.1.1'; do
  grep -F "$contract" "$srcdir/CHANGELOG.md" >/dev/null || {
    echo "missing dependency floor in changelog: $contract" >&2
    exit 1
  }
done

[ "$(sed -n 's/^## \([0-9][0-9.]*\) - .*/\1/p' \
    "$srcdir/CHANGELOG.md" | head -n 1)" = "$version" ]
