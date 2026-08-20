#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
run_evidence_inspect_fixture=$4
runtime_root_fixture=$5
state_ownership_inspect_fixture=$6
rootfs_audit_fixture=$7
fixture_collection=$8
root_view_fixture=$9
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-shared-ownership-single-owner.XXXXXX")
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

fail()
{
  printf 'pkgctl:cli-run-shared-ownership-single-owner: %s\n' "$*" >&2
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
      "pkgctl:cli-run-shared-ownership-single-owner: $label: missing expected text: $expected" >&2
    dump_file "$label" "$file"
    exit 1
  fi
}

require_equal()
{
  label=$1
  expected=$2
  actual=$3
  [ "$actual" = "$expected" ] || \
    fail "$label: expected '$expected', got '$actual'"
}

field()
{
  prefix=$1
  file=$2
  value=$(sed -n "s/^$prefix//p" "$file")
  [ -n "$value" ] || fail "$file omits field prefix $prefix"
  lines=$(printf '%s\n' "$value" | wc -l)
  [ "$lines" -eq 1 ] || fail "$file repeats field prefix $prefix"
  printf '%s\n' "$value"
}

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

set -- run --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'run=base-files' \
  --start "$(printf '%064d' 41)" \
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
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --lifecycle-user-id "$uid" \
  --lifecycle-group-id "$gid" \
  --max-steps 8
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
    set -- "$@" --lifecycle-supplementary-group "$group"
  fi
done

set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/run.out" 2>"$root/run.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/run.err" >/dev/null; then
    require_equal unavailable-state "$initial_state" \
      "$("$state_inspect_fixture" "$state")"
    if find "$target" -mindepth 1 -print -quit | grep . >/dev/null; then
      fail 'native preflight refusal mutated the empty managed target'
    fi
    printf '%s\n' \
      'pkgctl:cli-run-shared-ownership-single-owner: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the single-owner operation path'
    fi
    exit 77
  fi
  dump_file 'run stdout' "$root/run.out"
  dump_file 'run stderr' "$root/run.err"
  fail "run: expected status 0, got $status"
fi

require_contains run "$root/run.out" 'origin admitted'
require_contains run "$root/run.out" 'disposition completed'
require_contains run "$root/run.out" 'complete yes'
require_contains run "$root/run.out" 'failed no'
require_contains run "$root/run.out" 'artifacts 1'
require_contains run "$root/run.out" 'artifact.0.package base-files'

journal=$(sed -n 's/^journal //p' "$root/run.out")
[ "${#journal}" -eq 64 ] || fail 'terminal run omits canonical journal identity'
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/run-inspection.out" 2>"$root/run-inspection.err" || {
  dump_file 'run inspection stdout' "$root/run-inspection.out"
  dump_file 'run inspection stderr' "$root/run-inspection.err"
  fail 'terminal run inspection failed'
}
require_contains run-inspection "$root/run-inspection.out" 'run.complete=true'
require_contains run-inspection "$root/run-inspection.out" 'run.failed=false'
operation_index=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/run-inspection.out")
[ -n "$operation_index" ] || fail 'terminal run has no operation dispatch'
operation_lines=$(printf '%s\n' "$operation_index" | wc -l)
[ "$operation_lines" -eq 1 ] || fail 'terminal run has multiple operation dispatches'
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" \
  "$root/run-inspection.out")
[ "${#effect}" -eq 64 ] || fail 'operation dispatch omits effect-attempt identity'

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/effect.out" 2>"$root/effect.err" || {
  dump_file 'effect inspection stdout' "$root/effect.out"
  dump_file 'effect inspection stderr' "$root/effect.err"
  fail 'terminal effect inspection failed'
}
require_contains effect "$root/effect.out" 'effect.stage=terminal'
require_contains effect "$root/effect.out" 'effect.application-outcome=completed'
require_contains effect "$root/effect.out" 'effect.application-completed-evidence='
require_contains effect "$root/effect.out" 'effect.transaction-evidence='
require_contains effect "$root/effect.out" 'effect.publication-request='
require_contains effect "$root/effect.out" 'effect.publication-outcome=published'
require_contains effect "$root/effect.out" 'effect.publication-resulting-snapshot='
require_contains effect "$root/effect.out" 'effect.terminal-outcome=completed'
application_evidence=$(field 'effect.application-completed-evidence=' "$root/effect.out")
transaction_evidence=$(field 'effect.transaction-evidence=' "$root/effect.out")
resulting_snapshot=$(field 'effect.publication-resulting-snapshot=' "$root/effect.out")

"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/ownership.out" 2>"$root/ownership.err" || {
  dump_file 'ownership inspection stdout' "$root/ownership.out"
  dump_file 'ownership inspection stderr' "$root/ownership.err"
  fail 'canonical state does not contain the expected sole-owner marker'
}
require_contains ownership "$root/ownership.out" 'packages 1'
require_contains ownership "$root/ownership.out" 'package base-files 1.0-1'
require_contains ownership "$root/ownership.out" 'path usr/lib/shared-ownership-marker'
require_contains ownership "$root/ownership.out" 'kind regular'
require_contains ownership "$root/ownership.out" 'origin incoming-payload'
require_contains ownership "$root/ownership.out" 'mode 0644'
require_contains ownership "$root/ownership.out" 'mtime 0'
require_contains ownership "$root/ownership.out" 'mtime-nanoseconds 0'
require_contains ownership "$root/ownership.out" 'size 17'
require_contains ownership "$root/ownership.out" \
  'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
require_contains ownership "$root/ownership.out" 'owners 1'
require_contains ownership "$root/ownership.out" 'owner.0 base-files '
manifest=$(field 'manifest ' "$root/ownership.out")
[ "$manifest" -gt 0 ] || fail 'installed package published an empty manifest'
state_snapshot=$(field 'snapshot ' "$root/ownership.out")
state_application=$(field 'application-evidence ' "$root/ownership.out")
state_transaction=$(field 'transaction-evidence ' "$root/ownership.out")
state_mode=$(field 'mode ' "$root/ownership.out")
state_uid=$(field 'uid ' "$root/ownership.out")
state_gid=$(field 'gid ' "$root/ownership.out")
state_size=$(field 'size ' "$root/ownership.out")
state_mtime=$(field 'mtime ' "$root/ownership.out")
operation_plan=$(field 'operation-plan ' "$root/ownership.out")
case $operation_plan in
  v1:sha256:*) ;;
  *) fail "installed state carries malformed operation-plan identity: $operation_plan" ;;
esac
[ "${#operation_plan}" -eq 74 ] || \
  fail "installed operation-plan identity has length ${#operation_plan}, expected 74"
printf '%s\n' "${operation_plan#v1:sha256:}" | \
  grep -E '^[0-9a-f]{64}$' >/dev/null || \
  fail "installed state carries non-canonical operation-plan digest: $operation_plan"
require_equal publication-snapshot "$resulting_snapshot" "$state_snapshot"
require_equal completed-application-binding "$application_evidence" "$state_application"
require_equal transaction-evidence-binding "$transaction_evidence" "$state_transaction"

[ -f "$target/usr/lib/shared-ownership-marker" ] || \
  fail 'application completed without the qualified marker in the managed target'
require_equal target-payload shared-authority \
  "$(cat "$target/usr/lib/shared-ownership-marker")"
require_equal target-sha256 \
  6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06 \
  "$(sha256sum "$target/usr/lib/shared-ownership-marker" | awk '{print $1}')"
require_equal target-metadata "${state_mode#0} $state_uid $state_gid $state_size" \
  "$(stat -c '%a %u %g %s' "$target/usr/lib/shared-ownership-marker")"
require_equal target-mtime "$state_mtime" \
  "$(stat -c '%Y' "$target/usr/lib/shared-ownership-marker")"

"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" \
  >"$root/terminal-evidence.out" 2>"$root/terminal-evidence.err" || {
  dump_file 'terminal evidence stdout' "$root/terminal-evidence.out"
  dump_file 'terminal evidence stderr' "$root/terminal-evidence.err"
  fail 'terminal construction evidence is unavailable'
}
require_contains terminal-evidence "$root/terminal-evidence.out" 'complete yes'
require_contains terminal-evidence "$root/terminal-evidence.out" 'failed no'
require_contains terminal-evidence "$root/terminal-evidence.out" 'constructions 1'
require_contains terminal-evidence "$root/terminal-evidence.out" \
  'construction-evidence 1'

set +e
"$rootfs_audit_fixture" "$state" "$target" \
  >"$root/audit.out" 2>"$root/audit.err"
audit_status=$?
set -e
[ "$audit_status" -eq 0 ] || {
  dump_file 'audit stdout' "$root/audit.out"
  dump_file 'audit stderr' "$root/audit.err"
  fail "independent target audit: expected status 0, got $audit_status"
}
require_contains audit "$root/audit.out" 'complete yes'
require_contains audit "$root/audit.out" 'packages 1'
require_contains audit "$root/audit.out" 'findings 0'
require_contains audit "$root/audit.out" 'failures 0'
