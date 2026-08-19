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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-publication-terminal-restart.XXXXXX")
cleanup()
{
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)
build_uid=$uid
build_gid=$gid
build_groups=$groups

fail()
{
  printf 'pkgctl:cli-run-publication-terminal-restart: %s\n' "$*" >&2
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
      "pkgctl:cli-run-publication-terminal-restart: $label: missing expected text: $expected" \
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
      "pkgctl:cli-run-publication-terminal-restart: $label: unexpected text: $unexpected" \
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
      "pkgctl:cli-run-publication-terminal-restart: $label: values differ" >&2
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

initialize_case()
{
  case_name=$1
  case_root=$root/$case_name
  collection=$case_root/collection
  state=$case_root/state
  runtime=$case_root/runtime
  build=$case_root/build
  lifecycle=$case_root/lifecycle
  target=$case_root/target
  mkdir "$case_root"
  cp -R "$fixture_collection" "$collection"
  binding=$($state_fixture "$state")

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
  "$interrupt_fixture" "$state" "$runtime/effects" -- "$pkgctl" "$@" $binding
}

capture_run()
{
  name=$1
  expected_status=$2
  intent=$3
  nonce=$4
  maximum_steps=$5
  stdout_file=$case_root/$name.out
  stderr_file=$case_root/$name.err
  set +e
  run_command "$intent" "$nonce" "$maximum_steps" \
    >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  if [ "$status" -ne "$expected_status" ]; then
    printf '%s\n' \
      "pkgctl:cli-run-publication-terminal-restart: $case_name/$name: expected status $expected_status, got $status" \
      >&2
    dump_file "$case_name/$name stdout" "$stdout_file"
    dump_file "$case_name/$name stderr" "$stderr_file"
    exit 1
  fi
}

qualify_case()
{
  initialize_case publication-terminal
  initial_state=$($state_inspect_fixture "$state")
  printf '%s\n' "$initial_state" >"$case_root/initial-state.out"
  require_contains initial-state "$case_root/initial-state.out" 'packages 0'
  require_absent initial-target "$target/usr/bin/pkgctl-fixture"

  run_nonce=$(printf '%064d' 7)
  set +e
  run_interrupted_command --start "$run_nonce" 8 \
    >"$case_root/interrupted.out" 2>"$case_root/interrupted.err"
  interrupt_status=$?
  set -e
  if [ "$interrupt_status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$case_root/interrupted.err" >/dev/null; then
      require_equal unavailable-state "$initial_state" \
        "$($state_inspect_fixture "$state")"
      require_absent unavailable-target "$target/usr/bin/pkgctl-fixture"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the privileged native CLI integration path'
      fi
      exit 77
    fi
    if [ "$interrupt_status" -eq 77 ] && \
        grep -F 'ptrace unavailable:' "$case_root/interrupted.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the publication-terminal interruption path'
      fi
      exit 77
    fi
    dump_file 'interrupted start stdout' "$case_root/interrupted.out"
    dump_file 'interrupted start stderr' "$case_root/interrupted.err"
    fail "interrupted start: expected status 0, got $interrupt_status"
  fi

  require_executable interrupted-target "$target/usr/bin/pkgctl-fixture"
  target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
  interrupted_state=$($state_inspect_fixture "$state")
  printf '%s\n' "$interrupted_state" >"$case_root/interrupted-state.out"
  require_contains interrupted-state "$case_root/interrupted-state.out" 'packages 1'
  require_contains interrupted-state "$case_root/interrupted-state.out" \
    'package fixture 1.0-1'

  run_head=$(single_file run-head "$runtime/run" '*.pjh')
  journal=$(basename "$run_head" .pjh)
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/interrupted-run.out"
  require_contains interrupted-run "$case_root/interrupted-run.out" 'run.complete=false'
  require_contains interrupted-run "$case_root/interrupted-run.out" 'run.failed=false'
  require_contains interrupted-run "$case_root/interrupted-run.out" 'run.disposition=active'
  operation_index=$(sed -n \
    's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
    "$case_root/interrupted-run.out")
  [ -n "$operation_index" ] || fail 'interrupted run lacks operation dispatch'
  require_contains interrupted-operation "$case_root/interrupted-run.out" \
    "dispatch.$operation_index.state=started"
  effect=$(sed -n \
    "s/^dispatch\.$operation_index\.effect-attempt=//p" \
    "$case_root/interrupted-run.out")
  [ "${#effect}" -eq 64 ] || fail 'interrupted run lacks exact effect attempt'

  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/interrupted-effect.out"
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.stage=publication-terminal'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.disposition=seal-terminal'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.automatically-continuable=true'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.application-outcome=completed'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.publication-outcome=published'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.publication-receipt='
  require_not_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.terminal-outcome='

  application_journal=$(sed -n \
    's/^effect.application-journal=//p' "$case_root/interrupted-effect.out")
  publication_request=$(sed -n \
    's/^effect.publication-request=//p' "$case_root/interrupted-effect.out")
  publication_receipt=$(sed -n \
    's/^effect.publication-receipt=//p' "$case_root/interrupted-effect.out")
  [ -n "$application_journal" ] || fail 'interrupted effect lacks application journal'
  [ -n "$publication_request" ] || fail 'interrupted effect lacks publication request'
  [ -n "$publication_receipt" ] || fail 'interrupted effect lacks publication receipt'

  run_record_before=$(sed -n 's/^run.record=//p' "$case_root/interrupted-run.out")
  effect_record_before=$(sed -n 's/^effect.record=//p' "$case_root/interrupted-effect.out")
  receipt_body=$(single_file retained-publication-receipt \
    "$runtime/effect-bodies" 'publication-receipt-*.bin')
  receipt_name=$(basename "$receipt_body")
  receipt_digest=$(sha256sum "$receipt_body")

  # Terminal publication evidence is part of the exact restart checkpoint. A
  # live canonical state cannot replace the retained receipt body.
  mv "$receipt_body" "$case_root/$receipt_name"
  rm -rf "$collection"
  capture_run missing-publication-receipt 1 --resume "$run_nonce" 1
  require_contains missing-publication-receipt-stderr \
    "$case_root/missing-publication-receipt.err" \
    "private run object is absent: $receipt_name"
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/after-missing-receipt-run.out"
  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/after-missing-receipt-effect.out"
  require_equal missing-receipt-run-record "$run_record_before" \
    "$(sed -n 's/^run.record=//p' "$case_root/after-missing-receipt-run.out")"
  require_equal missing-receipt-effect-record "$effect_record_before" \
    "$(sed -n 's/^effect.record=//p' "$case_root/after-missing-receipt-effect.out")"
  require_equal missing-receipt-target "$target_before" \
    "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
  require_equal missing-receipt-state "$interrupted_state" \
    "$($state_inspect_fixture "$state")"

  mv "$case_root/$receipt_name" "$receipt_body"
  require_equal restored-publication-receipt "$receipt_digest" \
    "$(sha256sum "$receipt_body")"

  # publication_terminal no longer owns subordinate application continuation.
  active_ref=$(single_file active-application-journal \
    "$runtime/application-journals" 'active-request-v1-sha256-*.ref')
  chmod u+w "$active_ref"
  printf '%s\n' 'not-an-application-journal-identity' >"$active_ref"
  chmod a-w "$active_ref"

  capture_run resume 0 --resume "$run_nonce" 8
  require_contains resume "$case_root/resume.out" 'origin resumed'
  require_contains resume "$case_root/resume.out" "journal $journal"
  require_contains resume "$case_root/resume.out" 'disposition completed'
  require_contains resume "$case_root/resume.out" 'complete yes'
  require_contains resume "$case_root/resume.out" 'failed no'
  require_equal target-not-reapplied "$target_before" \
    "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
  require_equal state-not-republished "$interrupted_state" \
    "$($state_inspect_fixture "$state")"

  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/resumed-effect.out"
  require_contains resumed-effect "$case_root/resumed-effect.out" 'effect.stage=terminal'
  require_contains resumed-effect "$case_root/resumed-effect.out" \
    'effect.terminal-outcome=completed'
  require_contains resumed-effect "$case_root/resumed-effect.out" \
    "effect.application-journal=$application_journal"
  require_contains resumed-effect "$case_root/resumed-effect.out" \
    "effect.publication-request=$publication_request"
  require_contains resumed-effect "$case_root/resumed-effect.out" \
    "effect.publication-receipt=$publication_receipt"
  require_contains resumed-effect "$case_root/resumed-effect.out" \
    'effect.publication-outcome=published'
  require_not_contains resumed-effect "$case_root/resumed-effect.out" \
    'effect.reconciled-state='

  resumed_effect_record=$(sed -n 's/^effect.record=//p' "$case_root/resumed-effect.out")
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/resumed-run.out"
  resumed_run_record=$(sed -n 's/^run.record=//p' "$case_root/resumed-run.out")

  capture_run repeat-resume 0 --resume "$run_nonce" 8
  require_contains repeat-resume "$case_root/repeat-resume.out" 'durable-steps 0'
  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/repeated-effect.out"
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/repeated-run.out"
  require_equal repeated-effect-record "$resumed_effect_record" \
    "$(sed -n 's/^effect.record=//p' "$case_root/repeated-effect.out")"
  require_equal repeated-run-record "$resumed_run_record" \
    "$(sed -n 's/^run.record=//p' "$case_root/repeated-run.out")"
}

qualify_case
