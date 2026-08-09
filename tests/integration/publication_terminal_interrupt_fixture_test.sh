#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

fixture=$1
probe=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-publication-terminal-interrupt.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM
mkdir "$root/state" "$root/effects"

set +e
"$fixture" "$root/state" "$root/effects" -- "$probe" "$root/state" "$root/effects" \
  >"$root/out" 2>"$root/err"
status=$?
set -e
if [ "$status" -eq 77 ] && grep -F 'ptrace unavailable:' "$root/err" >/dev/null; then
  exit 77
fi
[ "$status" -eq 0 ] || {
  cat "$root/err" >&2
  exit 1
}
[ -e "$root/state/current" ] || {
  echo 'publication-terminal interrupt missed state selection' >&2
  exit 1
}
set -- "$root/effects"/*.pjeh
[ "$#" -eq 1 ] && [ -e "$1" ] || {
  echo 'publication-terminal interrupt missed effect-head publication' >&2
  exit 1
}
[ ! -e "$root/state/after-publication-terminal" ] || {
  echo 'publication-terminal interrupt occurred after the next userspace action' >&2
  exit 1
}
