#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1

[ "$($pkgctl --version)" = 'pkgctl 0.1.0' ]
$pkgctl --help | grep -F 'Package transaction commands are not enabled' >/dev/null

if $pkgctl install >/dev/null 2>&1; then
  echo 'unsupported transaction command unexpectedly succeeded' >&2
  exit 1
else
  status=$?
fi
[ "$status" -eq 2 ]
