#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
interrupt_fixture=$5
lock_holder=$6
fixture_collection=$7
root_view_fixture=$8
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

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-recovery-lease-contention.XXXXXX")
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
  printf 'pkgctl:cli-run-recovery-lease-contention: %s\n' "$*" >&2
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
      "pkgctl:cli-run-recovery-lease-contention: $label: missing expected text: $expected" \
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
      "pkgctl:cli-run-recovery-lease-contention: $label: unexpected text: $unexpected" \
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
      "pkgctl:cli-run-recovery-lease-contention: $label: values differ" >&2
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

single_file()
{
  label=$1
  directory=$2
  pattern=$3
  set -- "$directory"/$pattern
  [ "$#" -eq 1 ] && [ -e "$1" ] || \
    fail "$label: expected exactly one path matching $directory/$pattern"
  printf '%s\n' "$1"
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
      --operation-policy strict-exclusive \
      --build-root-view "$(printf '%064d' 81)" \
      --lifecycle-root-view "$(printf '%064d' 82)"
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

run_interrupted_command()
{
  intent=$1
  nonce=$2
  maximum_steps=$3
  set -- run \
    --collection "core=$collection" \
    --canonical-store "$state" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal 'run=@base' \
    "$intent" "$nonce" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy strict-exclusive \
    --build-root-view "$(printf '%064d' 81)" \
    --lifecycle-root-view "$(printf '%064d' 82)" \
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
  # shellcheck disable=SC2086
  "$interrupt_fixture" "$runtime/application-journals" -- \
    "$pkgctl" "$@" $binding
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
require_absent initial-target "$target/usr/bin/pkgctl-fixture"

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
case $target_binding in
  v1:sha256:????????????????????????????????????????????????????????????????)
    ;;
  *)
    fail 'planned resolution lacks canonical target-binding identity'
    ;;
esac

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

run_nonce=$(printf '%064d' 5)
set +e
run_interrupted_command --start "$run_nonce" 8 \
  >"$root/interrupted.out" 2>"$root/interrupted.err"
interrupt_status=$?
set -e
if [ "$interrupt_status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/interrupted.err" >/dev/null; then
    dump_file native-execution-preflight "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  if [ "$interrupt_status" -eq 77 ] && \
      grep -F 'ptrace unavailable:' "$root/interrupted.err" >/dev/null; then
    dump_file syscall-interruption-fixture "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the application-intent interruption path'
    fi
    exit 77
  fi
  dump_file interrupted-stdout "$root/interrupted.out"
  dump_file interrupted-stderr "$root/interrupted.err"
  fail "interrupted start returned status $interrupt_status, expected 0"
fi

require_equal interrupted-state "$initial_state" "$($state_inspect_fixture "$state")"
require_absent interrupted-target "$target/usr/bin/pkgctl-fixture"

run_head=$(single_file run-head "$runtime/run" '*.pjh')
journal=$(basename "$run_head" .pjh)
[ "${#journal}" -eq 64 ] || \
  fail "interrupted run journal identity has length ${#journal}, expected 64"
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/interrupted-run.out"
require_contains interrupted-run "$root/interrupted-run.out" 'run.complete=false'
require_contains interrupted-run "$root/interrupted-run.out" 'run.failed=false'
require_contains interrupted-run "$root/interrupted-run.out" 'run.disposition=active'
require_contains interrupted-run "$root/interrupted-run.out" 'run.active=1'
operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/interrupted-run.out")
operation_count=$(printf '%s\n' "$operation_indices" | sed '/^$/d' | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "interrupted run retains $operation_count operation dispatches, expected 1"
operation_index=$operation_indices
require_contains interrupted-operation "$root/interrupted-run.out" \
  "dispatch.$operation_index.state=started"
operation_dispatch=$(sed -n \
  "s/^dispatch\.$operation_index\.identity=//p" "$root/interrupted-run.out")
[ "${#operation_dispatch}" -eq 64 ] || fail 'started operation dispatch lacks identity'
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" "$root/interrupted-run.out")
[ "${#effect}" -eq 64 ] || fail 'started operation dispatch lacks effect attempt'
run_record_before=$(sed -n 's/^run.record=//p' "$root/interrupted-run.out")

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/interrupted-effect.out"
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.stage=application-intent'
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.disposition=resume-application'
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.automatically-continuable=true'
require_not_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.application-receipt='
effect_record_before=$(sed -n 's/^effect.record=//p' "$root/interrupted-effect.out")

effect_count=$(find "$runtime/effects" -mindepth 1 -maxdepth 1 -type f \
  -name '*.pjeh' | wc -l | tr -d ' ')
[ "$effect_count" -eq 1 ] || \
  fail "interrupted run retains $effect_count effect objects, expected 1"

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
  fail 'target mutation holder did not acquire the expected recovery domain'
fi

rm -rf "$collection"
capture_run blocked-resume --resume "$run_nonce" 8
blocked_status=$(cat "$root/blocked-resume.status")
[ "$blocked_status" -eq 1 ] || {
  dump_file blocked-resume-stdout "$root/blocked-resume.out"
  dump_file blocked-resume-stderr "$root/blocked-resume.err"
  fail "contended resume returned status $blocked_status, expected 1"
}
require_contains blocked-resume "$root/blocked-resume.out" "transaction $transaction"
require_contains blocked-resume "$root/blocked-resume.out" "journal $journal"
require_contains blocked-resume "$root/blocked-resume.out" 'origin resumed'
require_contains blocked-resume "$root/blocked-resume.out" \
  'disposition mutation-authority-unavailable'
require_contains blocked-resume "$root/blocked-resume.out" 'steps 1'
require_contains blocked-resume "$root/blocked-resume.out" 'durable-steps 0'
require_contains blocked-resume "$root/blocked-resume.out" 'complete no'
require_contains blocked-resume "$root/blocked-resume.out" 'failed no'
require_not_contains blocked-resume-stderr "$root/blocked-resume.err" \
  'target mutation exclusion domain is already held'
require_absent blocked-target "$target/usr/bin/pkgctl-fixture"
require_equal blocked-state "$initial_state" "$($state_inspect_fixture "$state")"

"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/blocked-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/blocked-effect.out"
require_equal blocked-run-record "$run_record_before" \
  "$(sed -n 's/^run.record=//p' "$root/blocked-run.out")"
require_equal blocked-effect-record "$effect_record_before" \
  "$(sed -n 's/^effect.record=//p' "$root/blocked-effect.out")"
require_contains blocked-run "$root/blocked-run.out" 'run.active=1'
require_contains blocked-operation "$root/blocked-run.out" \
  "dispatch.$operation_index.state=started"
require_contains blocked-operation-effect "$root/blocked-run.out" \
  "dispatch.$operation_index.effect-attempt=$effect"
require_contains blocked-effect "$root/blocked-effect.out" \
  'effect.stage=application-intent'
blocked_operation_count=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/blocked-run.out" | wc -l | tr -d ' ')
[ "$blocked_operation_count" -eq 1 ] || \
  fail "contended resume retains $blocked_operation_count operation dispatches, expected 1"
blocked_effect_count=$(find "$runtime/effects" -mindepth 1 -maxdepth 1 -type f \
  -name '*.pjeh' | wc -l | tr -d ' ')
[ "$blocked_effect_count" -eq 1 ] || \
  fail "contended resume retains $blocked_effect_count effect objects, expected 1"

release_holder
capture_run resumed --resume "$run_nonce" 1
resumed_status=$(cat "$root/resumed.status")
[ "$resumed_status" -eq 0 ] || {
  dump_file resumed-stdout "$root/resumed.out"
  dump_file resumed-stderr "$root/resumed.err"
  fail "resume after holder release returned status $resumed_status, expected 0"
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
  >"$root/resumed-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/resumed-effect.out"
require_contains resumed-run "$root/resumed-run.out" 'run.complete=true'
require_contains resumed-run "$root/resumed-run.out" 'run.failed=false'
require_contains resumed-run "$root/resumed-run.out" 'run.disposition=completed'
resumed_operation_count=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/resumed-run.out" | wc -l | tr -d ' ')
[ "$resumed_operation_count" -eq 1 ] || \
  fail "completed recovery retains $resumed_operation_count operation dispatches, expected 1"
require_contains resumed-same-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.identity=$operation_dispatch"
require_contains resumed-same-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.state=completed"
require_contains resumed-same-effect "$root/resumed-run.out" \
  "dispatch.$operation_index.effect-attempt=$effect"
require_contains resumed-effect "$root/resumed-effect.out" 'effect.stage=terminal'
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.application-outcome=completed'
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.publication-outcome=published'
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.terminal-outcome=completed'

capture_run repeated-resume --resume "$run_nonce" 1
[ "$(cat "$root/repeated-resume.status")" -eq 0 ] || \
  fail 'repeated completed resume failed'
require_contains repeated-resume "$root/repeated-resume.out" 'disposition completed'
require_contains repeated-resume "$root/repeated-resume.out" 'durable-steps 0'
require_equal repeated-resume-state "$final_state" "$($state_inspect_fixture "$state")"
