#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

mode=$1
pkgctl=$2
state_fixture=$3
state_inspect_fixture=$4
runtime_root_fixture=$5
run_head_interrupt_fixture=$6
artifact_interrupt_fixture=$7
fixture_collection=$8
root_view_fixture=$9

case $mode in
  construction-started|artifact-published|check-started)
    ;;
  *)
    printf '%s\n' "pkgctl:cli-build-process-death: unknown mode: $mode" >&2
    exit 1
    ;;
esac

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-process-death.XXXXXX")
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
lifecycle=$root/lifecycle-must-remain-absent
target=$root/target-must-remain-absent
nonce=$(printf '%064d' 9)
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-build-process-death(%s): %s\n' "$mode" "$*" >&2
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
      "pkgctl:cli-build-process-death($mode): $label: missing expected text: $expected" \
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
  --max-steps 3
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done

set +e
case $mode in
  construction-started)
    "$run_head_interrupt_fixture" "$runtime/run" 2 -- \
      "$pkgctl" "$@" $binding >"$root/interrupted.out" 2>"$root/interrupted.err"
    interrupt_status=$?
    ;;
  artifact-published)
    "$artifact_interrupt_fixture" "$artifacts" -- \
      "$pkgctl" "$@" $binding >"$root/interrupted.out" 2>"$root/interrupted.err"
    interrupt_status=$?
    ;;
  check-started)
    "$run_head_interrupt_fixture" "$runtime/run" 8 -- \
      "$pkgctl" "$@" $binding >"$root/interrupted.out" 2>"$root/interrupted.err"
    interrupt_status=$?
    ;;
esac
set -e

if [ "$interrupt_status" -eq 77 ] && \
    grep -F 'ptrace unavailable:' "$root/interrupted.err" >/dev/null; then
  dump_file syscall-interruption-fixture "$root/interrupted.err"
  if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
    fail 'release qualification requires the process-death interruption path'
  fi
  exit 77
fi
if [ "$interrupt_status" -ne 0 ]; then
  dump_file interrupted-stdout "$root/interrupted.out"
  dump_file interrupted-stderr "$root/interrupted.err"
  fail "interruption fixture returned status $interrupt_status, expected 0"
fi

# The tracer killed pkgctl itself after the selected durable boundary. The
# canonical target remains untouched; all recovery authority is in the private
# command/run/construction evidence and public artifact roots.
final_state=$("$state_inspect_fixture" "$state")
require_equal interrupted-canonical-state "$initial_state" "$final_state"
require_absent lifecycle-root "$lifecycle"
require_absent target-root "$target"

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
set +e
"$pkgctl" "$@" >"$root/resume.out" 2>"$root/resume.err"
resume_status=$?
set -e
if [ "$resume_status" -ne 0 ]; then
  dump_file resume-stdout "$root/resume.out"
  dump_file resume-stderr "$root/resume.err"
  fail "resume returned status $resume_status, expected 0"
fi

require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'
require_contains resume "$root/resume.out" 'frontend build'
require_contains resume "$root/resume.out" 'artifacts 2'

final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"
require_absent lifecycle-root "$lifecycle"
require_absent target-root "$target"

artifact_list=$root/artifacts.list
find "$artifacts" -type f -name '*.tar' | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 2 ] || \
  fail "recovered build retained $artifact_count archives, expected 2"

check_count=$(find "$runtime/check-temporary" -type f -name check-ran | wc -l)
[ "$check_count" -eq 1 ] || \
  fail "recovered build retained $check_count check markers, expected 1"
check_marker=$(find "$runtime/check-temporary" -type f -name check-ran)
require_equal check-payload checked:tool-source+dependency-source \
  "$(cat "$check_marker")"
