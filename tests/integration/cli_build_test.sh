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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build.XXXXXX")
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
wrong_artifacts=$root/wrong-artifacts
lifecycle=$root/lifecycle-must-remain-absent
target=$root/target-must-remain-absent
nonce=$(printf '%064d' 8)
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-build: %s\n' "$*" >&2
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
      "pkgctl:cli-build: $label: missing expected text: $expected" >&2
    dump_file "$label" "$file"
    exit 1
  fi
}

require_absent()
{
  label=$1
  path=$2
  [ ! -e "$path" ] || fail "$label: unexpected path exists: $path"
}

require_equal()
{
  label=$1
  expected=$2
  actual=$3
  [ "$actual" = "$expected" ] || \
    fail "$label: expected '$expected', got '$actual'"
}

mkdir "$runtime" "$build" "$artifacts" "$wrong_artifacts"
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
mkdir "$build/build/inputs/build/dep"
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

# Public artifact authority must not be hidden inside the private runtime. This
# refusal precedes command-evidence retention and run admission, so the same
# nonce remains valid for the real start below.
set -- build tool --check \
  --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --start "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$runtime/content" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/overlap.out" 2>"$root/overlap.err"
status=$?
set -e
[ "$status" -eq 1 ] || \
  fail "overlapping artifact root: expected status 1, got $status"
require_contains overlap "$root/overlap.err" \
  'build artifact root must be disjoint from private runtime root'
if find "$runtime/command-evidence" "$runtime/run" -type f -print -quit | \
    grep . >/dev/null; then
  fail 'overlapping artifact root retained command or run evidence'
fi

set -- build tool --check \
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
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done

set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/start.out" 2>"$root/start.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/start.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-build: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/start.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  dump_file 'start stdout' "$root/start.out"
  dump_file 'start stderr' "$root/start.err"
  fail "start: expected status 0, got $status"
fi

require_contains start "$root/start.out" 'origin admitted'
require_contains start "$root/start.out" 'disposition step-limit-reached'
require_contains start "$root/start.out" 'durable-steps 1'
require_contains start "$root/start.out" 'complete no'
require_contains start "$root/start.out" 'frontend build'
require_contains start "$root/start.out" 'artifacts 1'
require_contains start "$root/start.out" 'artifact.0.package dep'
require_contains start "$root/start.out" 'artifact.0.path '
require_contains start "$root/start.out" 'artifact.0.sha256 '
require_contains start "$root/start.out" 'artifact.0.binding-identity '
require_contains start "$root/start.out" 'artifact.0.image-identity '

set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$wrong_artifacts" \
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
"$pkgctl" "$@" >"$root/wrong-resume.out" 2>"$root/wrong-resume.err"
status=$?
set -e
[ "$status" -eq 1 ] || \
  fail "wrong-artifact resume: expected status 1, got $status"
require_contains wrong-resume "$root/wrong-resume.err" \
  'current artifact root differs from admitted command authority'
if find "$wrong_artifacts" -mindepth 1 -print -quit | grep . >/dev/null; then
  fail 'wrong-artifact resume published beneath refused artifact authority'
fi

set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
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
"$pkgctl" "$@" >"$root/resume.out" 2>"$root/resume.err" || {
  dump_file 'resume stdout' "$root/resume.out"
  dump_file 'resume stderr' "$root/resume.err"
  fail 'resume failed'
}
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'durable-steps 2'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'
require_contains resume "$root/resume.out" 'frontend build'
require_contains resume "$root/resume.out" 'artifacts 2'
require_contains resume "$root/resume.out" 'artifact.0.package dep'
require_contains resume "$root/resume.out" 'artifact.1.package tool'

final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"
require_absent lifecycle-root "$lifecycle"
require_absent target-root "$target"
require_absent private-artifact-root "$runtime/artifacts"
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
find "$artifacts" -type f -name '*.tar' | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 2 ] || \
  fail "build retained $artifact_count public package archives, expected 2"

# The public result coordinates must name exactly the archives retained under
# the caller-selected artifact authority.
for index in 0 1; do
  path=$(sed -n "s/^artifact\.$index\.path //p" "$root/resume.out")
  [ -n "$path" ] || fail "artifact.$index.path is absent"
  [ -f "$path" ] || fail "artifact.$index.path is not a retained file: $path"
  case $path in
    "$artifacts"/*)
      ;;
    *)
      fail "artifact.$index.path escaped public artifact root: $path"
      ;;
  esac
done

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
  fail "build retained $check_count check markers, expected 1"
check_marker=$(find "$runtime/check-temporary" -type f -name check-ran)
require_equal check-payload checked:tool-source+dependency-source \
  "$(cat "$check_marker")"

# Terminal replay is driven entirely by retained authority. The live catalog is
# deliberately gone, and no durable work may be repeated.
rm -rf "$collection"
set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 3
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
"$pkgctl" "$@" >"$root/terminal.out" 2>"$root/terminal.err" || {
  dump_file 'terminal stdout' "$root/terminal.out"
  dump_file 'terminal stderr' "$root/terminal.err"
  fail 'terminal resume failed'
}
require_contains terminal "$root/terminal.out" 'origin resumed'
require_contains terminal "$root/terminal.out" 'disposition completed'
require_contains terminal "$root/terminal.out" 'durable-steps 0'
require_contains terminal "$root/terminal.out" 'complete yes'
require_contains terminal "$root/terminal.out" 'frontend build'
require_contains terminal "$root/terminal.out" 'artifacts 2'
require_contains terminal "$root/terminal.out" 'artifact.0.package dep'
require_contains terminal "$root/terminal.out" 'artifact.1.package tool'

resume_inventory=$root/resume.inventory
terminal_inventory=$root/terminal.inventory
grep '^artifact\.' "$root/resume.out" >"$resume_inventory"
grep '^artifact\.' "$root/terminal.out" >"$terminal_inventory"
cmp "$resume_inventory" "$terminal_inventory" >/dev/null || {
  dump_file 'resume artifact inventory' "$resume_inventory"
  dump_file 'terminal artifact inventory' "$terminal_inventory"
  fail 'terminal replay changed retained artifact inventory'
}
