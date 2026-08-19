#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
state_ownership_inspect_fixture=$5
interrupt_fixture=$6
rootfs_audit_fixture=$7
fixture_collection=$8
root_view_fixture=$9
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-shared-ownership-restart.XXXXXX")
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
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)
run_nonce=$(printf '%064d' 81)

fail()
{
  printf 'pkgctl:cli-run-shared-ownership-restart: %s\n' "$*" >&2
  exit 1
}

dump_file()
(
  printf '%s\n' "--- $1 ---" >&2
  if [ -s "$2" ]; then
    cat "$2" >&2
  else
    printf '%s\n' '<empty>' >&2
  fi
)

require_contains()
{
  if ! grep -F -- "$3" "$2" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-shared-ownership-restart: $1: missing expected text: $3" >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_not_contains()
{
  if grep -F -- "$3" "$2" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-shared-ownership-restart: $1: contains forbidden text: $3" >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_equal()
{
  [ "$2" = "$3" ] || fail "$1: expected '$2', got '$3'"
}

field()
(
  value=$(sed -n "s/^$1//p" "$2")
  [ -n "$value" ] || fail "$2 omits field prefix $1"
  lines=$(printf '%s\n' "$value" | wc -l | tr -d ' ')
  [ "$lines" -eq 1 ] || fail "$2 repeats field prefix $1"
  printf '%s\n' "$value"
)

single_file()
(
  directory=$1
  pattern=$2
  set -- "$directory"/$pattern
  [ "$#" -eq 1 ] && [ -e "$1" ] || \
    fail "expected exactly one path matching $directory/$pattern"
  printf '%s\n' "$1"
)

target_fingerprint()
(
  output=$1
  cd "$target"
  {
    find . ! -name . -exec stat -c 'meta %n|%F|%f|%u|%g|%s|%y|%z' {} \;
    find . -type f -exec sha256sum {} \; | sed 's/^/sha256 /'
  } | LC_ALL=C sort >"$output"
)

binding=$("$state_fixture" "$state")
initial_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

mkdir "$runtime" "$build" "$lifecycle" "$target"
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
  check-resources \
  check-temporary \
  lifecycle-sessions; do
  mkdir "$runtime/$directory"
done

"$root_view_fixture" "$build"
"$root_view_fixture" "$lifecycle"
mkdir_program=$(command -v mkdir) || fail 'host mkdir is unavailable for runtime fixture'
case $mkdir_program in
  /*) ;;
  *) fail "host mkdir did not resolve to an absolute path: $mkdir_program" ;;
esac
interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$mkdir_program")
lifecycle_interpreter=$("$runtime_root_fixture" "$lifecycle" /bin/sh)
require_equal interpreter-authority "$interpreter" "$lifecycle_interpreter"
case $interpreter in
  /*) ;;
  *) fail "runtime fixture returned non-absolute interpreter: $interpreter" ;;
esac

run_interrupted_start()
{
  set -- run --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal 'run=runtime-lib' \
    --start "$run_nonce" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy exact-compatible-sharing \
    --build-root-view "$(printf '%064d' 81)" \
    --lifecycle-root-view "$(printf '%064d' 82)" \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --lifecycle-root "$lifecycle" \
    --target-root "$target" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --lifecycle-user-id "$uid" \
    --lifecycle-group-id "$gid" \
    --max-steps 16
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --build-supplementary-group "$group"
      set -- "$@" --lifecycle-supplementary-group "$group"
    fi
  done
  # shellcheck disable=SC2086
  "$interrupt_fixture" "$state" "$runtime/effects" -- "$pkgctl" "$@" $binding
}

run_resume()
{
  set -- run --canonical-store "$state" \
    --resume "$run_nonce" \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --lifecycle-root "$lifecycle" \
    --target-root "$target" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --lifecycle-user-id "$uid" \
    --lifecycle-group-id "$gid" \
    --max-steps 16
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --build-supplementary-group "$group"
      set -- "$@" --lifecycle-supplementary-group "$group"
    fi
  done
  "$pkgctl" "$@"
}

run_redeclared_resume()
{
  set -- run --canonical-store "$state" \
    --resume "$run_nonce" \
    --operation-policy strict-exclusive \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --lifecycle-root "$lifecycle" \
    --target-root "$target" \
    --interpreter "$interpreter" \
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
  "$pkgctl" "$@"
}

# The run dependency makes base-files completion an ordering prerequisite for
# runtime-lib completion. Kill the first target publication only after its
# durable publication-terminal effect head exists.
set +e
run_interrupted_start >"$root/interrupted.out" 2>"$root/interrupted.err"
interrupt_status=$?
set -e
if [ "$interrupt_status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/interrupted.err" >/dev/null; then
    require_equal unavailable-state "$initial_state" \
      "$("$state_inspect_fixture" "$state")"
    [ ! -e "$target/base-files-marker" ] || \
      fail 'native-unavailable start mutated base-files target payload'
    [ ! -e "$target/runtime-lib-marker" ] || \
      fail 'native-unavailable start mutated runtime-lib target payload'
    printf '%s\n' \
      'pkgctl:cli-run-shared-ownership-restart: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the shared-ownership restart path'
    fi
    exit 77
  fi
  if [ "$interrupt_status" -eq 77 ] && \
      grep -F 'ptrace unavailable:' "$root/interrupted.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-run-shared-ownership-restart: publication-terminal interruption is unavailable in this process context' >&2
    dump_file 'publication-terminal interruption' "$root/interrupted.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the shared-ownership restart cut'
    fi
    exit 77
  fi
  dump_file 'interrupted start stdout' "$root/interrupted.out"
  dump_file 'interrupted start stderr' "$root/interrupted.err"
  fail "interrupted start: expected status 0, got $interrupt_status"
fi

"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/base-before.out" 2>"$root/base-before.err" || {
  dump_file 'base ownership at restart cut stdout' "$root/base-before.out"
  dump_file 'base ownership at restart cut stderr' "$root/base-before.err"
  fail 'restart cut did not publish the first owner'
}
require_contains base-before "$root/base-before.out" 'packages 1'
require_contains base-before "$root/base-before.out" 'package base-files 1.0-1'
require_contains base-before "$root/base-before.out" 'origin incoming-payload'
require_contains base-before "$root/base-before.out" 'owners 1'
require_contains base-before "$root/base-before.out" 'owner.0 base-files '
require_contains base-before "$root/base-before.out" 'mode 0644'
require_contains base-before "$root/base-before.out" 'size 17'
require_contains base-before "$root/base-before.out" \
  'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
base_manifest=$(field 'manifest ' "$root/base-before.out")
[ "$base_manifest" -gt 0 ] || fail 'restart cut published an empty first-owner manifest'
base_package_identity=$(field 'package-identity ' "$root/base-before.out")
base_operation_plan=$(field 'operation-plan ' "$root/base-before.out")
base_application_evidence=$(field 'application-evidence ' "$root/base-before.out")
base_transaction_evidence=$(field 'transaction-evidence ' "$root/base-before.out")
base_snapshot=$(field 'snapshot ' "$root/base-before.out")

interrupted_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$interrupted_state" >"$root/interrupted-state.out"
require_contains interrupted-state "$root/interrupted-state.out" 'packages 1'
require_contains interrupted-state "$root/interrupted-state.out" 'package base-files 1.0-1'
require_not_contains interrupted-state "$root/interrupted-state.out" 'package runtime-lib 1.0-1'
[ -f "$target/base-files-marker" ] || fail 'restart cut lacks first-owner unique payload'
[ -f "$target/usr/lib/shared-ownership-marker" ] || fail 'restart cut lacks shared object'
[ ! -e "$target/runtime-lib-marker" ] || fail 'second owner mutated target before restart cut'
require_equal shared-marker-before shared-authority \
  "$(cat "$target/usr/lib/shared-ownership-marker")"
marker_fingerprint_before=$(stat -c '%d:%i:%f:%u:%g:%s:%y:%z' \
  "$target/usr/lib/shared-ownership-marker")
target_fingerprint "$root/target-before-redeclaration.out"

run_head=$(single_file "$runtime/run" '*.pjh')
journal=$(basename "$run_head" .pjh)
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/interrupted-run.out" 2>"$root/interrupted-run.err" || {
  dump_file 'interrupted run inspection stdout' "$root/interrupted-run.out"
  dump_file 'interrupted run inspection stderr' "$root/interrupted-run.err"
  fail 'restart cut run inspection failed'
}
require_contains interrupted-run "$root/interrupted-run.out" 'run.complete=false'
require_contains interrupted-run "$root/interrupted-run.out" 'run.failed=false'
require_contains interrupted-run "$root/interrupted-run.out" 'run.disposition=active'
operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/interrupted-run.out")
[ -n "$operation_indices" ] || fail 'restart cut lacks an operation dispatch'
operation_count=$(printf '%s\n' "$operation_indices" | wc -l | tr -d ' ')
[ "$operation_count" -eq 1 ] || \
  fail "restart cut expected one admitted operation, got $operation_count"
operation_index=$operation_indices
require_contains interrupted-operation "$root/interrupted-run.out" \
  "dispatch.$operation_index.state=started"
first_effect=$(field "dispatch.$operation_index.effect-attempt=" \
  "$root/interrupted-run.out")
[ "${#first_effect}" -eq 64 ] || fail 'restart cut omits exact first effect attempt'

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$first_effect" \
  >"$root/interrupted-effect.out" 2>"$root/interrupted-effect.err" || {
  dump_file 'interrupted effect inspection stdout' "$root/interrupted-effect.out"
  dump_file 'interrupted effect inspection stderr' "$root/interrupted-effect.err"
  fail 'restart cut effect inspection failed'
}
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.stage=publication-terminal'
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.disposition=seal-terminal'
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.application-outcome=completed'
require_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.publication-outcome=published'
require_not_contains interrupted-effect "$root/interrupted-effect.out" \
  'effect.terminal-outcome='
first_application_evidence=$(field 'effect.application-completed-evidence=' \
  "$root/interrupted-effect.out")
first_transaction_evidence=$(field 'effect.transaction-evidence=' \
  "$root/interrupted-effect.out")
first_resulting_snapshot=$(field 'effect.publication-resulting-snapshot=' \
  "$root/interrupted-effect.out")
first_publication_receipt=$(field 'effect.publication-receipt=' \
  "$root/interrupted-effect.out")
require_equal first-owner-application-binding \
  "$base_application_evidence" "$first_application_evidence"
require_equal first-owner-transaction-binding \
  "$base_transaction_evidence" "$first_transaction_evidence"
require_equal first-owner-snapshot-binding \
  "$base_snapshot" "$first_resulting_snapshot"

# A fresh resume is not allowed to redeclare current CLI policy. This refusal
# must occur without changing the already-published first-owner authority.
set +e
run_redeclared_resume >"$root/redeclared.out" 2>"$root/redeclared.err"
redeclared_status=$?
set -e
[ "$redeclared_status" -eq 2 ] || {
  dump_file 'policy redeclaration stdout' "$root/redeclared.out"
  dump_file 'policy redeclaration stderr' "$root/redeclared.err"
  fail "policy redeclaration: expected usage status 2, got $redeclared_status"
}
require_contains policy-redeclaration "$root/redeclared.err" \
  '--resume uses retained transaction semantics'
require_equal policy-redeclaration-state "$interrupted_state" \
  "$("$state_inspect_fixture" "$state")"
"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/base-after-redeclaration.out" 2>"$root/base-after-redeclaration.err" || {
  dump_file 'base ownership after policy redeclaration stdout' \
    "$root/base-after-redeclaration.out"
  dump_file 'base ownership after policy redeclaration stderr' \
    "$root/base-after-redeclaration.err"
  fail 'policy redeclaration damaged first-owner authority'
}
require_equal policy-redeclaration-base-authority \
  "$(cat "$root/base-before.out")" \
  "$(cat "$root/base-after-redeclaration.out")"
target_fingerprint "$root/target-after-redeclaration.out"
cmp -s "$root/target-before-redeclaration.out" \
  "$root/target-after-redeclaration.out" || {
  diff -u "$root/target-before-redeclaration.out" \
    "$root/target-after-redeclaration.out" >&2 || :
  fail 'policy redeclaration mutated target authority'
}

# No catalog/goal/build-policy/operation-policy is redeclared here. The fresh
# process must decode the controller-owned policy retained at admission.
set +e
run_resume >"$root/resume.out" 2>"$root/resume.err"
resume_status=$?
set -e
[ "$resume_status" -eq 0 ] || {
  dump_file 'resume stdout' "$root/resume.out"
  dump_file 'resume stderr' "$root/resume.err"
  fail "retained-policy resume: expected status 0, got $resume_status"
}
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" "journal $journal"
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'

"$state_ownership_inspect_fixture" \
  "$state" runtime-lib usr/lib/shared-ownership-marker \
  >"$root/runtime-after.out" 2>"$root/runtime-after.err" || {
  dump_file 'runtime-lib ownership after resume stdout' "$root/runtime-after.out"
  dump_file 'runtime-lib ownership after resume stderr' "$root/runtime-after.err"
  fail 'retained-policy resume did not publish the compatible second owner'
}
require_contains runtime-after "$root/runtime-after.out" 'packages 2'
require_contains runtime-after "$root/runtime-after.out" 'package runtime-lib 1.0-1'
require_contains runtime-after "$root/runtime-after.out" 'origin retained-existing'
require_contains runtime-after "$root/runtime-after.out" 'owners 2'
require_contains runtime-after "$root/runtime-after.out" 'owner.0 base-files '
require_contains runtime-after "$root/runtime-after.out" 'owner.1 runtime-lib '
require_contains runtime-after "$root/runtime-after.out" 'mode 0644'
require_contains runtime-after "$root/runtime-after.out" 'size 17'
require_contains runtime-after "$root/runtime-after.out" \
  'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
runtime_manifest=$(field 'manifest ' "$root/runtime-after.out")
[ "$runtime_manifest" -gt 0 ] || fail 'resumed second owner published an empty manifest'

"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/base-after.out" 2>"$root/base-after.err" || {
  dump_file 'base ownership after resume stdout' "$root/base-after.out"
  dump_file 'base ownership after resume stderr' "$root/base-after.err"
  fail 'retained-policy resume lost the first owner'
}
require_contains base-after "$root/base-after.out" 'packages 2'
require_contains base-after "$root/base-after.out" 'origin incoming-payload'
require_contains base-after "$root/base-after.out" 'owners 2'
require_contains base-after "$root/base-after.out" 'owner.0 base-files '
require_contains base-after "$root/base-after.out" 'owner.1 runtime-lib '
require_equal base-package-stability "$base_package_identity" \
  "$(field 'package-identity ' "$root/base-after.out")"
require_equal base-plan-stability "$base_operation_plan" \
  "$(field 'operation-plan ' "$root/base-after.out")"
require_equal base-application-stability "$base_application_evidence" \
  "$(field 'application-evidence ' "$root/base-after.out")"
require_equal base-transaction-stability "$base_transaction_evidence" \
  "$(field 'transaction-evidence ' "$root/base-after.out")"

for semantic_field in 'mode ' 'uid ' 'gid ' 'mtime ' 'mtime-nanoseconds ' 'size ' 'content '; do
  require_equal "shared-object-${semantic_field% }" \
    "$(field "$semantic_field" "$root/base-after.out")" \
    "$(field "$semantic_field" "$root/runtime-after.out")"
done

marker_fingerprint_after=$(stat -c '%d:%i:%f:%u:%g:%s:%y:%z' \
  "$target/usr/lib/shared-ownership-marker")
require_equal retained-shared-path-not-rewritten \
  "$marker_fingerprint_before" "$marker_fingerprint_after"
require_equal retained-shared-payload shared-authority \
  "$(cat "$target/usr/lib/shared-ownership-marker")"
[ -f "$target/runtime-lib-marker" ] || \
  fail 'resumed second operation omitted its unique runtime-lib payload'
require_equal runtime-payload runtime-lib-source "$(cat "$target/runtime-lib-marker")"

"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/final-run.out" 2>"$root/final-run.err" || {
  dump_file 'final run inspection stdout' "$root/final-run.out"
  dump_file 'final run inspection stderr' "$root/final-run.err"
  fail 'terminal retained-policy run inspection failed'
}
require_contains final-run "$root/final-run.out" 'run.complete=true'
require_contains final-run "$root/final-run.out" 'run.failed=false'
final_operation_indices=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' "$root/final-run.out")
[ -n "$final_operation_indices" ] || fail 'terminal run has no operation dispatches'
final_operation_count=$(printf '%s\n' "$final_operation_indices" | wc -l | tr -d ' ')
[ "$final_operation_count" -eq 2 ] || \
  fail "terminal run expected two operation dispatches, got $final_operation_count"
for index in $final_operation_indices; do
  require_contains "final operation $index" "$root/final-run.out" \
    "dispatch.$index.state=completed"
done

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$first_effect" \
  >"$root/first-effect-after.out" 2>"$root/first-effect-after.err" || {
  dump_file 'first effect after resume stdout' "$root/first-effect-after.out"
  dump_file 'first effect after resume stderr' "$root/first-effect-after.err"
  fail 'resume did not preserve first-operation effect authority'
}
require_contains first-effect-after "$root/first-effect-after.out" 'effect.stage=terminal'
require_contains first-effect-after "$root/first-effect-after.out" \
  'effect.terminal-outcome=completed'
require_contains first-effect-after "$root/first-effect-after.out" \
  "effect.publication-receipt=$first_publication_receipt"

final_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 2'
require_contains final-state "$root/final-state.out" 'package base-files 1.0-1'
require_contains final-state "$root/final-state.out" 'package runtime-lib 1.0-1'

"$rootfs_audit_fixture" "$state" "$target" \
  >"$root/audit.out" 2>"$root/audit.err" || {
  dump_file 'audit stdout' "$root/audit.out"
  dump_file 'audit stderr' "$root/audit.err"
  fail 'retained-policy restart left canonical state/target inconsistent'
}
require_contains audit "$root/audit.out" 'complete yes'
require_contains audit "$root/audit.out" 'packages 2'
require_contains audit "$root/audit.out" 'findings 0'
require_contains audit "$root/audit.out" 'failures 0'
