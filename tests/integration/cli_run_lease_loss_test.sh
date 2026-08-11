#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
revoker=$5
credential_context_runner=$6
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

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-lease-loss.XXXXXX")
revoker_pid=
cleanup()
{
  if [ -n "$revoker_pid" ]; then
    kill -TERM "$revoker_pid" 2>/dev/null || :
    wait "$revoker_pid" 2>/dev/null || :
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
lifecycle_goal='lifecycle:post-install=fixture'

invoke_pkgctl()
{
  if [ -n "${supervisor_uid:-}" ]; then
    "$credential_context_runner" "$supervisor_uid" "$supervisor_gid" \
      "$pkgctl" "$@"
  else
    "$pkgctl" "$@"
  fi
}

fail()
{
  printf 'pkgctl:cli-run-lease-loss: %s\n' "$*" >&2
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
      "pkgctl:cli-run-lease-loss: $label: missing expected text: $expected" \
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
      "pkgctl:cli-run-lease-loss: $label: unexpected text: $unexpected" \
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
    printf '%s\n' "pkgctl:cli-run-lease-loss: $label: values differ" >&2
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
      --goal 'run=@base' \
      --goal "$lifecycle_goal"
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
    --source-date-epoch 0 \
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
    # shellcheck disable=SC2086
    invoke_pkgctl "$@" $binding
  else
    invoke_pkgctl "$@"
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

mkdir -p "$runtime" "$build" "$lifecycle" "$target"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  target-locks \
  application-journals \
  application-checkpoints \
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

set -- transaction \
  --collection "core=$collection" \
  --canonical-store "$state" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'run=@base' \
  --goal "$lifecycle_goal"
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

"$revoker" "$runtime/target-locks" "$target" \
  >"$root/revoker.out" 2>"$root/revoker.err" &
revoker_pid=$!
revoker_ready=0
revoker_wait=0
while [ "$revoker_wait" -lt 100 ]; do
  if grep -Fx 'ready' "$root/revoker.out" >/dev/null 2>&1; then
    revoker_ready=1
    break
  fi
  if ! kill -0 "$revoker_pid" 2>/dev/null; then
    break
  fi
  revoker_wait=$((revoker_wait + 1))
  sleep 0.05
done
if [ "$revoker_ready" -ne 1 ]; then
  wait "$revoker_pid" 2>/dev/null || :
  revoker_pid=
  dump_file revoker-stdout "$root/revoker.out"
  dump_file revoker-stderr "$root/revoker.err"
  fail 'target mutation revoker did not establish lifecycle synchronization'
fi

run_nonce=$(printf '%064d' 6)
capture_run lost --start "$run_nonce" 8
lost_status=$(cat "$root/lost.status")
if grep -F 'native execution unavailable before transaction execution;' \
    "$root/lost.err" >/dev/null; then
  dump_file native-execution-preflight "$root/lost.err"
  if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
    fail 'release qualification requires the privileged native CLI integration path'
  fi
  exit 77
fi
if [ "$lost_status" -ne 1 ]; then
  dump_file lost-stdout "$root/lost.out"
  dump_file lost-stderr "$root/lost.err"
  fail "lease-loss start returned status $lost_status, expected 1"
fi
if ! wait "$revoker_pid"; then
  revoker_pid=
  dump_file revoker-stdout "$root/revoker.out"
  dump_file revoker-stderr "$root/revoker.err"
  fail 'target mutation revoker did not revoke the anchored lease cleanly'
fi
revoker_pid=
require_contains revoker "$root/revoker.out" 'revoked '

require_contains lost "$root/lost.out" "transaction $transaction"
require_contains lost "$root/lost.out" 'origin admitted'
require_contains lost "$root/lost.out" 'disposition external-resolution-required'
require_contains lost "$root/lost.out" "steps $expected_blocked_steps"
require_contains lost "$root/lost.out" "durable-steps $expected_blocked_steps"
require_contains lost "$root/lost.out" 'complete no'
require_contains lost "$root/lost.out" 'failed no'
require_not_contains lost-stderr "$root/lost.err" \
  'target mutation exclusion domain is already held'
require_executable lost-target "$target/usr/bin/pkgctl-fixture"
require_contains post-install "$target/post-install-ran" 'post-install ran'
require_equal lost-state "$initial_state" "$($state_inspect_fixture "$state")"
require_absent ready-fifo "$target/.pkgctl-test-lease-loss-ready"
require_absent acknowledgement-fifo "$target/.pkgctl-test-lease-loss-ack"
lock_count=$(find "$runtime/target-locks" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')
[ "$lock_count" -eq 0 ] || \
  fail "lease-loss run retains $lock_count target locks, expected 0"

journal=$(sed -n 's/^journal //p' "$root/lost.out")
[ "${#journal}" -eq 64 ] || \
  fail "lease-loss run journal identity has length ${#journal}, expected 64"
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/lost-run.out"
require_contains lost-run "$root/lost-run.out" 'run.complete=false'
require_contains lost-run "$root/lost-run.out" 'run.failed=false'
require_contains lost-run "$root/lost-run.out" 'run.disposition=active'
require_contains lost-run "$root/lost-run.out" 'run.active=1'
require_contains lost-run "$root/lost-run.out" 'run.external-evidence-required=true'
operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/lost-run.out")
operation_count=$(printf '%s\n' "$operation_indices" | sed '/^$/d' | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "lease-loss run retains $operation_count operation dispatches, expected 1"
operation_index=$operation_indices
require_contains lost-operation "$root/lost-run.out" \
  "dispatch.$operation_index.state=started"
require_contains lost-operation "$root/lost-run.out" \
  "dispatch.$operation_index.observations=1"
operation_dispatch=$(sed -n \
  "s/^dispatch\.$operation_index\.identity=//p" "$root/lost-run.out")
[ "${#operation_dispatch}" -eq 64 ] || fail 'lease-loss operation lacks dispatch identity'
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" "$root/lost-run.out")
[ "${#effect}" -eq 64 ] || fail 'lease-loss operation lacks effect attempt'
run_record_before=$(sed -n 's/^run.record=//p' "$root/lost-run.out")

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/lost-effect.out"
require_contains lost-effect "$root/lost-effect.out" 'effect.stage=terminal'
require_contains lost-effect "$root/lost-effect.out" \
  'effect.disposition=terminal'
require_contains lost-effect "$root/lost-effect.out" 'effect.terminal=true'
require_contains lost-effect "$root/lost-effect.out" \
  'effect.automatically-continuable=true'
require_contains lost-effect "$root/lost-effect.out" \
  'effect.external-resolution-required=false'
require_contains lost-effect "$root/lost-effect.out" 'effect.after-total=1'
require_contains lost-effect "$root/lost-effect.out" 'effect.after-completed=1'
require_contains lost-effect "$root/lost-effect.out" \
  'effect.application-outcome=completed'
require_not_contains lost-effect "$root/lost-effect.out" \
  'effect.publication-request='
require_not_contains lost-effect "$root/lost-effect.out" \
  'effect.publication-receipt='
require_contains lost-effect "$root/lost-effect.out" \
  'effect.terminal-outcome=outer-lease-lost'
effect_record_before=$(sed -n 's/^effect.record=//p' "$root/lost-effect.out")

effect_count=$(find "$runtime/effects" -mindepth 1 -maxdepth 1 -type f \
  -name '*.pjeh' | wc -l | tr -d ' ')
[ "$effect_count" -eq 1 ] || \
  fail "lease-loss run retains $effect_count effect objects, expected 1"
target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
post_install_before=$(sha256sum "$target/post-install-ran")

rm -rf "$collection"
if [ "$uid" -eq 0 ]; then
  chown -R 65534:65534 "$root"
  supervisor_uid=65534
  supervisor_gid=65534
  capture_run resumed --resume "$run_nonce" 1
  unset supervisor_uid supervisor_gid
  chown -R "$uid:$gid" "$root"
else
  capture_run resumed --resume "$run_nonce" 1
fi
resumed_status=$(cat "$root/resumed.status")
[ "$resumed_status" -eq 1 ] || {
  dump_file resumed-stdout "$root/resumed.out"
  dump_file resumed-stderr "$root/resumed.err"
  fail "lease-loss resume returned status $resumed_status, expected 1"
}
require_contains resumed "$root/resumed.out" "transaction $transaction"
require_contains resumed "$root/resumed.out" "journal $journal"
require_contains resumed "$root/resumed.out" 'origin resumed'
require_contains resumed "$root/resumed.out" \
  'disposition external-resolution-required'
require_contains resumed "$root/resumed.out" 'steps 1'
require_contains resumed "$root/resumed.out" 'durable-steps 0'
require_contains resumed "$root/resumed.out" 'complete no'
require_contains resumed "$root/resumed.out" 'failed no'

"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/resumed-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/resumed-effect.out"
require_equal resumed-run-record "$run_record_before" \
  "$(sed -n 's/^run.record=//p' "$root/resumed-run.out")"
require_equal resumed-effect-record "$effect_record_before" \
  "$(sed -n 's/^effect.record=//p' "$root/resumed-effect.out")"
require_contains resumed-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.identity=$operation_dispatch"
require_contains resumed-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.state=started"
require_contains resumed-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.effect-attempt=$effect"
require_contains resumed-operation "$root/resumed-run.out" \
  "dispatch.$operation_index.observations=1"
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.terminal-outcome=outer-lease-lost'
require_equal resumed-target "$target_before" \
  "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
require_equal resumed-post-install "$post_install_before" \
  "$(sha256sum "$target/post-install-ran")"
require_equal resumed-state "$initial_state" "$($state_inspect_fixture "$state")"
lock_count=$(find "$runtime/target-locks" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')
[ "$lock_count" -eq 0 ] || \
  fail "lease-loss resume recreated $lock_count target locks, expected 0"

capture_run repeated-resume --resume "$run_nonce" 1
[ "$(cat "$root/repeated-resume.status")" -eq 1 ] || \
  fail 'repeated lease-loss resume unexpectedly completed'
require_contains repeated-resume "$root/repeated-resume.out" \
  'disposition external-resolution-required'
require_contains repeated-resume "$root/repeated-resume.out" 'durable-steps 0'
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/repeated-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/repeated-effect.out"
require_equal repeated-run-record "$run_record_before" \
  "$(sed -n 's/^run.record=//p' "$root/repeated-run.out")"
require_equal repeated-effect-record "$effect_record_before" \
  "$(sed -n 's/^effect.record=//p' "$root/repeated-effect.out")"
require_equal repeated-state "$initial_state" "$($state_inspect_fixture "$state")"
require_equal repeated-target "$target_before" \
  "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
lock_count=$(find "$runtime/target-locks" -mindepth 1 -maxdepth 1 -type f | wc -l | tr -d ' ')
[ "$lock_count" -eq 0 ] || \
  fail "repeated lease-loss resume recreated $lock_count target locks, expected 0"
