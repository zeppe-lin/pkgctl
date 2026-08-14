#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

program=${1:?}

fail()
{
  echo "fetch-generation-contract: $*" >&2
  exit 1
}

command -v readelf >/dev/null 2>&1 || fail 'readelf is required'
needed=$(readelf -d "$program" | sed -n 's/^.*Shared library: \[\(.*\)\].*$/\1/p')

printf '%s\n' "$needed" | grep -Fx 'libpkgfetch.so.3' >/dev/null ||
  fail 'pkgctl is not directly bound to libpkgfetch.so.3'

if printf '%s\n' "$needed" | grep -E '^libpkgfetch\.so\.[012]$' >/dev/null; then
  fail 'obsolete libpkgfetch ABI generation remains in pkgctl'
fi
