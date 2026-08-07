#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
paths="$srcdir/include $srcdir/src $srcdir/cli $srcdir/tests/*.cpp $srcdir/tests/*.h $srcdir/meson.build $srcdir/meson.options"

for token in \
  'PKGMK_E_' \
  'Transaction::Result_t' \
  'pkgmksetting' \
  'libpkgcore' \
  'build_and_run' \
  'operation_graph' \
  'install_intent' \
  'system_update_intent' \
  'hold_installed_release'; do
  if grep -R -n -F "$token" $paths >/dev/null 2>&1; then
    echo "forbidden duplicated or legacy semantic in controller source: $token" >&2
    grep -R -n -F "$token" $paths >&2 || true
    exit 1
  fi
done

if grep -R -n -E \
    '(^|[[:space:]])(class|struct|using)[[:space:]]+package_operation([[:space:];=]|$)' \
    "$srcdir/include/pkgctl" "$srcdir/src" >/dev/null 2>&1; then
  echo 'controller-defined package operation model found' >&2
  exit 1
fi

if grep -R -n 'SPDX-License-Identifier: GPL-2' \
    "$srcdir/include" "$srcdir/src" "$srcdir/cli" >/dev/null 2>&1; then
  echo 'GPL-2 SPDX identifier found in native source' >&2
  exit 1
fi

direct_build=$srcdir/tests/run-direct.sh
core_modules=$(sed -n "s/^core_modules='\(.*\)'$/\1/p" "$direct_build")
[ -n "$core_modules" ] || {
  echo 'direct qualification core module closure is missing' >&2
  exit 1
}

reviewed_core_modules='libcrypto libpkgsource libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-posix libpkgstate-plan libpkgstate-apply libpkgfetch libpkgbuild libpkgbuild-exec libpkgbuild-image libpkgsource-plan libpkgbuild-plan libpkgimage libpkgplan libpkgexec libpkgapply libpkgapply-posix libpkgapply-exec libpkgresolve libpkgtransaction libpkgcheck libpkgcheck-exec'
for constraint in \
  'libpkgbuild-exec >= 2.2.0' 'libpkgbuild-exec < 3.0.0' \
  'libpkgcheck-exec >= 0.4.0' 'libpkgcheck-exec < 1.0.0'; do
  grep -F "'$constraint'" "$direct_build" >/dev/null || {
    echo "direct qualification omits adapter API constraint: $constraint" >&2
    exit 1
  }
done

[ "$core_modules" = "$reviewed_core_modules" ] || {
  echo 'direct qualification core module closure differs from review' >&2
  echo "expected: $reviewed_core_modules" >&2
  echo "actual:   $core_modules" >&2
  exit 1
}

for required in \
  'libcrypto' \
  'libpkgsource' \
  'libpkgsource-plan' \
  'libpkgcatalog' \
  'libpkgcatalog-acquire' \
  'libpkgstate' \
  'libpkgstate-posix' \
  'libpkgstate-plan' \
  'libpkgstate-apply' \
  'libpkgfetch' \
  'libpkgbuild' \
  'libpkgbuild-exec' \
  'libpkgbuild-image' \
  'libpkgbuild-plan' \
  'libpkgimage' \
  'libpkgplan' \
  'libpkgexec' \
  'libpkgapply' \
  'libpkgapply-posix' \
  'libpkgapply-exec' \
  'libpkgresolve' \
  'libpkgtransaction' \
  'libpkgcheck' \
  'libpkgcheck-exec'; do
  grep -F "'$required'" "$srcdir/meson.build" >/dev/null || {
    echo "missing native authority dependency: $required" >&2
    exit 1
  }
  case " $core_modules " in
    *" $required "*) ;;
    *)
      echo "direct qualification omits native authority dependency: $required" >&2
      exit 1
      ;;
  esac
done
