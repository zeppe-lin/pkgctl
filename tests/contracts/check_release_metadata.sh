#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
version=0.37.0

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
require_line "$srcdir/meson.build" "  meson_version: '>=1.6.0',"
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_major = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_minor = 37;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_patch = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr const char* version_string = "0.37.0";'
require_line "$srcdir/src/core.cpp" \
  'static_assert(pkgctl::version_minor == 37);'

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

require_dependency_range libpkgsource '>=4.1.0' '<5.0.0'
require_dependency_range libpkgsource-yaml '>=2.0.0' '<3.0.0'
require_dependency_range libpkgsource-plan '>=2.0.0' '<3.0.0'
require_dependency_range libpkgcatalog '>=4.0.0' '<5.0.0'
require_dependency_range libpkgcatalog-codec '>=4.0.0' '<5.0.0'
require_dependency_range libpkgcatalog-acquire '>=4.0.0' '<5.0.0'
require_dependency_range libpkgstate '>=3.1.0' '<4.0.0'
require_dependency_range libpkgstate-apply '>=3.1.1' '<4.0.0'
require_dependency_range libpkgfetch '>=3.0.0' '<4.0.0'
require_dependency_range libpkgbuild '>=3.0.1' '<4.0.0'
require_dependency_range libpkgbuild-exec '>=3.1.0' '<4.0.0'
require_dependency_range libpkgbuild-image '>=1.0.1' '<2.0.0'
require_dependency_range libpkgbuild-plan '>=1.1.0' '<2.0.0'
require_dependency_range libpkgresolve '>=4.0.0' '<5.0.0'
require_dependency_range libpkgtransaction '>=4.0.0' '<5.0.0'
require_dependency_range libpkgcheck '>=0.3.0' '<1.0.0'
require_dependency_range libpkgcheck-exec '>=0.5.0' '<1.0.0'
require_dependency_range libpkgplan '>=0.3.0' '<1.0.0'
require_dependency_range libpkgexec '>=2.1.1' '<3.0.0'
require_dependency_range libpkgapply '>=3.0.1' '<4.0.0'
require_dependency_range libpkgapply-posix '>=3.2.1' '<4.0.0'
require_dependency_range libpkgapply-exec '>=3.0.1' '<4.0.0'
require_dependency_range libpkgexec-linux '>=0.6.2' '<1.0.0'

grep -F '## 0.37.0 - 2026-08-13' "$srcdir/HISTORY.md" >/dev/null
grep -F 'HISTORY.md` preserves tagged release facts only' \
  "$srcdir/MAINTAINING.md" >/dev/null
grep -F '## 0.36.0 - 2026-08-13' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.35.1 - 2026-08-12' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.35.0 - 2026-08-12' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Release 0.37.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.36.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.35.0' "$srcdir/README.md" >/dev/null
grep -F 'Version 0.37.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.36.0' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

grep -F 'Bounded native transaction command' "$srcdir/HISTORY.md" >/dev/null
grep -F '`pkgctl run`' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Uses one current fail-closed private command-evidence proof format' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Version 0.35.0 exposes *pkgctl run*' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

temporary=${TMPDIR:-/tmp}/pkgctl-release-contract.$$
trap 'rm -f "$temporary.current" "$temporary.deps" "$temporary.027"' EXIT HUP INT TERM

awk '
  /^## 0\.35\.0 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.current"
awk '
  /^### Dependency contract$/ { dependencies = 1; next }
  /^### / && dependencies { exit }
  dependencies { print }
' "$temporary.current" > "$temporary.deps"

for contract in \
  'libpkgsource >= 3.0.0, < 4.0.0' \
  'libpkgsource-yaml >= 1.0.0, < 2.0.0 (CLI only)' \
  'libpkgsource-plan >= 1.0.0, < 2.0.0' \
  'libpkgcatalog >= 3.0.0, < 4.0.0' \
  'libpkgcatalog-codec >= 3.0.0, < 4.0.0 (CLI only)' \
  'libpkgcatalog-acquire >= 3.0.0, < 4.0.0' \
  'libpkgstate >= 3.1.0, < 4.0.0' \
  'libpkgstate-posix >= 3.0.0, < 4.0.0' \
  'libpkgstate-plan >= 3.0.0, < 4.0.0' \
  'libpkgstate-apply >= 3.1.0, < 4.0.0' \
  'libpkgfetch >= 2.1.0, < 3.0.0' \
  'libpkgbuild >= 3.0.0, < 4.0.0' \
  'libpkgbuild-exec >= 2.2.0, < 3.0.0' \
  'libpkgbuild-image >= 1.0.0, < 2.0.0' \
  'libpkgbuild-plan >= 1.0.0, < 2.0.0' \
  'libpkgimage >= 0.4.0, < 1.0.0' \
  'libpkgplan >= 0.3.0, < 1.0.0' \
  'libpkgexec >= 2.1.0, < 3.0.0' \
  'libpkgexec-linux >= 0.6.0, < 1.0.0 (CLI only)' \
  'libpkgapply >= 3.0.0, < 4.0.0' \
  'libpkgapply-posix >= 3.1.0, < 4.0.0' \
  'libpkgapply-exec >= 3.0.0, < 4.0.0' \
  'libpkgresolve >= 3.0.0, < 4.0.0' \
  'libpkgtransaction >= 3.0.0, < 4.0.0' \
  'libpkgcheck >= 0.2.0, < 1.0.0' \
  'libpkgcheck-exec >= 0.4.0, < 1.0.0'; do
  count=$(grep -F -x -- "- $contract" "$temporary.deps" | wc -l | tr -d ' ')
  [ "$count" -eq 1 ] || {
    echo "current 0.35 dependency ledger contains $count copies of: $contract" >&2
    exit 1
  }
done

count=$(grep -c '^- libpkg' "$temporary.deps")
[ "$count" -eq 26 ] || {
  echo "current 0.35 dependency ledger contains $count libpkg entries, expected 26" >&2
  exit 1
}

# Historical release facts are immutable. 0.27 predates resolver/transaction 3.
awk '
  /^## 0\.27\.0 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.027"
grep -F -x -- '- libpkgresolve >= 2.0.0, < 3.0.0' "$temporary.027" >/dev/null || {
  echo '0.27 resolver dependency history was rewritten' >&2
  exit 1
}
grep -F -x -- '- libpkgtransaction >= 2.1.0, < 3.0.0' "$temporary.027" >/dev/null || {
  echo '0.27 transaction dependency history was rewritten' >&2
  exit 1
}
if grep -F -x -- '- libpkgresolve >= 3.0.0, < 4.0.0' "$temporary.027" >/dev/null || \
   grep -F -x -- '- libpkgtransaction >= 3.0.0, < 4.0.0' "$temporary.027" >/dev/null; then
  echo '0.27 dependency history contains a current-generation rewrite' >&2
  exit 1
fi

[ "$(sed -n 's/^## \([0-9][0-9.]*\) - .*/\1/p' \
    "$srcdir/HISTORY.md" | head -n 1)" = "$version" ]
