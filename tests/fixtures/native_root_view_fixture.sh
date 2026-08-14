#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
case $root in
  /*)
    ;;
  *)
    printf '%s\n' 'native-root-view-fixture: root path must be absolute' >&2
    exit 2
    ;;
esac

[ -d "$root" ] || {
  printf '%s\n' "native-root-view-fixture: root directory is absent: $root" >&2
  exit 2
}

# Stable logical destinations required by the native execution adapters.  The
# production Linux backend deliberately refuses to create these destinations:
# the supplied root view is caller-owned authority.  /dev is only a structural
# mountpoint: the Linux backend replaces it with its private execution-only
# device namespace.  Dynamic dependency input leaves are scenario-specific and
# are therefore not invented here.
mkdir -p \
  "$root/dev" \
  "$root/build/source" \
  "$root/build/work" \
  "$root/build/package" \
  "$root/build/inputs/build" \
  "$root/build/inputs/check" \
  "$root/check/source" \
  "$root/check/package" \
  "$root/check/inputs" \
  "$root/target" \
  "$root/tmp"
