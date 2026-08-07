#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
version=0.32.0

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
  'inline constexpr unsigned version_minor = 32;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_patch = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr const char* version_string = "0.32.0";'
require_line "$srcdir/src/core.cpp" \
  'static_assert(pkgctl::version_minor == 32);'

require_dependency_range()
{
  module=$1
  lower=$2
  upper=$3
  grep -A 3 -F "  '$module'," "$srcdir/meson.build" |
    grep -F "  version: ['$lower', '$upper']," >/dev/null || {
      echo "missing Meson dependency range: $module $lower, $upper" >&2
      exit 1
    }
}

require_dependency_range libpkgstate '>=3.1.0' '<4.0.0'
require_dependency_range libpkgbuild-exec '>=2.2.0' '<3.0.0'
require_dependency_range libpkgcheck-exec '>=0.4.0' '<1.0.0'
require_dependency_range libpkgresolve '>=2.0.0' '<3.0.0'
require_dependency_range libpkgplan '>=0.3.0' '<1.0.0'

grep -F '## 0.32.0 - 2026-08-07' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Release 0.32.0' "$srcdir/README.md" >/dev/null
grep -F 'Version 0.32.0' "$srcdir/man/pkgctl.1.scd" >/dev/null

grep -F 'Exact transaction-progress rehydration' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'project_publication_request()' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Reserved, started, released' "$srcdir/CHANGELOG.md" >/dev/null
grep -F 'Version 0.32.0 implements one store-backed' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

for contract in \
  'libpkgsource >= 3.0.0, < 4.0.0' \
  'libpkgsource-yaml >= 1.0.0, < 2.0.0' \
  'libpkgsource-plan >= 1.0.0, < 2.0.0' \
  'libpkgcatalog >= 3.0.0, < 4.0.0' \
  'libpkgcatalog-acquire >= 3.0.0, < 4.0.0' \
  'libpkgstate >= 3.1.0, < 4.0.0' \
  'libpkgstate-posix >= 3.0.0, < 4.0.0' \
  'libpkgstate-plan >= 3.0.0, < 4.0.0' \
  'libpkgstate-apply >= 3.0.0, < 4.0.0' \
  'libpkgfetch >= 1.0.0, < 2.0.0' \
  'libpkgbuild >= 3.0.0, < 4.0.0' \
  'libpkgbuild-exec >= 2.2.0, < 3.0.0' \
  'libpkgbuild-image >= 1.0.0, < 2.0.0' \
  'libpkgbuild-plan >= 1.0.0, < 2.0.0' \
  'libpkgimage >= 0.4.0, < 1.0.0' \
  'libpkgplan >= 0.3.0, < 1.0.0' \
  'libpkgexec >= 1.4.0, < 2.0.0' \
  'libpkgapply >= 3.0.0, < 4.0.0' \
  'libpkgapply-posix >= 3.0.0, < 4.0.0' \
  'libpkgapply-exec >= 2.0.0, < 3.0.0' \
  'libpkgresolve >= 2.0.0, < 3.0.0' \
  'libpkgtransaction >= 2.1.0, < 3.0.0' \
  'libpkgcheck >= 0.2.0, < 1.0.0' \
  'libpkgcheck-exec >= 0.4.0, < 1.0.0'; do
  grep -F "$contract" "$srcdir/CHANGELOG.md" >/dev/null || {
    echo "missing dependency floor in changelog: $contract" >&2
    exit 1
  }
done

[ "$(sed -n 's/^## \([0-9][0-9.]*\) - .*/\1/p' \
    "$srcdir/CHANGELOG.md" | head -n 1)" = "$version" ]
