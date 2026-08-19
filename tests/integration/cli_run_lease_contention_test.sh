#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
lock_holder=$5
fixture_collection=$6
root_view_fixture=$7
case $interpreter in
  /*)
    ;;
  *)
    interpreter_dir=$(dirname "$interpreter")
    interpreter_name=$(basename "$interpreter")
    interpreter=$(
      cd "$interpreter_dir"
      printf '%s/%s\n' "$(pwd -P)" "$interpreter_name"
    )
    ;;
esac

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-lease-contention.XXXXXX")
holder_pid=
cleanup()
{
  if [ -n "$holder_pid" ]; then
    kill -TERM "$holder_pid" 2>/dev/null || :
    wait "$holder_pid" 2>/dev/null || :
  fi
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

collection=$root/collection
state=$root/state
runtime=$root/runtime
build=$root/build
lifecycle=$root/lifecycle
target=$root/target
cp -R "$fixture_collection" "$collection"
binding=$($state_fixture "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)
build_uid=$uid
build_gid=$gid
build_groups=$groups

fail()
{
  printf 'pkgctl:cli-run-lease-contention: %s\n' "$*" >&2
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
      "pkgctl:cli-run-lease-contention: $label: missing expected text: $expected" \
      >&2
    dump_file "$label" "$file"
    exit 1
  fi
}

require_not_contains()
{
  label=$1
  file=$2
  unexpected=$3
  if grep -F -- "$unexpected" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-lease-contention: $label: unexpected text: $unexpected" \
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
  if [ "$expected" != "$actual" ]; then
    printf '%s\n' \
      "pkgctl:cli-run-lease-contention: $label: values differ" >&2
    printf '%s\n' "expected: $expected" >&2
    printf '%s\n' "actual:   $actual" >&2
    exit 1
  fi
}

require_absent()
{
  label=$1
  path=$2
  [ ! -e "$path" ] || fail "$label: unexpected path exists: $path"
}

require_executable()
{
  label=$1
  path=$2
  [ -x "$path" ] || fail "$label: expected executable is absent: $path"
}

run_command()
{
  intent=$1
  nonce=$2
  maximum_steps=$3
  set -- run --canonical-store "$state"
  if [ "$intent" = --start ]; then
    set -- "$@" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture x86_64 \
      --goal 'run=@base'
    set -- "$@" \
      --build-parallelism 1 \
      --build-source-date-epoch 0 \
      --operation-policy strict-exclusive
  fi
  set -- "$@" \
    "$intent" "$nonce" \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --lifecycle-root "$lifecycle" \
    --target-root "$target" \
    --interpreter "$interpreter" \
    --build-user-id "$build_uid" \
    --build-group-id "$build_gid" \
    --lifecycle-user-id "$uid" \
    --lifecycle-group-id "$gid" \
    --max-steps "$maximum_steps"
  for group in $build_groups; do
    if [ "$group" != "$build_gid" ]; then
      set -- "$@" --build-supplementary-group "$group"
    fi
  done
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --lifecycle-supplementary-group "$group"
    fi
  done
  if [ "$intent" = --start ]; then
    # The fixture emits these five option/value pairs as one trusted shell word
    # sequence so the CLI is exercised exactly as an operator would invoke it.
    # shellcheck disable=SC2086
    "$pkgctl" "$@" $binding
  else
    "$pkgctl" "$@"
  fi
}

capture_run()
{
  name=$1
  intent=$2
  nonce=$3
  maximum_steps=$4
  stdout_file=$root/$name.out
  stderr_file=$root/$name.err
  set +e
  run_command "$intent" "$nonce" "$maximum_steps" \
    >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  printf '%s\n' "$status" >"$root/$name.status"
}

release_holder()
{
  [ -n "$holder_pid" ] || fail 'holder release requested without a live holder'
  kill -TERM "$holder_pid"
  if ! wait "$holder_pid"; then
    dump_file holder-stderr "$root/holder.err"
    fail 'target mutation holder did not release cleanly'
  fi
  holder_pid=
}

mkdir -p "$runtime" "$build" "$lifecycle" "$target"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  target-locks \
  application-journals \
  payload \
  capture \
  rejected \
  completed \
  effect-bodies \
  content \
  construction-sessions \
  package-outputs \
  artifacts \
  check-temporary \
  lifecycle-sessions; do
  mkdir "$runtime/$directory"
done
"$root_view_fixture" "$build"
"$root_view_fixture" "$lifecycle"

initial_state=$($state_inspect_fixture "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

set -- resolve \
  --collection "core=$collection" \
  --canonical-store "$state" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'run=@base'
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/planned-resolution.out"
target_binding=$(sed -n \
  's/^state.target-binding=//p' "$root/planned-resolution.out")

set -- transaction \
  --collection "core=$collection" \
  --canonical-store "$state" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'run=@base'
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/planned.out"
transaction=$(sed -n 's/^session.identity=//p' "$root/planned.out")
[ "${#transaction}" -eq 64 ] || \
  fail "planned transaction identity has length ${#transaction}, expected 64"
pre_operation_steps=$(grep -E \
  '^node\.[0-9][0-9]*\.action=(build|check)$' "$root/planned.out" | \
  wc -l | tr -d ' ')
[ "$pre_operation_steps" -gt 0 ] || \
  fail 'planned transaction lacks construction/check work before operation'
expected_blocked_steps=$((pre_operation_steps + 1))
case $target_binding in
  v1:sha256:????????????????????????????????????????????????????????????????)
    ;;
  *)
    fail 'planned transaction lacks canonical target-binding identity'
    ;;
esac

"$lock_holder" "$runtime/target-locks" "$transaction" "$target_binding" \
  "$target" "$runtime" >"$root/holder.out" 2>"$root/holder.err" &
holder_pid=$!
holder_ready=0
holder_wait=0
while [ "$holder_wait" -lt 100 ]; do
  if grep -F 'ready v1:sha256:' "$root/holder.out" >/dev/null 2>&1; then
    holder_ready=1
    break
  fi
  if ! kill -0 "$holder_pid" 2>/dev/null; then
    break
  fi
  holder_wait=$((holder_wait + 1))
  sleep 0.05
done
if [ "$holder_ready" -ne 1 ]; then
  wait "$holder_pid" 2>/dev/null || :
  holder_pid=
  dump_file holder-stdout "$root/holder.out"
  dump_file holder-stderr "$root/holder.err"
  fail 'target mutation holder did not acquire the expected domain'
fi

lock_count=$(find "$runtime/target-locks" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')
[ "$lock_count" -eq 1 ] || fail "holder created $lock_count lock files, expected 1"

run_nonce=$(printf '%064d' 4)
capture_run blocked --start "$run_nonce" 8
blocked_status=$(cat "$root/blocked.status")
if grep -F 'native execution unavailable before transaction execution;' \
    "$root/blocked.err" >/dev/null; then
  dump_file native-execution-preflight "$root/blocked.err"
  if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
    fail 'release qualification requires the privileged native CLI integration path'
  fi
  exit 77
fi
[ "$blocked_status" -eq 1 ] || {
  dump_file blocked-stdout "$root/blocked.out"
  dump_file blocked-stderr "$root/blocked.err"
  fail "blocked start returned status $blocked_status, expected 1"
}
require_contains blocked "$root/blocked.out" "transaction $transaction"
require_contains blocked "$root/blocked.out" 'origin admitted'
require_contains blocked "$root/blocked.out" \
  'disposition mutation-authority-unavailable'
require_contains blocked "$root/blocked.out" \
  "steps $expected_blocked_steps"
require_contains blocked "$root/blocked.out" \
  "durable-steps $expected_blocked_steps"
require_contains blocked "$root/blocked.out" 'complete no'
require_contains blocked "$root/blocked.out" 'failed no'
require_not_contains blocked-stderr "$root/blocked.err" \
  'target mutation exclusion domain is already held'
require_absent blocked-target "$target/usr/bin/pkgctl-fixture"
require_equal blocked-state "$initial_state" "$($state_inspect_fixture "$state")"
if find "$runtime/effects" -type f -print -quit | grep . >/dev/null; then
  fail 'fresh contention created effect evidence before mutation authority'
fi

journal=$(sed -n 's/^journal //p' "$root/blocked.out")
[ "${#journal}" -eq 64 ] || \
  fail "blocked run journal identity has length ${#journal}, expected 64"
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/blocked-inspection.out"
require_contains blocked-inspection "$root/blocked-inspection.out" \
  'run.complete=false'
require_contains blocked-inspection "$root/blocked-inspection.out" \
  'run.failed=false'
require_contains blocked-inspection "$root/blocked-inspection.out" \
  'run.active=0'
operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/blocked-inspection.out")
operation_count=$(printf '%s\n' "$operation_indices" | sed '/^$/d' | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "blocked run retains $operation_count operation dispatches, expected 1"
operation_index=$operation_indices
require_contains blocked-operation "$root/blocked-inspection.out" \
  "dispatch.$operation_index.state=released-unstarted"
require_not_contains blocked-operation "$root/blocked-inspection.out" \
  "dispatch.$operation_index.effect-attempt="
released_dispatch=$(sed -n \
  "s/^dispatch\.$operation_index\.identity=//p" "$root/blocked-inspection.out")
[ "${#released_dispatch}" -eq 64 ] || fail 'blocked run lacks released dispatch identity'

capture_run duplicate-start --start "$run_nonce" 1
[ "$(cat "$root/duplicate-start.status")" -eq 1 ] || \
  fail 'duplicate start unexpectedly succeeded while run was admitted'
require_contains duplicate-start-stderr "$root/duplicate-start.err" \
  'exact transaction run is already admitted; use --resume'
require_equal duplicate-start-state "$initial_state" "$($state_inspect_fixture "$state")"
require_absent duplicate-start-target "$target/usr/bin/pkgctl-fixture"

rm -rf "$collection"
release_holder
capture_run resumed --resume "$run_nonce" 1
resumed_status=$(cat "$root/resumed.status")
[ "$resumed_status" -eq 0 ] || {
  dump_file resumed-stdout "$root/resumed.out"
  dump_file resumed-stderr "$root/resumed.err"
  fail "resume returned status $resumed_status, expected 0"
}
require_contains resumed "$root/resumed.out" "transaction $transaction"
require_contains resumed "$root/resumed.out" "journal $journal"
require_contains resumed "$root/resumed.out" 'origin resumed'
require_contains resumed "$root/resumed.out" 'disposition completed'
require_contains resumed "$root/resumed.out" 'steps 1'
require_contains resumed "$root/resumed.out" 'durable-steps 1'
require_contains resumed "$root/resumed.out" 'complete yes'
require_contains resumed "$root/resumed.out" 'failed no'
require_executable resumed-target "$target/usr/bin/pkgctl-fixture"
final_state=$($state_inspect_fixture "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 1'
require_contains final-state "$root/final-state.out" 'package fixture 1.0-1'

"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/resumed-inspection.out"
require_contains resumed-inspection "$root/resumed-inspection.out" \
  'run.complete=true'
operation_count=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/resumed-inspection.out" | wc -l | tr -d ' ')
[ "$operation_count" -eq 2 ] || \
  fail "completed run retains $operation_count operation dispatches, expected 2"
require_contains retained-released-operation "$root/resumed-inspection.out" \
  "dispatch.$operation_index.state=released-unstarted"
completed_operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.state=completed$/\1/p' \
  "$root/resumed-inspection.out" | while IFS= read -r index; do
    if grep -F "dispatch.$index.kind=operation" \
        "$root/resumed-inspection.out" >/dev/null; then
      printf '%s\n' "$index"
    fi
  done)
completed_operation_count=$(printf '%s\n' "$completed_operation_indices" | \
  sed '/^$/d' | wc -l | tr -d ' ')
[ "$completed_operation_count" -eq 1 ] || \
  fail "completed run retains $completed_operation_count completed operation dispatches, expected 1"
completed_operation_index=$completed_operation_indices
completed_dispatch=$(sed -n \
  "s/^dispatch\.$completed_operation_index\.identity=//p" \
  "$root/resumed-inspection.out")
[ "${#completed_dispatch}" -eq 64 ] || fail 'completed operation dispatch lacks identity'
[ "$completed_dispatch" != "$released_dispatch" ] || \
  fail 'resume reused the released-unstarted operation dispatch'
require_contains resumed-operation "$root/resumed-inspection.out" \
  "dispatch.$completed_operation_index.effect-attempt="

lock_count=$(find "$runtime/target-locks" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')
[ "$lock_count" -eq 1 ] || \
  fail "completed run retains $lock_count lock files, expected the same one"

capture_run repeated-resume --resume "$run_nonce" 1
[ "$(cat "$root/repeated-resume.status")" -eq 0 ] || \
  fail 'repeated completed resume failed'
require_contains repeated-resume "$root/repeated-resume.out" \
  'disposition completed'
require_contains repeated-resume "$root/repeated-resume.out" 'durable-steps 0'
require_equal repeated-resume-state "$final_state" "$($state_inspect_fixture "$state")"
