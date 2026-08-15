#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

fixture=$1
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-native-root-view.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM

fail()
{
  printf 'pkgctl:native-root-view-fixture: %s\n' "$*" >&2
  exit 1
}

view=$root/view
mkdir "$view"
"$fixture" "$view"

for path in \
  dev \
  build/source \
  build/work \
  build/package \
  build/inputs \
  check/source \
  check/inputs \
  target \
  tmp; do
  [ -d "$view/$path" ] || fail "stable mountpoint is absent: $path"
done

[ -z "$(find "$view/dev" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture populated backend-owned /dev authority'
[ -z "$(find "$view/build/inputs" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture populated backend-owned build input namespace'
[ ! -e "$view/build/inputs/build" ] ||
  fail 'fixture retained obsolete build-scope namespace child'
[ ! -e "$view/build/inputs/check" ] ||
  fail 'fixture retained obsolete construction check-scope namespace child'
[ -z "$(find "$view/check/inputs" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture populated backend-owned check input namespace'
[ ! -e "$view/check/package" ] ||
  fail 'fixture invented backend-owned checked-package leaf'
