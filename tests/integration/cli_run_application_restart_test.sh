#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
interrupt_fixture=$5
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
fixture_collection=$6
root_view_fixture=$7
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-application-restart.XXXXXX")
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
  printf 'pkgctl:cli-run-application-restart: %s\n' "$*" >&2
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
      "pkgctl:cli-run-application-restart: $label: missing expected text: $expected" \
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
      "pkgctl:cli-run-application-restart: $label: unexpected text: $unexpected" \
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
    printf '%s\n' "pkgctl:cli-run-application-restart: $label: values differ" >&2
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
  # shellcheck disable=SC2086
  "$interrupt_fixture" "$runtime/application-journals" -- \
    "$pkgctl" "$@" $binding
}

capture_run()
{
  name=$1
  expected_status=$2
  intent=$3
  nonce=$4
  maximum_steps=$5
  stdout_file=$root/$name.out
  stderr_file=$root/$name.err
  set +e
  run_command "$intent" "$nonce" "$maximum_steps" \
    >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  if [ "$status" -ne "$expected_status" ]; then
    printf '%s\n' \
      "pkgctl:cli-run-application-restart: $name: expected status $expected_status, got $status" \
      >&2
    dump_file "$name stdout" "$stdout_file"
    dump_file "$name stderr" "$stderr_file"
    exit 1
  fi
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

run_nonce=$(printf '%064d' 4)
set +e
run_interrupted_command --start "$run_nonce" 8 \
  >"$root/interrupted.out" 2>"$root/interrupted.err"
interrupt_status=$?
set -e
if [ "$interrupt_status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/interrupted.err" >/dev/null; then
    require_equal unavailable-state "$initial_state" \
      "$($state_inspect_fixture "$state")"
    require_absent unavailable-target "$target/usr/bin/pkgctl-fixture"
    printf '%s\n' \
      'pkgctl:cli-run-application-restart: native execution preflight is unavailable in this process context' \
      >&2
    dump_file 'native execution preflight' "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  if [ "$interrupt_status" -eq 77 ] && \
      grep -F 'ptrace unavailable:' "$root/interrupted.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-run-application-restart: syscall interruption fixture is unavailable in this process context' \
      >&2
    dump_file 'syscall interruption fixture' "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the application-intent interruption path'
    fi
    exit 77
  fi
  printf '%s\n' \
    "pkgctl:cli-run-application-restart: interrupted start: expected status 0, got $interrupt_status" \
    >&2
  dump_file 'interrupted start stdout' "$root/interrupted.out"
  dump_file 'interrupted start stderr' "$root/interrupted.err"
  exit 1
fi

require_equal interrupted-state "$initial_state" \
  "$($state_inspect_fixture "$state")"
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

operation_count=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/interrupted-run.out" | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "interrupted run contains $operation_count operation dispatches, expected 1"
operation_index=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/interrupted-run.out")
require_contains interrupted-operation "$root/interrupted-run.out" \
  "dispatch.$operation_index.state=started"
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" \
  "$root/interrupted-run.out")
[ "${#effect}" -eq 64 ] || \
  fail "interrupted effect identity has length ${#effect}, expected 64"
attempt_session=$(sed -n \
  "s/^dispatch\.$operation_index\.attempt-session=//p" \
  "$root/interrupted-run.out")
[ "${#attempt_session}" -eq 64 ] || \
  fail "interrupted attempt-session identity has length ${#attempt_session}, expected 64"

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

active_ref=$(single_file active-application-journal \
  "$runtime/application-journals" 'active-request-v1-sha256-*.ref')
application_journal=$(cat "$active_ref")
case $application_journal in
  v1:sha256:*)
    application_journal_hex=${application_journal#v1:sha256:}
    ;;
  *)
    fail "active application journal has invalid identity: $application_journal"
    ;;
esac
[ "${#application_journal_hex}" -eq 64 ] &&
  ! printf '%s\n' "$application_journal_hex" | grep -E '[^0-9a-f]' >/dev/null ||
  fail "active application journal has invalid identity: $application_journal"
application_snapshot=$runtime/application-journals/journal-v1-sha256-$application_journal_hex.bin
[ -s "$application_snapshot" ] || \
  fail 'active application-request index names no durable journal snapshot'

observation_body=$(single_file retained-operation-observations \
  "$runtime/effect-bodies" 'operation-observations-*.bin')
observation_name=$(basename "$observation_body")
observation_digest=$(sha256sum "$observation_body")
run_record_before=$(sed -n 's/^run.record=//p' "$root/interrupted-run.out")
effect_record_before=$(sed -n 's/^effect.record=//p' "$root/interrupted-effect.out")

# Resume must consume the retained started-dispatch observations. Removing that
# exact body may not cause the controller to fall back to a fresh target read.
mv "$observation_body" "$root/$observation_name"
rm -rf "$collection"
capture_run missing-observation-resume 1 --resume "$run_nonce" 1
require_contains missing-observation-resume-stderr \
  "$root/missing-observation-resume.err" \
  "private run object is absent: $observation_name"
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/after-missing-observation-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/after-missing-observation-effect.out"
require_equal missing-observation-run-record "$run_record_before" \
  "$(sed -n 's/^run.record=//p' "$root/after-missing-observation-run.out")"
require_equal missing-observation-effect-record "$effect_record_before" \
  "$(sed -n 's/^effect.record=//p' "$root/after-missing-observation-effect.out")"
require_absent missing-observation-target "$target/usr/bin/pkgctl-fixture"
require_equal missing-observation-state "$initial_state" \
  "$($state_inspect_fixture "$state")"

mv "$root/$observation_name" "$observation_body"
require_equal restored-observation "$observation_digest" \
  "$(sha256sum "$observation_body")"

capture_run resume 0 --resume "$run_nonce" 8
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" "journal $journal"
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'

"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/resumed-run.out"
require_contains resumed-run "$root/resumed-run.out" 'run.complete=true'
require_contains resumed-run "$root/resumed-run.out" 'run.failed=false'
require_contains resumed-run "$root/resumed-run.out" 'run.disposition=completed'
require_contains resumed-run "$root/resumed-run.out" \
  "dispatch.$operation_index.state=completed"
require_contains resumed-run "$root/resumed-run.out" \
  "dispatch.$operation_index.effect-attempt=$effect"
run_record_terminal=$(sed -n 's/^run.record=//p' "$root/resumed-run.out")
[ "${#run_record_terminal}" -eq 64 ] || \
  fail "terminal run record identity has length ${#run_record_terminal}, expected 64"

require_executable resumed-target "$target/usr/bin/pkgctl-fixture"
printf 'pkgctl integration fixture\n' >"$root/expected-payload"
if ! cmp "$root/expected-payload" "$target/usr/bin/pkgctl-fixture"; then
  fail 'resumed target bytes differ from expected fixture payload'
fi
final_state=$($state_inspect_fixture "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 1'
require_contains final-state "$root/final-state.out" 'package fixture 1.0-1'

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/resumed-effect.out"
require_contains resumed-effect "$root/resumed-effect.out" 'effect.stage=terminal'
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.application-outcome=completed'
require_contains resumed-effect "$root/resumed-effect.out" \
  "effect.application-journal=$application_journal"
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.application-completed-evidence='
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.publication-outcome=published'
require_contains resumed-effect "$root/resumed-effect.out" \
  'effect.terminal-outcome=completed'
effect_record_terminal=$(sed -n 's/^effect.record=//p' "$root/resumed-effect.out")
[ "${#effect_record_terminal}" -eq 64 ] || \
  fail "terminal effect record identity has length ${#effect_record_terminal}, expected 64"
require_equal retained-observation-after-resume "$observation_digest" \
  "$(sha256sum "$observation_body")"

# Terminal replay must not consult the historical active application-journal
# locator. Poisoning it after completion makes any out-of-scope load explicit.
chmod u+w "$active_ref"
printf '%s\n' 'not-an-application-journal-identity' >"$active_ref"
chmod a-w "$active_ref"
target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
state_before=$($state_inspect_fixture "$state")
capture_run terminal-resume 0 --resume "$run_nonce" 4
require_contains terminal-resume "$root/terminal-resume.out" 'origin resumed'
require_contains terminal-resume "$root/terminal-resume.out" "journal $journal"
require_contains terminal-resume "$root/terminal-resume.out" \
  'disposition completed'
require_contains terminal-resume "$root/terminal-resume.out" 'durable-steps 0'
require_contains terminal-resume "$root/terminal-resume.out" 'complete yes'
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/terminal-resume-run.out"
"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/terminal-resume-effect.out"
require_equal terminal-resume-run-record "$run_record_terminal" \
  "$(sed -n 's/^run.record=//p' "$root/terminal-resume-run.out")"
require_equal terminal-resume-effect-record "$effect_record_terminal" \
  "$(sed -n 's/^effect.record=//p' "$root/terminal-resume-effect.out")"
require_equal terminal-resume-target "$target_before" \
  "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
require_equal terminal-resume-state "$state_before" \
  "$($state_inspect_fixture "$state")"
