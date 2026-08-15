#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
fixture_collection=$5
root_view_fixture=$6
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-archive-source.XXXXXX")
cleanup()
{
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

collection=$root/collection
state=$root/state
runtime=$root/runtime
build=$root/build
artifacts=$root/artifacts
nonce=$(printf '%064d' 9)
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-build-archive-source: %s\n' "$*" >&2
  exit 1
}

dump_file()
{
  label=$1
  file=$2
  printf '%s\n' "--- $label ---" >&2
  if [ -s "$file" ]; then
    cat "$file" >&2
  else
    printf '%s\n' '<empty>' >&2
  fi
}

require_contains()
{
  label=$1
  file=$2
  expected=$3
  if ! grep -F -- "$expected" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-build-archive-source: $label: missing expected text: $expected" >&2
    dump_file "$label" "$file"
    exit 1
  fi
}

require_equal()
{
  label=$1
  expected=$2
  actual=$3
  [ "$actual" = "$expected" ] || \
    fail "$label: expected '$expected', got '$actual'"
}

mkdir "$runtime" "$build" "$artifacts"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  content \
  construction-sessions \
  package-outputs \
  check-temporary; do
  mkdir "$runtime/$directory"
done
"$root_view_fixture" "$build"
chmod_program=$(command -v chmod) || fail 'host chmod is unavailable for runtime fixture'
case $chmod_program in
  /*) ;;
  *) fail "host chmod did not resolve to an absolute path: $chmod_program" ;;
esac
interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$chmod_program")
case $interpreter in
  /*) ;;
  *) fail "runtime fixture returned non-absolute interpreter: $interpreter" ;;
esac

initial_state=$("$state_inspect_fixture" "$state")
set -- build archive-probe --check \
  --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --start "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 2
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done

set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/run.out" 2>"$root/run.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/run.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-build-archive-source: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the archive-source native CLI path'
    fi
    exit 77
  fi
  dump_file 'run stdout' "$root/run.out"
  dump_file 'run stderr' "$root/run.err"
  fail "build/check returned status $status, expected 0"
fi

require_contains run "$root/run.out" 'origin admitted'
require_contains run "$root/run.out" 'disposition completed'
require_contains run "$root/run.out" 'durable-steps 2'
require_contains run "$root/run.out" 'complete yes'
require_contains run "$root/run.out" 'failed no'
require_contains run "$root/run.out" 'frontend build'
require_contains run "$root/run.out" 'artifacts 1'
require_contains run "$root/run.out" 'artifact.0.package archive-probe'

final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"

artifact=$(find "$artifacts" -type f -name '*.tar' -print)
[ -n "$artifact" ] || fail 'public archive is absent'
[ "$(printf '%s\n' "$artifact" | wc -l | tr -d ' ')" -eq 1 ] || \
  fail 'expected exactly one public archive'
require_equal archive-payload archive-source \
  "$(tar -xOf "$artifact" archive-result)"

check_count=$(find "$runtime/check-temporary" -type f -name archive-check-ran | wc -l)
[ "$check_count" -eq 1 ] || \
  fail "retained $check_count archive check markers, expected 1"
check_marker=$(find "$runtime/check-temporary" -type f -name archive-check-ran)
require_equal check-payload checked:archive-source "$(cat "$check_marker")"
