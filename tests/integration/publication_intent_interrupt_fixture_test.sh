#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

fixture=$1
probe=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-publication-intent-interrupt.XXXXXX")
cleanup()
{
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

fail()
{
  printf 'pkgctl:publication-intent-interrupt-fixture: %s\n' "$*" >&2
  exit 1
}

read_current()
{
  cat "$1/current"
}

mkdir "$root/untraced"
"$probe" "$root/untraced"
[ "$(read_current "$root/untraced")" = new ] || \
  fail 'untraced probe did not select the new value'
[ -f "$root/untraced/post-selection" ] || \
  fail 'untraced probe did not cross the selection boundary'

for point in before-selection after-selection; do
  directory=$root/$point
  mkdir "$directory"
  set +e
  "$fixture" "$point" "$directory" -- "$probe" "$directory" \
    >"$root/$point.out" 2>"$root/$point.err"
  status=$?
  set -e
  if [ "$status" -eq 77 ] && grep -F 'ptrace unavailable:' \
      "$root/$point.err" >/dev/null; then
    cat "$root/$point.err" >&2
    exit 77
  fi
  [ "$status" -eq 0 ] || {
    cat "$root/$point.out" >&2
    cat "$root/$point.err" >&2
    fail "$point supervisor returned status $status"
  }
  [ -f "$directory/pre-selection" ] || \
    fail "$point fired before the unrelated-rename oracle"
  [ ! -e "$directory/post-selection" ] || \
    fail "$point allowed the post-selection marker"

done

[ "$(read_current "$root/before-selection")" = old ] || \
  fail 'before-selection mode allowed the selector rename'
[ -f "$root/before-selection/current.tmp.probe" ] || \
  fail 'before-selection mode did not retain the unselected temporary'

[ "$(read_current "$root/after-selection")" = new ] || \
  fail 'after-selection mode did not retain the selected value'
[ ! -e "$root/after-selection/current.tmp.probe" ] || \
  fail 'after-selection mode left the selected temporary behind'
