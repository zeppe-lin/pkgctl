#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

fixture=$1
probe=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-lifecycle-intent-interrupt.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM

fail()
{
  printf 'pkgctl:lifecycle-intent-interrupt-fixture: %s\n' "$*" >&2
  exit 1
}

fresh_case()
{
  name=$1
  effects=$root/$name-effects
  markers=$root/$name-markers
  mkdir "$effects" "$markers"
}

fresh_case untraced
"$probe" "$effects" "$markers"
for index in 1 2 3 4 5; do
  [ -e "$markers/after-head-$index" ] || fail "untraced probe omitted marker $index"
done
[ -e "$markers/completed" ] || fail 'untraced probe did not complete'

run_interrupted()
{
  mode=$1
  expected_heads=$2
  last_completed_marker=$3
  first_absent_marker=$4
  fresh_case "$mode"
  set +e
  "$fixture" "$mode" "$effects" -- "$probe" "$effects" "$markers" \
    >"$root/$mode.out" 2>"$root/$mode.err"
  status=$?
  set -e
  if [ "$status" -eq 77 ] && \
      grep -F 'ptrace unavailable:' "$root/$mode.err" >/dev/null; then
    exit 77
  fi
  [ "$status" -eq 0 ] || {
    cat "$root/$mode.err" >&2
    fail "$mode supervisor failed with status $status"
  }
  set -- "$effects"/*.pjeh
  [ "$#" -eq "$expected_heads" ] || fail "$mode published unexpected head count"
  if [ "$last_completed_marker" -gt 0 ]; then
    [ -e "$markers/after-head-$last_completed_marker" ] || \
      fail "$mode missed prior userspace marker"
  fi
  [ ! -e "$markers/after-head-$first_absent_marker" ] || \
    fail "$mode killed after the next userspace action"
  [ ! -e "$markers/completed" ] || fail "$mode allowed probe completion"
}

run_interrupted before-lifecycle-intent 1 2 3
run_interrupted after-lifecycle-intent 1 4 5
