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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-construction-only.XXXXXX")
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
lifecycle=$root/lifecycle-must-remain-absent
target=$root/target-must-remain-absent
cp -R "$fixture_collection" "$collection"
binding=$($state_fixture "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-run-construction-only: %s\n' "$*" >&2
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
      "pkgctl:cli-run-construction-only: $label: missing expected text: $expected" \
      >&2
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

run_command()
{
  intent=$1
  nonce=$2
  selected_interpreter=${interpreter_override:-$interpreter}
  set -- run --canonical-store "$state"
  if [ "$intent" = --start ]; then
    set -- "$@" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture fixture-target \
      --goal 'build=fixture'
    # Keep build and target architecture intentionally distinct. Command
    # evidence decoding must preserve field order across a fresh process.
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
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --lifecycle-user-id "$uid" \
    --lifecycle-group-id "$gid" \
    --max-steps 1
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --build-supplementary-group "$group"
      set -- "$@" --lifecycle-supplementary-group "$group"
    fi
  done
  if [ "$intent" = --start ]; then
    # shellcheck disable=SC2086
    "$pkgctl" "$@" $binding
  else
    "$pkgctl" "$@"
  fi
}

capture_start_or_skip()
{
  nonce=$1
  set +e
  run_command --start "$nonce" >"$root/start.out" 2>"$root/start.err"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    return
  fi
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/start.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-run-construction-only: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' \
      >&2
    dump_file 'native execution preflight' "$root/start.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  printf '%s\n' \
    "pkgctl:cli-run-construction-only: start: expected status 0, got $status" \
    >&2
  dump_file 'start stdout' "$root/start.out"
  dump_file 'start stderr' "$root/start.err"
  exit 1
}

mkdir "$runtime" "$build"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  content \
  construction-sessions \
  package-outputs \
  artifacts \
  check-temporary; do
  mkdir "$runtime/$directory"
done
"$root_view_fixture" "$build"

initial_state=$($state_inspect_fixture "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

run_nonce=$(printf '%064d' 6)
capture_start_or_skip "$run_nonce"
require_contains start "$root/start.out" 'origin admitted'
require_contains start "$root/start.out" 'disposition completed'
require_contains start "$root/start.out" 'durable-steps 1'
require_contains start "$root/start.out" 'complete yes'
require_contains start "$root/start.out" 'failed no'

require_absent lifecycle-root "$lifecycle"
require_absent target-root "$target"
for directory in \
  target-locks \
  application-journals \
  payload \
  capture \
  rejected \
  completed \
  effect-bodies \
  lifecycle-sessions; do
  require_absent "operation-runtime-$directory" "$runtime/$directory"
done

final_state=$($state_inspect_fixture "$state")
if [ "$final_state" != "$initial_state" ]; then
  fail 'construction-only run changed canonical target state'
fi

artifact_count=$(find "$runtime/artifacts" -type f -name '*.tar' | wc -l)
[ "$artifact_count" -eq 1 ] || \
  fail "construction-only run retained $artifact_count package archives, expected 1"

rm -rf "$collection"
interpreter_override=/bin/false
set +e
run_command --resume "$run_nonce" >"$root/resume.out" 2>"$root/resume.err"
resume_status=$?
set -e
unset interpreter_override
[ "$resume_status" -eq 0 ] || {
  dump_file 'resume stdout' "$root/resume.out"
  dump_file 'resume stderr' "$root/resume.err"
  fail "resume: expected status 0, got $resume_status"
}
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'durable-steps 0'
require_absent resumed-lifecycle-root "$lifecycle"
require_absent resumed-target-root "$target"
