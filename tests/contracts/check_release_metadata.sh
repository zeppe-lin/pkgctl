#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
version=0.43.0
latest_release=0.43.0

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
  'inline constexpr unsigned version_minor = 43;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr unsigned version_patch = 0;'
require_line "$srcdir/include/pkgctl/version.h" \
  'inline constexpr const char* version_string = "0.43.0";'
require_line "$srcdir/src/core.cpp" \
  'static_assert(pkgctl::version_minor == 43);'

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
require_dependency_range libpkgstate-apply '>=3.1.3' '<4.0.0'
require_dependency_range libpkgfetch '>=3.0.0' '<4.0.0'
require_dependency_range libpkgsource-exec '>=0.1.0' '<1.0.0'
require_dependency_range libpkgbuild '>=3.0.3' '<4.0.0'
require_dependency_range libpkgbuild-exec '>=3.3.1' '<4.0.0'
require_dependency_range libpkgbuild-image '>=1.0.1' '<2.0.0'
require_dependency_range libpkgimage-exec '>=0.1.0' '<1.0.0'
require_dependency_range libpkgobject '>=0.1.0' '<1.0.0'
require_dependency_range libpkgbuild-plan '>=1.1.0' '<2.0.0'
require_dependency_range libpkgresolve '>=4.0.0' '<5.0.0'
require_dependency_range libpkgtransaction '>=4.1.0' '<5.0.0'
require_dependency_range libpkgcheck '>=0.3.0' '<1.0.0'
require_dependency_range libpkgcheck-exec '>=0.9.0' '<1.0.0'
require_dependency_range libpkgplan '>=0.3.0' '<1.0.0'
require_dependency_range libpkgexec '>=2.2.0' '<3.0.0'
require_dependency_range libpkgapply '>=4.0.1' '<5.0.0'
require_dependency_range libpkgapply-posix '>=4.0.0' '<5.0.0'
require_dependency_range libpkgapply-exec '>=3.0.2' '<4.0.0'
require_dependency_range libpkgexec-linux '>=0.7.1' '<1.0.0'

grep -F '## 0.43.0 - 2026-08-20' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.42.2 - 2026-08-20' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.42.1 - 2026-08-19' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.42.0 - 2026-08-19' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.40.4 - 2026-08-18' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.40.3 - 2026-08-18' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.40.2 - 2026-08-18' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.40.1 - 2026-08-18' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.40.0 - 2026-08-18' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.39.0 - 2026-08-17' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.38.0 - 2026-08-15' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.37.0 - 2026-08-13' "$srcdir/HISTORY.md" >/dev/null
grep -F 'HISTORY.md` keeps one mutable `Unreleased` section' \
  "$srcdir/MAINTAINING.md" >/dev/null
grep -F '`Unreleased` does not predict the next version number or release class.' \
  "$srcdir/MAINTAINING.md" >/dev/null
grep -F '## 0.36.0 - 2026-08-13' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.35.1 - 2026-08-12' "$srcdir/HISTORY.md" >/dev/null
grep -F '## 0.35.0 - 2026-08-12' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Release 0.43.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.42.2' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.42.1' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.42.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.40.4' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.40.3' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.40.2' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.40.1' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.40.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.39.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.38.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.37.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.36.0' "$srcdir/README.md" >/dev/null
grep -F 'Release 0.35.0' "$srcdir/README.md" >/dev/null
grep -F 'Version 0.43.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.2' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.1' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.4' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.3' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.2' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.1' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.39.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.38.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.37.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.36.0' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

grep -F 'Bounded native transaction command' "$srcdir/HISTORY.md" >/dev/null
grep -F '`pkgctl run`' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Uses one current fail-closed private command-evidence proof format' "$srcdir/HISTORY.md" >/dev/null
grep -F 'Version 0.35.0 exposes *pkgctl run*' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

temporary=${TMPDIR:-/tmp}/pkgctl-release-contract.$$
trap 'rm -f "$temporary.current" "$temporary.deps" "$temporary.027" "$temporary.unreleased" "$temporary.043" "$temporary.0422" "$temporary.0421" "$temporary.0404" "$temporary.0403" "$temporary.0402" "$temporary.0401" "$temporary.040" "$temporary.039"' EXIT HUP INT TERM

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
    "$srcdir/HISTORY.md" | head -n 1)" = "$latest_release" ]

[ "$(sed -n 's/^## \(.*\)$/\1/p' "$srcdir/HISTORY.md" | head -n 1)" = "Unreleased" ] || {
  echo 'HISTORY.md must begin with an Unreleased section' >&2
  exit 1
}

awk '
  /^## Unreleased$/ { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.unreleased"

if grep -q '[^[:space:]]' "$temporary.unreleased"; then
  echo 'release commit must leave a new empty Unreleased section' >&2
  exit 1
fi

awk '
  /^## 0\.43\.0 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.043"

for release_fact in \
  'Adds the native installed-package resource plane.' \
  'Successful native construction now admits its exact sealed public archive into' \
  'Replaces caller-supplied `--installed-tree` mappings with the explicit current' \
  'Add a privileged BUILD/CHECK/lifecycle × runtime/build/artifact host-root' \
  'Terminal cleanup now owns successful lifecycle scratch realizations as exact'; do
  grep -F -- "$release_fact" "$temporary.043" >/dev/null || {
    echo "0.43.0 history omits qualified resource-plane fact: $release_fact" >&2
    exit 1
  }
done

awk '
  /^## 0\.42\.2 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0422"

grep -F 'Corrects build-frontend authority so the exact BUILD/CHECK subject remains' \
  "$temporary.0422" >/dev/null || {
  echo '0.42.2 history omits direct build-subject authority correction' >&2
  exit 1
}
grep -F '`pkgctl build` no longer manufactures global `prefer-catalog`' \
  "$temporary.0422" >/dev/null || {
  echo '0.42.2 history omits dependency-preference authority correction' >&2
  exit 1
}
grep -F 'artifact import, cache discovery, dependency reconstruction, or a resolver API.' \
  "$temporary.0422" >/dev/null || {
  echo '0.42.2 history omits the negative reuse boundary' >&2
  exit 1
}
grep -F 'Old private build command evidence carrying the surplus global preference is' \
  "$temporary.0422" >/dev/null || {
  echo '0.42.2 history omits fail-closed old build-evidence behavior' >&2
  exit 1
}

awk '
  /^## 0\.42\.1 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0421"

grep -F 'Requires libpkgbuild 3.0.3' "$temporary.0421" >/dev/null || {
  echo '0.42.1 history omits the libpkgbuild 3.0.3 floor' >&2
  exit 1
}
grep -F 'Transitive BUILD-only selections' "$temporary.0421" >/dev/null || {
  echo '0.42.1 history omits phase-activation authority qualification' >&2
  exit 1
}

awk '
  /^## 0\.40\.4 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0404"

grep -F 'Requires libpkgapply-posix 3.2.3' "$temporary.0404" >/dev/null || {
  echo '0.40.4 history omits the libpkgapply-posix 3.2.3 floor' >&2
  exit 1
}
grep -F 'one representative descriptor per' "$temporary.0404" >/dev/null || {
  echo '0.40.4 history omits bounded active-durability authority' >&2
  exit 1
}
grep -F 'without reopening current package' "$temporary.0404" >/dev/null || {
  echo '0.40.4 history omits owner-side durability authority' >&2
  exit 1
}

awk '
  /^## 0\.40\.3 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0403"

grep -F 'Preserves subordinate application uncertainty' "$temporary.0403" >/dev/null || {
  echo '0.40.3 history omits application uncertainty preservation' >&2
  exit 1
}
grep -F 'application_resolution_required' "$temporary.0403" >/dev/null || {
  echo '0.40.3 history omits the non-retiring controller outcome' >&2
  exit 1
}
grep -F 'without another application, archive' "$temporary.0403" >/dev/null || {
  echo '0.40.3 history omits the no-replay restart boundary' >&2
  exit 1
}

awk '
  /^## 0\.40\.2 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0402"

grep -F 'Requires libpkgapply-posix 3.2.2' "$temporary.0402" >/dev/null || {
  echo '0.40.2 history omits the libpkgapply-posix 3.2.2 floor' >&2
  exit 1
}
grep -F 'bounds descriptor cardinality by live capture' "$temporary.0402" >/dev/null || {
  echo '0.40.2 history omits bounded capture-descriptor authority' >&2
  exit 1
}

awk '
  /^## 0\.40\.1 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.0401"

grep -F 'Requires libpkgtransaction 4.1.0' "$temporary.0401" >/dev/null || {
  echo '0.40.1 history omits the libpkgtransaction 4.1.0 floor' >&2
  exit 1
}
grep -F 'pkgctl continues to consume the transaction-owned executable' \
  "$temporary.0401" >/dev/null

awk '
  /^## 0\.40\.0 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.040"

grep -F 'Makes native target-operation policy one explicit start-only controller' \
  "$temporary.040" >/dev/null
grep -F 'operation-session evidence carries no' "$temporary.040" >/dev/null
grep -F 'Qualifies shared ownership as a five-layer authority staircase' \
  "$temporary.040" >/dev/null
grep -F 'Rebuilds empty-target/rootfs qualification around independent sealed image' \
  "$temporary.040" >/dev/null
grep -F 'Requires libpkgbuild-exec 3.3.1' "$temporary.040" >/dev/null || {
  echo '0.40 history omits the released libpkgbuild-exec 3.3.1 floor' >&2
  exit 1
}

awk '
  /^## 0\.39\.0 / { current = 1; next }
  /^## / && current { exit }
  current { print }
' "$srcdir/HISTORY.md" > "$temporary.039"

grep -F 'Renders structured native construction and check execution failures' \
  "$temporary.039" >/dev/null
grep -F 'Requires libpkgbuild-exec 3.3.0' "$temporary.039" >/dev/null
grep -F 'Requires libpkgexec 2.2.0, libpkgexec-linux 0.7.1, and libpkgcheck-exec' \
  "$temporary.039" >/dev/null
grep -F 'only check dependencies inhabit `/check/inputs/<canonical-package-name>`' \
  "$temporary.039" >/dev/null
grep -F 'archive-source build/check qualification' \
  "$temporary.039" >/dev/null
