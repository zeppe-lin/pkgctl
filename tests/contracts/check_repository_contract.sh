#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}

[ -f "$srcdir/meson.options" ] || {
  echo 'missing canonical meson.options' >&2
  exit 1
}
[ ! -e "$srcdir/meson_options.txt" ] || {
  echo 'legacy meson_options.txt must not exist' >&2
  exit 1
}

grep -F -x "  meson_version: '>=1.6.0'," "$srcdir/meson.build" >/dev/null || {
  echo 'Meson minimum does not match the test-graph feature floor' >&2
  exit 1
}

echo 'repository-contract: ok'
