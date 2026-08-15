#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
paths="$srcdir/include $srcdir/src $srcdir/cli $srcdir/tests/unit $srcdir/tests/support $srcdir/tests/fixtures $srcdir/meson.build $srcdir/meson.options"

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
cli_modules=$(sed -n "s/^cli_modules='\(.*\)'$/\1/p" "$direct_build")
[ -n "$cli_modules" ] || {
  echo 'direct qualification CLI module closure is missing' >&2
  exit 1
}

reviewed_core_modules='libcrypto libpkgsource libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-posix libpkgstate-plan libpkgstate-apply libpkgfetch libpkgsource-exec libpkgbuild libpkgbuild-exec libpkgbuild-image libpkgsource-plan libpkgbuild-plan libpkgimage libpkgimage-exec libpkgplan libpkgexec libpkgapply libpkgapply-posix libpkgapply-exec libpkgresolve libpkgtransaction libpkgcheck libpkgcheck-exec'

if grep -R -n 'ZEPPE_LIN_CHECK_' \
    "$srcdir/tests/fixtures" "$srcdir/tests/integration" "$srcdir/README.md" \
    "$srcdir/DESIGN.md" "$srcdir/TESTING.md" "$srcdir/man" >/dev/null; then
  echo 'current pkgctl sources retain branded check recipe variables' >&2
  exit 1
fi
reviewed_cli_modules='libpkgsource-yaml libpkgcatalog-codec libpkgexec-linux'
for constraint in \
  'libpkgsource >= 4.1.0' 'libpkgsource < 5.0.0' \
  'libpkgsource-yaml >= 2.0.0' 'libpkgsource-yaml < 3.0.0' \
  'libpkgsource-plan >= 2.0.0' 'libpkgsource-plan < 3.0.0' \
  'libpkgcatalog >= 4.0.0' 'libpkgcatalog < 5.0.0' \
  'libpkgcatalog-codec >= 4.0.0' 'libpkgcatalog-codec < 5.0.0' \
  'libpkgcatalog-acquire >= 4.0.0' 'libpkgcatalog-acquire < 5.0.0' \
  'libpkgfetch >= 3.0.0' 'libpkgfetch < 4.0.0' \
  'libpkgsource-exec >= 0.1.0' 'libpkgsource-exec < 1.0.0' \
  'libpkgbuild >= 3.0.1' 'libpkgbuild < 4.0.0' \
  'libpkgbuild-exec >= 3.3.0' 'libpkgbuild-exec < 4.0.0' \
  'libpkgbuild-image >= 1.0.1' 'libpkgbuild-image < 2.0.0' \
  'libpkgimage-exec >= 0.1.0' 'libpkgimage-exec < 1.0.0' \
  'libpkgbuild-plan >= 1.1.0' 'libpkgbuild-plan < 2.0.0' \
  'libpkgstate-apply >= 3.1.1' 'libpkgstate-apply < 4.0.0' \
  'libpkgresolve >= 4.0.0' 'libpkgresolve < 5.0.0' \
  'libpkgtransaction >= 4.0.0' 'libpkgtransaction < 5.0.0' \
  'libpkgcheck >= 0.3.0' 'libpkgcheck < 1.0.0' \
  'libpkgexec >= 2.2.0' 'libpkgexec < 3.0.0' \
  'libpkgexec-linux >= 0.7.0' 'libpkgexec-linux < 1.0.0' \
  'libpkgapply >= 3.0.1' 'libpkgapply < 4.0.0' \
  'libpkgapply-posix >= 3.2.1' 'libpkgapply-posix < 4.0.0' \
  'libpkgapply-exec >= 3.0.1' 'libpkgapply-exec < 4.0.0' \
  'libpkgcheck-exec >= 0.8.0' 'libpkgcheck-exec < 1.0.0'; do
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

for constraint in \
  'libpkgreconcile-apply >= 0.1.1' 'libpkgreconcile-apply < 1.0.0' \
  'libpkgreconcile-apply-posix >= 0.1.1' 'libpkgreconcile-apply-posix < 1.0.0'; do
  grep -F "'$constraint'" "$direct_build" >/dev/null || {
    echo "direct qualification omits source-4 reconciliation constraint: $constraint" >&2
    exit 1
  }
done

[ "$cli_modules" = "$reviewed_cli_modules" ] || {
  echo 'direct qualification CLI module closure differs from review' >&2
  echo "expected: $reviewed_cli_modules" >&2
  echo "actual:   $cli_modules" >&2
  exit 1
}
grep -F '"$srcdir/cli/run_command.cpp"' "$direct_build" >/dev/null || {
  echo 'direct qualification does not link the bounded CLI command' >&2
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
