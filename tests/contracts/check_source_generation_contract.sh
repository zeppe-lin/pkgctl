#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

program=${1:?}

fail()
{
  echo "source-generation-contract: $*" >&2
  exit 1
}

command -v readelf >/dev/null 2>&1 || fail 'readelf is required'
needed=$(readelf -d "$program" | sed -n 's/^.*Shared library: \[\(.*\)\].*$/\1/p')

printf '%s\n' "$needed" | grep -Fx 'libpkgsource.so.4' >/dev/null ||
  fail 'pkgctl is not directly bound to libpkgsource.so.4'

if printf '%s\n' "$needed" | grep -E '^libpkgsource\.so\.[123]$' >/dev/null; then
  fail 'obsolete libpkgsource ABI generation remains in pkgctl'
fi
