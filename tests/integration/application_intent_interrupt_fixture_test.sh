#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

interrupt_fixture=$1
probe=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-application-intent-interrupt.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM

fail()
{
  printf 'pkgctl:application-intent-interrupt-fixture: %s\n' "$*" >&2
  exit 1
}

reference_name='active-request-v1-sha256-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.ref'
reference_body='v1:sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'

plain=$root/plain
mkdir "$plain"
"$probe" "$plain" "$plain/post-sync.marker"
[ -e "$plain/post-sync.marker" ] || fail 'untraced probe did not cross the final sync'
[ "$(cat "$plain/$reference_name")" = "$reference_body" ] || \
  fail 'untraced probe retained an invalid active reference'

interrupted=$root/interrupted
mkdir "$interrupted"
set +e
"$interrupt_fixture" "$interrupted" -- \
  "$probe" "$interrupted" "$interrupted/post-sync.marker" \
  >"$root/interrupted.out" 2>"$root/interrupted.err"
status=$?
set -e
if [ "$status" -eq 77 ]; then
  cat "$root/interrupted.err" >&2
  exit 77
fi
[ "$status" -eq 0 ] || {
  cat "$root/interrupted.err" >&2
  fail "interrupt fixture returned status $status"
}
[ ! -e "$interrupted/post-sync.marker" ] || \
  fail 'traced probe executed after the durable active-reference sync'
[ "$(cat "$interrupted/$reference_name")" = "$reference_body" ] || \
  fail 'interrupt boundary did not retain the durable active reference'
[ ! -e "$interrupted/.active-reference.tmp" ] || \
  fail 'interrupt boundary fired before active-reference publication'
