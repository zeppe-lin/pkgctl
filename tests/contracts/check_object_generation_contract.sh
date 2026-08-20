#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

program=${1:?}

fail()
{
  echo "object-generation-contract: $*" >&2
  exit 1
}

command -v readelf >/dev/null 2>&1 || fail 'readelf is required'
needed=$(readelf -d "$program" | sed -n 's/^.*Shared library: \[\(.*\)\].*$/\1/p')

printf '%s\n' "$needed" | grep -Fx 'libpkgobject.so.0' >/dev/null ||
  fail 'pkgctl is not directly bound to libpkgobject.so.0'

if printf '%s\n' "$needed" | grep -E '^libpkgobject\.so\.[1-9][0-9]*$' >/dev/null; then
  fail 'unexpected future libpkgobject ABI generation remains in pkgctl'
fi
