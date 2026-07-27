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
  'package_operation' \
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

if grep -R -n 'SPDX-License-Identifier: GPL-2' \
    "$srcdir/include" "$srcdir/src" "$srcdir/cli" >/dev/null 2>&1; then
  echo 'GPL-2 SPDX identifier found in native source' >&2
  exit 1
fi

for required in \
  'libpkgcatalog-acquire' \
  'libpkgstate' \
  'libpkgresolve' \
  'libpkgtransaction'; do
  grep -F "'$required'" "$srcdir/meson.build" >/dev/null || {
    echo "missing native authority dependency: $required" >&2
    exit 1
  }
done
