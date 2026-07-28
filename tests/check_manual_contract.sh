#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}

for page in "$srcdir/man/pkgctl.1.scd" \
            "$srcdir/man/pkgctl_orchestration.7.scd"; do
  [ -s "$page" ] || {
    echo "missing manual source: $page" >&2
    exit 1
  }
done

grep -F 'Version 0.4.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.4.0' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F '*--converge-exact*' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'The canonical state store is opened with *open_existing*' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'One target-mutation lease must remain held' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'exposes no effect-implying command' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE EFFECT ATTEMPTS' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'exact durable *libpkgapply* journal' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'newly held physical target-mutation lease' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

for obsolete in 'forbid-node' 'operation graph ordering' 'download named'; do
  if grep -R -n -F "$obsolete" "$srcdir/man" >/dev/null 2>&1; then
    echo "obsolete provisional controller semantic in manuals: $obsolete" >&2
    exit 1
  fi
done

if grep -nE '^[1-9][0-9]*\. ' "$srcdir"/man/*.scd >/dev/null 2>&1; then
  echo 'ordered scdoc lists must use dot-item markup' >&2
  exit 1
fi
