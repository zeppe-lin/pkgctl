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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-removal.XXXXXX")
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
  printf 'pkgctl:cli-run-removal: %s\n' "$*" >&2
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
      "pkgctl:cli-run-removal: $label: missing expected text: $expected" >&2
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
    printf '%s\n' "pkgctl:cli-run-removal: $label: values differ" >&2
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
  goal=$1
  convergence=$2
  intent=$3
  nonce=$4
  maximum_steps=$5
  set -- run --canonical-store "$state"
  if [ "$intent" = --start ]; then
    set -- "$@" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture x86_64 \
      --goal "$goal"
    if [ "$convergence" = exact ]; then
      set -- "$@" --converge-exact
    fi
    set -- "$@" \
      --build-parallelism 1 \
      --build-source-date-epoch 0
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
    # shellcheck disable=SC2086
    "$pkgctl" "$@" $binding
  else
    "$pkgctl" "$@"
  fi
}

capture_run()
{
  name=$1
  expected_status=$2
  goal=$3
  convergence=$4
  intent=$5
  nonce=$6
  maximum_steps=$7
  stdout_file=$root/$name.out
  stderr_file=$root/$name.err
  set +e
  run_command "$goal" "$convergence" "$intent" "$nonce" "$maximum_steps" \
    >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  if [ "$status" -ne "$expected_status" ]; then
    printf '%s\n' \
      "pkgctl:cli-run-removal: $name: expected status $expected_status, got $status" \
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

install_nonce=$(printf '%064d' 2)
set +e
run_command 'run=@base' preserve --start "$install_nonce" 8 \
  >"$root/install.out" 2>"$root/install.err"
install_status=$?
set -e
if [ "$install_status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/install.err" >/dev/null; then
    require_equal unavailable-state "$initial_state" \
      "$($state_inspect_fixture "$state")"
    require_absent unavailable-target "$target/usr/bin/pkgctl-fixture"
    printf '%s\n' \
      'pkgctl:cli-run-removal: native execution preflight is unavailable in this process context' \
      >&2
    dump_file 'native execution preflight' "$root/install.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
    fi
    exit 77
  fi
  printf '%s\n' \
    "pkgctl:cli-run-removal: install: expected status 0, got $install_status" >&2
  dump_file 'install stdout' "$root/install.out"
  dump_file 'install stderr' "$root/install.err"
  exit 1
fi
require_contains install "$root/install.out" 'disposition completed'
require_contains install "$root/install.out" 'complete yes'
require_contains install "$root/install.out" 'failed no'
require_executable installed-target "$target/usr/bin/pkgctl-fixture"
installed_state=$($state_inspect_fixture "$state")
printf '%s\n' "$installed_state" >"$root/installed-state.out"
require_contains installed-state "$root/installed-state.out" 'packages 1'
require_contains installed-state "$root/installed-state.out" 'package fixture 1.0-1'

# Before execution, prove that exact convergence turns the installed package
# into an explicit removal action while the requested package is build-only.
set -- transaction \
  --collection "core=$collection" \
  --canonical-store "$state" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'build=fixture' \
  --converge-exact
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/removal-transaction.out"
require_contains removal-transaction "$root/removal-transaction.out" \
  'transaction.convergence=converge-exact'
planned_transaction=$(sed -n 's/^session.identity=//p' \
  "$root/removal-transaction.out")
[ "${#planned_transaction}" -eq 64 ] || \
  fail "planned transaction identity has length ${#planned_transaction}, expected 64"
remove_count=$(sed -n 's/^node\.\([0-9][0-9]*\)\.action=remove$/\1/p' \
  "$root/removal-transaction.out" | wc -l | tr -d ' ')
[ "$remove_count" -eq 1 ] || {
  dump_file removal-transaction "$root/removal-transaction.out"
  fail "removal transaction contains $remove_count remove actions, expected 1"
}
remove_index=$(sed -n 's/^node\.\([0-9][0-9]*\)\.action=remove$/\1/p' \
  "$root/removal-transaction.out")
remove_node=$(sed -n \
  "s/^node\.$remove_index\.identity=//p" "$root/removal-transaction.out")
[ "${#remove_node}" -eq 64 ] || \
  fail "remove node identity has length ${#remove_node}, expected 64"

removal_nonce=$(printf '%064d' 3)
capture_run removal-start 0 'build=fixture' exact --start "$removal_nonce" 8
require_contains removal-start "$root/removal-start.out" 'origin admitted'
require_contains removal-start "$root/removal-start.out" 'disposition completed'
require_contains removal-start "$root/removal-start.out" 'complete yes'
require_contains removal-start "$root/removal-start.out" 'failed no'
removal_transaction=$(sed -n 's/^transaction //p' "$root/removal-start.out")
removal_journal=$(sed -n 's/^journal //p' "$root/removal-start.out")
[ "${#removal_transaction}" -eq 64 ] || \
  fail "removal-start: transaction identity has length ${#removal_transaction}, expected 64"
require_equal removal-transaction-identity "$planned_transaction" \
  "$removal_transaction"
[ "${#removal_journal}" -eq 64 ] || \
  fail "removal-start: journal identity has length ${#removal_journal}, expected 64"

require_absent removed-target "$target/usr/bin/pkgctl-fixture"
removed_state=$($state_inspect_fixture "$state")
printf '%s\n' "$removed_state" >"$root/removed-state.out"
require_contains removed-state "$root/removed-state.out" 'packages 0'

inspection=$($pkgctl inspect-run --run-store "$runtime/run" \
  --journal "$removal_journal")
printf '%s\n' "$inspection" >"$root/removal-inspection.out"
require_contains removal-inspection "$root/removal-inspection.out" \
  'run.complete=true'
require_contains removal-inspection "$root/removal-inspection.out" \
  'run.failed=false'
operation_count=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/removal-inspection.out" | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "removal run contains $operation_count operation dispatches, expected 1"
operation_index=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/removal-inspection.out")
operation_primary=$(sed -n \
  "s/^dispatch\.$operation_index\.primary-node=//p" \
  "$root/removal-inspection.out")
require_equal removal-dispatch-node "$remove_node" "$operation_primary"
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" \
  "$root/removal-inspection.out")
[ "${#effect}" -eq 64 ] || \
  fail "removal effect identity has length ${#effect}, expected 64"

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/removal-effect.out"
require_contains removal-effect "$root/removal-effect.out" 'effect.stage=terminal'
require_contains removal-effect "$root/removal-effect.out" \
  'effect.application-outcome=completed'
require_contains removal-effect "$root/removal-effect.out" \
  'effect.application-completed-evidence='
require_contains removal-effect "$root/removal-effect.out" \
  'effect.publication-outcome=published'
require_contains removal-effect "$root/removal-effect.out" \
  'effect.terminal-outcome=completed'

state_before_repeat=$($state_inspect_fixture "$state")
# A completed run remains authority after the mutable source collection is gone.
rm -rf "$collection"
capture_run repeated-removal-resume 0 'build=fixture' exact --resume \
  "$removal_nonce" 4
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  "transaction $removal_transaction"
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  "journal $removal_journal"
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  'origin resumed'
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  'disposition completed'
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  'durable-steps 0'
require_contains repeated-removal-resume "$root/repeated-removal-resume.out" \
  'complete yes'
require_absent repeated-removal-target "$target/usr/bin/pkgctl-fixture"
require_equal repeated-removal-state "$state_before_repeat" \
  "$($state_inspect_fixture "$state")"
