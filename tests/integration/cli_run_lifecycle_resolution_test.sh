#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
interrupt_fixture=$5
credential_context_runner=$6
credential_context_preload=$7
pre_collection_fixture=$8
post_collection_fixture=$9
root_view_fixture=${10}
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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-lifecycle-resolution.XXXXXX")
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

invoke_pkgctl()
{
  if [ -n "${supervisor_uid:-}" ]; then
    "$credential_context_runner" "$supervisor_uid" "$supervisor_gid" \
      "$credential_context_preload" "$pkgctl" "$@"
  else
    "$pkgctl" "$@"
  fi
}

fail()
{
  printf 'pkgctl:cli-run-lifecycle-resolution: %s\n' "$*" >&2
  exit 1
}

dump_file()
{
  dump_label=$1
  dump_path=$2
  printf '%s\n' "--- $dump_label ---" >&2
  if [ -s "$dump_path" ]; then
    cat "$dump_path" >&2
  else
    printf '%s\n' '<empty>' >&2
  fi
}

require_contains()
{
  contains_label=$1
  contains_path=$2
  contains_expected=$3
  if ! grep -F -- "$contains_expected" "$contains_path" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-lifecycle-resolution: $contains_label: missing expected text: $contains_expected" \
      >&2
    dump_file "$contains_label" "$contains_path"
    exit 1
  fi
}

require_not_contains()
{
  not_contains_label=$1
  not_contains_path=$2
  not_contains_unexpected=$3
  if grep -F -- "$not_contains_unexpected" "$not_contains_path" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-lifecycle-resolution: $not_contains_label: unexpected text: $not_contains_unexpected" \
      >&2
    dump_file "$not_contains_label" "$not_contains_path"
    exit 1
  fi
}

require_equal()
{
  equal_label=$1
  equal_expected=$2
  equal_actual=$3
  if [ "$equal_expected" != "$equal_actual" ]; then
    printf '%s\n' \
      "pkgctl:cli-run-lifecycle-resolution: $equal_label: values differ" >&2
    printf '%s\n' "expected: $equal_expected" >&2
    printf '%s\n' "actual:   $equal_actual" >&2
    exit 1
  fi
}

require_absent()
{
  absent_label=$1
  absent_path=$2
  [ ! -e "$absent_path" ] || \
    fail "$absent_label: unexpected path exists: $absent_path"
}

require_empty_directory()
{
  empty_label=$1
  empty_directory=$2
  if find "$empty_directory" -mindepth 1 -maxdepth 1 -print -quit | grep . >/dev/null; then
    fail "$empty_label: expected empty directory: $empty_directory"
  fi
}

single_file()
{
  single_label=$1
  single_directory=$2
  single_pattern=$3
  set -- "$single_directory"/$single_pattern
  [ "$#" -eq 1 ] && [ -e "$1" ] || \
    fail "$single_label: expected exactly one path matching $single_directory/$single_pattern"
  printf '%s\n' "$1"
}

initialize_case()
{
  case_name=$1
  fixture_collection=$2
  lifecycle_goal=$3
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
  selected_interpreter=${interpreter_override:-$interpreter}
  set -- run --canonical-store "$state"
  if [ "$intent" = --start ]; then
    set -- "$@" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture x86_64 \
      --goal 'run=@base' \
      --goal "$lifecycle_goal"
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
    --package-object-store "$root/package-objects" \
    --build-root "$build" \
    --lifecycle-root "$lifecycle" \
    --target-root "$target" \
    --interpreter "$selected_interpreter" \
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
    # shellcheck disable=SC2086
    invoke_pkgctl "$@" $binding
  else
    invoke_pkgctl "$@"
  fi
}

run_interrupted_command()
{
  mode=$1
  intent=$2
  nonce=$3
  maximum_steps=$4
  set -- run \
    --collection "core=$collection" \
    --canonical-store "$state" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal 'run=@base' \
    --goal "$lifecycle_goal" \
    "$intent" "$nonce" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy strict-exclusive \
    --build-root-view "$(printf '%064d' 81)" \
    --lifecycle-root-view "$(printf '%064d' 82)" \
    --runtime-root "$runtime" \
    --package-object-store "$root/package-objects" \
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
  "$interrupt_fixture" "$mode" "$runtime/effects" -- "$pkgctl" "$@" $binding
}

capture_command()
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
    dump_file "$case_name/$name stdout" "$stdout_file"
    dump_file "$case_name/$name stderr" "$stderr_file"
    fail "$case_name/$name: expected status $expected_status, got $status"
  fi
}

capture_command_as()
{
  name=$1
  expected_status=$2
  intent=$3
  nonce=$4
  maximum_steps=$5
  supervisor_uid=$6
  supervisor_gid=$7
  capture_command "$name" "$expected_status" "$intent" "$nonce" "$maximum_steps"
  unset supervisor_uid supervisor_gid
}

capture_native_start_or_skip()
{
  name=$1
  nonce=$2
  maximum_steps=$3
  stdout_file=$case_root/$name.out
  stderr_file=$case_root/$name.err
  set +e
  run_command --start "$nonce" "$maximum_steps" \
    >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    return
  fi
  if grep -F -- 'native execution unavailable before transaction execution;' \
      "$stderr_file" >/dev/null; then
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      dump_file "$case_name/$name stdout" "$stdout_file"
      dump_file "$case_name/$name stderr" "$stderr_file"
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  dump_file "$case_name/$name stdout" "$stdout_file"
  dump_file "$case_name/$name stderr" "$stderr_file"
  fail "$case_name/$name: expected status 0, got $status"
}

capture_resume()
{
  name=$1
  nonce=$2
  stdout_file=$case_root/$name.out
  stderr_file=$case_root/$name.err
  set +e
  run_command --resume "$nonce" 8 >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  [ "$status" -eq 1 ] || {
    dump_file "$case_name/$name stdout" "$stdout_file"
    dump_file "$case_name/$name stderr" "$stderr_file"
    fail "$case_name/$name: expected status 1, got $status"
  }
  require_contains "$case_name/$name" "$stdout_file" 'origin resumed'
  require_contains "$case_name/$name" "$stdout_file" \
    'disposition external-resolution-required'
  require_contains "$case_name/$name" "$stdout_file" 'durable-steps 0'
  require_contains "$case_name/$name" "$stdout_file" 'complete no'
  require_contains "$case_name/$name" "$stdout_file" 'failed no'
}

inspect_interrupted()
{
  run_head=$(single_file run-head "$runtime/run" '*.pjh')
  journal=$(basename "$run_head" .pjh)
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/interrupted-run.out"
  require_contains interrupted-run "$case_root/interrupted-run.out" \
    'run.complete=false'
  require_contains interrupted-run "$case_root/interrupted-run.out" \
    'run.failed=false'
  require_contains interrupted-run "$case_root/interrupted-run.out" \
    'run.disposition=active'
  require_contains interrupted-run "$case_root/interrupted-run.out" \
    'run.external-evidence-required=true'
  operation_index=$(sed -n \
    's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
    "$case_root/interrupted-run.out")
  [ -n "$operation_index" ] || fail "$case_name: interrupted run lacks operation dispatch"
  require_contains interrupted-operation "$case_root/interrupted-run.out" \
    "dispatch.$operation_index.state=started"
  effect=$(sed -n \
    "s/^dispatch\.$operation_index\.effect-attempt=//p" \
    "$case_root/interrupted-run.out")
  [ "${#effect}" -eq 64 ] || fail "$case_name: interrupted run lacks exact effect attempt"

  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/interrupted-effect.out"
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    "effect.stage=$expected_stage"
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.disposition=external-resolution-required'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.automatically-continuable=false'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.external-resolution-required=true'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.active-index=0'

  run_record_before=$(sed -n 's/^run.record=//p' "$case_root/interrupted-run.out")
  effect_record_before=$(sed -n 's/^effect.record=//p' "$case_root/interrupted-effect.out")
}

check_records_unchanged()
{
  label=$1
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/$label-run.out"
  "$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
    >"$case_root/$label-effect.out"
  require_equal "$label-run-record" "$run_record_before" \
    "$(sed -n 's/^run.record=//p' "$case_root/$label-run.out")"
  require_equal "$label-effect-record" "$effect_record_before" \
    "$(sed -n 's/^effect.record=//p' "$case_root/$label-effect.out")"
  require_contains "$label-effect" "$case_root/$label-effect.out" \
    "effect.stage=$expected_stage"
  require_contains "$label-effect" "$case_root/$label-effect.out" \
    'effect.disposition=external-resolution-required'
}

qualify_live_execution_authority()
{
  initialize_case live-execution-authority "$pre_collection_fixture" \
    'lifecycle:pre-install=fixture'
  initial_state=$($state_inspect_fixture "$state")
  require_absent live-authority-target "$target/usr/bin/pkgctl-fixture"

  run_nonce=$(printf '%064d' 7)
  capture_native_start_or_skip start "$run_nonce" 1
  require_contains live-authority-start "$case_root/start.out" \
    'disposition step-limit-reached'
  require_contains live-authority-start "$case_root/start.out" 'durable-steps 1'
  require_contains live-authority-start "$case_root/start.out" 'complete no'

  run_head=$(single_file live-authority-run-head "$runtime/run" '*.pjh')
  journal=$(basename "$run_head" .pjh)
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/run-before.out"
  run_record_before=$(sed -n 's/^run.record=//p' "$case_root/run-before.out")
  [ "${#run_record_before}" -eq 64 ] || \
    fail 'live-execution-authority: missing durable run record before refusal'

  interpreter_override=/bin/false
  capture_command interpreter-refusal 1 --resume "$run_nonce" 1
  unset interpreter_override
  require_contains live-authority-interpreter \
    "$case_root/interpreter-refusal.err" \
    'current interpreter differs from admitted run authority'
  "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
    >"$case_root/run-after-interpreter.out"
  require_equal live-authority-interpreter-run-head "$run_record_before" \
    "$(sed -n 's/^run.record=//p' "$case_root/run-after-interpreter.out")"

  if [ "$uid" -eq 0 ]; then
    chown -R 65534:65534 "$root"
    capture_command_as credential-refusal 1 --resume "$run_nonce" 1 \
      65534 65534
    require_contains live-authority-credentials \
      "$case_root/credential-refusal.err" \
      'lifecycle credentials must match the native supervisor'
    chown -R "$uid:$gid" "$root"
    "$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
      >"$case_root/run-after-credentials.out"
    require_equal live-authority-credential-run-head "$run_record_before" \
      "$(sed -n 's/^run.record=//p' "$case_root/run-after-credentials.out")"
  fi

  require_absent live-authority-refusal-target "$target/usr/bin/pkgctl-fixture"
  require_equal live-authority-refusal-state "$initial_state" \
    "$($state_inspect_fixture "$state")"
}

qualify_pre_lifecycle()
{
  initialize_case before-lifecycle-intent "$pre_collection_fixture" \
    'lifecycle:pre-install=fixture'
  expected_stage=before-lifecycle-intent
  initial_state=$($state_inspect_fixture "$state")
  printf '%s\n' "$initial_state" >"$case_root/initial-state.out"
  require_contains initial-state "$case_root/initial-state.out" 'packages 0'
  require_absent initial-target "$target/usr/bin/pkgctl-fixture"

  run_nonce=$(printf '%064d' 8)
  set +e
  run_interrupted_command before-lifecycle-intent --start "$run_nonce" 8 \
    >"$case_root/interrupted.out" 2>"$case_root/interrupted.err"
  interrupt_status=$?
  set -e
  if [ "$interrupt_status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$case_root/interrupted.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the privileged native CLI integration path'
      fi
      exit 77
    fi
    if [ "$interrupt_status" -eq 77 ] && \
        grep -F 'ptrace unavailable:' "$case_root/interrupted.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the lifecycle-intent interruption path'
      fi
      exit 77
    fi
    dump_file 'before-lifecycle interrupted stdout' "$case_root/interrupted.out"
    dump_file 'before-lifecycle interrupted stderr' "$case_root/interrupted.err"
    fail "before-lifecycle interrupted start: expected status 0, got $interrupt_status"
  fi

  inspect_interrupted
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.before-total=1'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.before-completed=0'
  require_not_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.application-outcome='
  require_not_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.publication-request='
  require_empty_directory interrupted-lifecycle-sessions "$runtime/lifecycle-sessions"
  require_absent interrupted-target "$target/usr/bin/pkgctl-fixture"
  require_equal interrupted-state "$initial_state" "$($state_inspect_fixture "$state")"

  rm -rf "$collection"
  capture_resume resume "$run_nonce"
  check_records_unchanged resume
  require_empty_directory resume-lifecycle-sessions "$runtime/lifecycle-sessions"
  require_absent resume-target "$target/usr/bin/pkgctl-fixture"
  require_equal resume-state "$initial_state" "$($state_inspect_fixture "$state")"

  capture_resume repeat-resume "$run_nonce"
  check_records_unchanged repeat-resume
  require_empty_directory repeat-lifecycle-sessions "$runtime/lifecycle-sessions"
  require_absent repeat-resume-target "$target/usr/bin/pkgctl-fixture"
  require_equal repeat-resume-state "$initial_state" "$($state_inspect_fixture "$state")"
}

qualify_post_lifecycle()
{
  initialize_case after-lifecycle-intent "$post_collection_fixture" \
    'lifecycle:post-install=fixture'
  expected_stage=after-lifecycle-intent
  initial_state=$($state_inspect_fixture "$state")
  printf '%s\n' "$initial_state" >"$case_root/initial-state.out"
  require_contains initial-state "$case_root/initial-state.out" 'packages 0'
  require_absent initial-target "$target/usr/bin/pkgctl-fixture"

  run_nonce=$(printf '%064d' 9)
  set +e
  run_interrupted_command after-lifecycle-intent --start "$run_nonce" 8 \
    >"$case_root/interrupted.out" 2>"$case_root/interrupted.err"
  interrupt_status=$?
  set -e
  if [ "$interrupt_status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$case_root/interrupted.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the privileged native CLI integration path'
      fi
      exit 77
    fi
    if [ "$interrupt_status" -eq 77 ] && \
        grep -F 'ptrace unavailable:' "$case_root/interrupted.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the lifecycle-intent interruption path'
      fi
      exit 77
    fi
    dump_file 'after-lifecycle interrupted stdout' "$case_root/interrupted.out"
    dump_file 'after-lifecycle interrupted stderr' "$case_root/interrupted.err"
    fail "after-lifecycle interrupted start: expected status 0, got $interrupt_status"
  fi

  inspect_interrupted
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.before-total=0'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.after-total=1'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.after-completed=0'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.application-outcome=completed'
  require_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.application-completed-evidence='
  require_not_contains interrupted-effect "$case_root/interrupted-effect.out" \
    'effect.publication-request='
  require_empty_directory interrupted-lifecycle-sessions "$runtime/lifecycle-sessions"
  [ -x "$target/usr/bin/pkgctl-fixture" ] || fail 'post-lifecycle application target is absent'
  target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
  require_equal interrupted-state "$initial_state" "$($state_inspect_fixture "$state")"

  rm -rf "$collection"
  capture_resume resume "$run_nonce"
  check_records_unchanged resume
  require_empty_directory resume-lifecycle-sessions "$runtime/lifecycle-sessions"
  require_equal resume-target "$target_before" "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
  require_equal resume-state "$initial_state" "$($state_inspect_fixture "$state")"

  capture_resume repeat-resume "$run_nonce"
  check_records_unchanged repeat-resume
  require_empty_directory repeat-lifecycle-sessions "$runtime/lifecycle-sessions"
  require_equal repeat-resume-target "$target_before" "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
  require_equal repeat-resume-state "$initial_state" "$($state_inspect_fixture "$state")"
}

qualify_live_execution_authority
qualify_pre_lifecycle
qualify_post_lifecycle
