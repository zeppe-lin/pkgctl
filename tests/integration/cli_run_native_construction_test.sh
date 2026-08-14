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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-native-construction.XXXXXX")
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
lifecycle=$root/lifecycle-must-remain-absent
target=$root/target-must-remain-absent
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-run-native-construction: %s\n' "$*" >&2
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
      "pkgctl:cli-run-native-construction: $label: missing expected text: $expected" \
      >&2
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

require_absent()
{
  label=$1
  path=$2
  [ ! -e "$path" ] || fail "$label: unexpected path exists: $path"
}

mkdir "$runtime" "$build"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  content \
  construction-sessions \
  package-outputs \
  artifacts \
  check-temporary; do
  mkdir "$runtime/$directory"
done
"$root_view_fixture" "$build"
chmod_program=$(command -v chmod) || fail 'host chmod is unavailable for runtime fixture'
case $chmod_program in
  /*)
    ;;
  *)
    fail "host chmod did not resolve to an absolute path: $chmod_program"
    ;;
esac
interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$chmod_program")
case $interpreter in
  /*)
    ;;
  *)
    fail "runtime fixture returned non-absolute interpreter: $interpreter"
    ;;
esac

initial_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

set -- run --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'build=tool' \
  --goal 'check=tool' \
  --start "$(printf '%064d' 7)" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --lifecycle-root "$lifecycle" \
  --target-root "$target" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --lifecycle-user-id "$uid" \
  --lifecycle-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 3
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
    set -- "$@" --lifecycle-supplementary-group "$group"
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
      'pkgctl:cli-run-native-construction: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' \
      >&2
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  dump_file 'run stdout' "$root/run.out"
  dump_file 'run stderr' "$root/run.err"
  fail "run: expected status 0, got $status"
fi

require_contains run "$root/run.out" 'origin admitted'
require_contains run "$root/run.out" 'disposition completed'
require_contains run "$root/run.out" 'durable-steps 3'
require_contains run "$root/run.out" 'complete yes'
require_contains run "$root/run.out" 'failed no'

final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"
require_absent lifecycle-root "$lifecycle"
require_absent target-root "$target"
for directory in \
  target-locks \
  application-journals \
  application-checkpoints \
  payload \
  capture \
  rejected \
  completed \
  effect-bodies \
  lifecycle-sessions; do
  require_absent "operation-runtime-$directory" "$runtime/$directory"
done

artifact_list=$root/artifacts.list
find "$runtime/artifacts" -type f -name '*.tar' | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 2 ] || \
  fail "native construction retained $artifact_count package archives, expected 2"

dep_archive=
tool_archive=
while IFS= read -r archive; do
  if tar -tf "$archive" | grep -Fx -- 'dep-token' >/dev/null; then
    [ -z "$dep_archive" ] || fail 'multiple dependency archives contain dep-token'
    dep_archive=$archive
  fi
  if tar -tf "$archive" | grep -Fx -- 'tool-token' >/dev/null; then
    [ -z "$tool_archive" ] || fail 'multiple tool archives contain tool-token'
    tool_archive=$archive
  fi
done <"$artifact_list"
[ -n "$dep_archive" ] || fail 'dependency archive lacks dep-token'
[ -n "$tool_archive" ] || fail 'tool archive lacks tool-token'
require_equal dependency-payload dependency-source \
  "$(tar -xOf "$dep_archive" dep-token)"
require_equal tool-payload tool-source+dependency-source \
  "$(tar -xOf "$tool_archive" tool-token)"

check_count=$(find "$runtime/check-temporary" -type f -name check-ran | wc -l)
[ "$check_count" -eq 1 ] || \
  fail "native construction retained $check_count check markers, expected 1"
check_marker=$(find "$runtime/check-temporary" -type f -name check-ran)
require_equal check-payload checked:tool-source+dependency-source \
  "$(cat "$check_marker")"
