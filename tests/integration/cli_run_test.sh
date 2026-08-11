#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
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
fixture_collection=$5
root_view_fixture=$6
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run.XXXXXX")
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

run_command()
{
  intent=$1
  nonce=$2
  maximum_steps=$3
  inject_resume_semantics=${4:-no}
  set -- run --canonical-store "$state"
  if [ "$intent" = --start ]; then
    set -- "$@" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture x86_64 \
      --goal 'run=@base'
  elif [ "$inject_resume_semantics" = yes ]; then
    set -- "$@" --goal 'run=@base'
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

fail()
{
  printf 'pkgctl:cli-run: %s\n' "$*" >&2
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
      "pkgctl:cli-run: $name: expected status $expected_status, got $status" \
      >&2
    dump_file "$name stdout" "$stdout_file"
    dump_file "$name stderr" "$stderr_file"
    exit 1
  fi
}

require_contains()
{
  label=$1
  file=$2
  expected=$3
  if ! grep -F -- "$expected" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run: $label: missing expected text: $expected" >&2
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
    printf '%s\n' "pkgctl:cli-run: $label: values differ" >&2
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

zero_nonce=$(printf '%064d' 9)
capture_run zero-bound 2 --start "$zero_nonce" 0
require_contains zero-bound-stderr "$root/zero-bound.err" \
  'maximum step count must be greater than zero'
require_absent zero-bound-runtime "$runtime"
require_absent zero-bound-build "$build"
require_absent zero-bound-lifecycle "$lifecycle"
require_absent zero-bound-target "$target"

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

initial_state=$($state_inspect_fixture "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

"$root_view_fixture" "$lifecycle"

current_build_uid=$build_uid
if [ "$uid" -eq 0 ]; then
  build_uid=65534
else
  build_uid=0
fi
credential_nonce=$(printf '%064d' 7)
capture_run noncurrent-build-credentials 1 --start "$credential_nonce" 1
require_contains noncurrent-build-credentials-stderr \
  "$root/noncurrent-build-credentials.err" \
  'construction/check credentials must match the native supervisor'
require_equal noncurrent-build-credentials-state "$initial_state" \
  "$($state_inspect_fixture "$state")"
if find "$runtime/command-evidence" "$runtime/run" "$runtime/evidence" \
    "$runtime/effects" -type f -print -quit | grep . >/dev/null; then
  fail 'non-current build credentials retained transaction evidence before refusal'
fi
build_uid=$current_build_uid

broken_build=$root/broken-build
mkdir "$broken_build"
build=$broken_build
broken_nonce=$(printf '%064d' 8)
capture_run malformed-root 1 --start "$broken_nonce" 1
if grep -F 'native execution unavailable before transaction execution;' \
    "$root/malformed-root.err" >/dev/null; then
  require_equal unsupported-native-state "$initial_state" \
    "$($state_inspect_fixture "$state")"
  require_absent unsupported-native-target "$target/usr/bin/pkgctl-fixture"
  if find "$runtime/command-evidence" "$runtime/run" "$runtime/evidence" \
      "$runtime/effects" -type f -print -quit | grep . >/dev/null; then
    fail 'unsupported native execution retained transaction evidence before refusal'
  fi
  printf '%s\n' \
    'pkgctl:cli-run: native execution preflight is unavailable in this process context' \
    >&2
  dump_file 'native execution preflight' "$root/malformed-root.err"
  if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
    fail 'release qualification requires the privileged native CLI integration path'
  fi
  exit 77
fi
require_contains malformed-root "$root/malformed-root.out" \
  'disposition stopped-after-failure'
require_contains malformed-root "$root/malformed-root.out" 'durable-steps 1'
require_contains malformed-root "$root/malformed-root.out" 'failed yes'
require_contains malformed-root-stderr "$root/malformed-root.err" \
  'pkgctl: construction failed:'
require_contains malformed-root-stderr "$root/malformed-root.err" \
  'open root resource destination'
require_equal malformed-root-state "$initial_state" \
  "$($state_inspect_fixture "$state")"
require_absent malformed-root-target "$target/usr/bin/pkgctl-fixture"

build=$root/build
"$root_view_fixture" "$build"

run_nonce=$(printf '%064d' 1)
capture_run start 0 --start "$run_nonce" 1
require_contains start "$root/start.out" 'origin admitted'
require_contains start "$root/start.out" 'disposition step-limit-reached'
require_contains start "$root/start.out" 'steps 1'
require_contains start "$root/start.out" 'durable-steps 1'
require_contains start "$root/start.out" 'complete no'
require_contains start "$root/start.out" 'failed no'
transaction=$(sed -n 's/^transaction //p' "$root/start.out")
journal=$(sed -n 's/^journal //p' "$root/start.out")
[ "${#transaction}" -eq 64 ] || \
  fail "start: transaction identity has length ${#transaction}, expected 64"
[ "${#journal}" -eq 64 ] || \
  fail "start: journal identity has length ${#journal}, expected 64"

state_after_construction=$($state_inspect_fixture "$state")
require_equal construction-state "$initial_state" "$state_after_construction"
require_absent construction-target "$target/usr/bin/pkgctl-fixture"
evidence_count=$(find "$runtime/evidence" -type f | wc -l)
[ "$evidence_count" -gt 0 ] || \
  fail 'construction-evidence: no durable evidence files were retained'

inspection=$($pkgctl inspect-run --run-store "$runtime/run" --journal "$journal")
printf '%s\n' "$inspection" >"$root/inspection.out"
require_contains run-inspection "$root/inspection.out" "run.journal=$journal"
require_contains run-inspection "$root/inspection.out" 'run.complete=false'

capture_run second-start 1 --start "$run_nonce" 1
require_contains second-start-stderr "$root/second-start.err" \
  'exact transaction run is already admitted; use --resume'

set +e
run_command --resume "$run_nonce" 1 yes \
  >"$root/resume-semantic.out" 2>"$root/resume-semantic.err"
resume_semantic_status=$?
set -e
[ "$resume_semantic_status" -eq 2 ] || {
  dump_file resume-semantic-stdout "$root/resume-semantic.out"
  dump_file resume-semantic-stderr "$root/resume-semantic.err"
  fail "resume semantic redeclaration: expected status 2, got $resume_semantic_status"
}
require_contains resume-semantic-stderr "$root/resume-semantic.err" \
  '--resume uses retained transaction semantics'

rm -rf "$collection"
capture_run resume 0 --resume "$run_nonce" 8
require_contains resume "$root/resume.out" "transaction $transaction"
require_contains resume "$root/resume.out" "journal $journal"
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'

require_executable target-payload "$target/usr/bin/pkgctl-fixture"
printf 'pkgctl integration fixture\n' >"$root/expected-payload"
if ! cmp "$root/expected-payload" "$target/usr/bin/pkgctl-fixture"; then
  fail 'target-payload: installed bytes differ from expected fixture payload'
fi
final_state=$($state_inspect_fixture "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 1'
require_contains final-state "$root/final-state.out" 'package fixture 1.0-1'

target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
state_before=$($state_inspect_fixture "$state")
capture_run repeated-resume 0 --resume "$run_nonce" 4
require_contains repeated-resume "$root/repeated-resume.out" \
  "transaction $transaction"
require_contains repeated-resume "$root/repeated-resume.out" "journal $journal"
require_contains repeated-resume "$root/repeated-resume.out" 'origin resumed'
require_contains repeated-resume "$root/repeated-resume.out" 'disposition completed'
require_contains repeated-resume "$root/repeated-resume.out" 'durable-steps 0'
require_contains repeated-resume "$root/repeated-resume.out" 'complete yes'
require_equal repeated-resume-target "$target_before" \
  "$(sha256sum "$target/usr/bin/pkgctl-fixture")"
require_equal repeated-resume-state "$state_before" \
  "$($state_inspect_fixture "$state")"
