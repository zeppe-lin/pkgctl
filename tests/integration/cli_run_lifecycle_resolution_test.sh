#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
interrupt_fixture=$5
pre_collection_fixture=$6
post_collection_fixture=$7
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

fail()
{
  printf 'pkgctl:cli-run-lifecycle-resolution: %s\n' "$*" >&2
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
  if ! grep -F "$expected" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-lifecycle-resolution: $label: missing expected text: $expected" \
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
  if grep -F "$unexpected" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-lifecycle-resolution: $label: unexpected text: $unexpected" \
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
      "pkgctl:cli-run-lifecycle-resolution: $label: values differ" >&2
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

require_empty_directory()
{
  label=$1
  directory=$2
  if find "$directory" -mindepth 1 -maxdepth 1 -print -quit | grep . >/dev/null; then
    fail "$label: expected empty directory: $directory"
  fi
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
}

run_command()
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
    --goal "$lifecycle_goal" \
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
  "$pkgctl" "$@" $binding
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
  "$interrupt_fixture" "$mode" "$runtime/effects" -- "$pkgctl" "$@" $binding
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

qualify_pre_lifecycle
qualify_post_lifecycle
