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
  build/inputs/build \
  build/inputs/check \
  check/source \
  check/package \
  check/inputs \
  target \
  tmp; do
  [ -d "$view/$path" ] || fail "stable mountpoint is absent: $path"
done

[ -z "$(find "$view/dev" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture populated backend-owned /dev authority'
[ -z "$(find "$view/build/inputs/build" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture invented a scenario-specific build input'
[ -z "$(find "$view/build/inputs/check" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
  fail 'fixture invented a scenario-specific check input'
