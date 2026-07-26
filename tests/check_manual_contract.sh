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

grep -F 'Version 0.1.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.1.0' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F '. forbid-node prevents' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

if grep -nE '^[1-9][0-9]*\. ' "$srcdir"/man/*.scd >/dev/null 2>&1; then
  echo 'ordered scdoc lists must use dot-item markup' >&2
  exit 1
fi
