#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
run_evidence_inspect_fixture=$4
runtime_root_fixture=$5
rootfs_audit_fixture=$6
fixture_collection=$7
root_view_fixture=$8
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-rootfs-campaign.XXXXXX")
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
  printf 'pkgctl:cli-run-rootfs-campaign: %s\n' "$*" >&2
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
      "pkgctl:cli-run-rootfs-campaign: $label: missing expected text: $expected" \
      >&2
    dump_file "$label" "$file"
    exit 1
  fi
}

require_not_contains()
{
  label=$1
  file=$2
  forbidden=$3
  if grep -F -- "$forbidden" "$file" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-rootfs-campaign: $label: contains forbidden text: $forbidden" \
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
  [ "$actual" = "$expected" ] || \
    fail "$label: expected '$expected', got '$actual'"
}

require_absent()
{
  label=$1
  path=$2
  [ ! -e "$path" ] || fail "$label: unexpected path exists: $path"
}

# The production provider separately qualifies pkgstate-init.  This campaign
# uses the pkgctl state fixture only as a test harness around the same explicit
# canonical_generation_store open-or-initialize authority, so pkgctl tests do
# not depend on optional installation of another project's reference client.
require_absent initial-canonical-store "$state"
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
build_interpreter=$("$runtime_root_fixture" "$build" /bin/sh)
lifecycle_interpreter=$("$runtime_root_fixture" "$lifecycle" /bin/sh)
require_equal interpreter-authority "$build_interpreter" "$lifecycle_interpreter"
case $build_interpreter in
  /*)
    ;;
  *)
    fail "runtime fixture returned non-absolute interpreter: $build_interpreter"
    ;;
esac

if find "$target" -mindepth 1 -print -quit | grep . >/dev/null; then
  fail 'managed target is not empty before the rootfs campaign'
fi

set -- run --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'build=@rootfs-test' \
  --goal 'run=@rootfs-test' \
  --goal 'check=rootfs-probe' \
  --converge-exact \
  --start "$(printf '%064d' 6)" \
  --build-parallelism 1 \
  --build-source-date-epoch 0 \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --lifecycle-root "$lifecycle" \
  --target-root "$target" \
  --interpreter "$build_interpreter" \
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
      'pkgctl:cli-run-rootfs-campaign: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native rootfs campaign'
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

final_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 3'
require_contains final-state "$root/final-state.out" 'package base-files 1.0-1'
require_contains final-state "$root/final-state.out" 'package runtime-lib 1.0-1'
require_contains final-state "$root/final-state.out" 'package rootfs-probe 1.0-1'
require_not_contains final-state "$root/final-state.out" 'package build-tool '
package_count=$(grep -c '^package ' "$root/final-state.out")
[ "$package_count" -eq 3 ] || \
  fail "canonical state contains $package_count package records, expected 3"

require_equal base-files-target base-files-source \
  "$(cat "$target/base-files-marker")"
require_equal runtime-dependency-target runtime-lib-source \
  "$(cat "$target/runtime-lib-marker")"
require_equal rootfs-probe-target rootfs-probe-source+build-tool-source \
  "$(cat "$target/rootfs-probe-marker")"
require_absent build-only-target "$target/build-tool-token"

artifact_list=$root/artifacts.list
find "$runtime/artifacts" -type f -name '*.tar' | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 4 ] || \
  fail "rootfs campaign retained $artifact_count package archives, expected 4"

build_tool_archive=
base_files_archive=
runtime_lib_archive=
rootfs_probe_archive=
while IFS= read -r archive; do
  if tar -tf "$archive" | grep -Fx -- 'build-tool-token' >/dev/null; then
    [ -z "$build_tool_archive" ] || fail 'multiple build-tool archives found'
    build_tool_archive=$archive
  fi
  if tar -tf "$archive" | grep -Fx -- 'base-files-marker' >/dev/null; then
    [ -z "$base_files_archive" ] || fail 'multiple base-files archives found'
    base_files_archive=$archive
  fi
  if tar -tf "$archive" | grep -Fx -- 'runtime-lib-marker' >/dev/null; then
    [ -z "$runtime_lib_archive" ] || fail 'multiple runtime-lib archives found'
    runtime_lib_archive=$archive
  fi
  if tar -tf "$archive" | grep -Fx -- 'rootfs-probe-marker' >/dev/null; then
    [ -z "$rootfs_probe_archive" ] || fail 'multiple rootfs-probe archives found'
    rootfs_probe_archive=$archive
  fi
done <"$artifact_list"

[ -n "$build_tool_archive" ] || fail 'build-tool construction archive is absent'
[ -n "$base_files_archive" ] || fail 'base-files construction archive is absent'
[ -n "$runtime_lib_archive" ] || fail 'runtime-lib construction archive is absent'
[ -n "$rootfs_probe_archive" ] || fail 'rootfs-probe construction archive is absent'
require_equal build-tool-archive build-tool-source \
  "$(tar -xOf "$build_tool_archive" build-tool-token)"
require_equal base-files-archive base-files-source \
  "$(tar -xOf "$base_files_archive" base-files-marker)"
require_equal runtime-lib-archive runtime-lib-source \
  "$(tar -xOf "$runtime_lib_archive" runtime-lib-marker)"
require_equal rootfs-probe-archive rootfs-probe-source+build-tool-source \
  "$(tar -xOf "$rootfs_probe_archive" rootfs-probe-marker)"

journal=$(sed -n 's/^journal //p' "$root/run.out")
[ -n "$journal" ] || fail 'terminal report did not expose journal identity'
"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" >"$root/terminal-evidence.out" || {
  dump_file 'terminal durable evidence' "$root/terminal-evidence.out"
  fail 'terminal durable construction/check evidence is unavailable after cleanup'
}
require_contains terminal-evidence "$root/terminal-evidence.out" 'complete yes'
require_contains terminal-evidence "$root/terminal-evidence.out" 'failed no'
require_contains terminal-evidence "$root/terminal-evidence.out" 'stopped no'
require_contains terminal-evidence "$root/terminal-evidence.out" 'constructions 4'
require_contains terminal-evidence "$root/terminal-evidence.out" 'checks 1'
require_contains terminal-evidence "$root/terminal-evidence.out" \
  'construction-evidence 4'
require_contains terminal-evidence "$root/terminal-evidence.out" \
  'check-evidence 1'

for directory in construction-sessions package-outputs check-resources check-temporary; do
  if [ -d "$runtime/$directory" ] && \
      find "$runtime/$directory" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "terminal cleanup retained private realization under $directory"
  fi
done

set +e
"$rootfs_audit_fixture" "$state" "$target" >"$root/audit-clean.out" 2>"$root/audit-clean.err"
audit_status=$?
set -e
if [ "$audit_status" -ne 0 ]; then
  dump_file 'clean audit stdout' "$root/audit-clean.out"
  dump_file 'clean audit stderr' "$root/audit-clean.err"
  fail "clean rootfs audit: expected status 0, got $audit_status"
fi
require_contains clean-audit "$root/audit-clean.out" 'complete yes'
require_contains clean-audit "$root/audit-clean.out" 'packages 3'
require_contains clean-audit "$root/audit-clean.out" 'findings 0'
require_contains clean-audit "$root/audit-clean.out" 'failures 0'

rm "$target/runtime-lib-marker"
set +e
"$rootfs_audit_fixture" "$state" "$target" >"$root/audit-drift.out" 2>"$root/audit-drift.err"
audit_status=$?
set -e
[ "$audit_status" -eq 1 ] || {
  dump_file 'drift audit stdout' "$root/audit-drift.out"
  dump_file 'drift audit stderr' "$root/audit-drift.err"
  fail "drift rootfs audit: expected status 1, got $audit_status"
}
require_contains drift-audit "$root/audit-drift.out" 'complete yes'
require_contains drift-audit "$root/audit-drift.out" 'findings 1'
require_contains drift-audit "$root/audit-drift.out" \
  'finding missing-object runtime-lib runtime-lib-marker'
require_contains drift-audit "$root/audit-drift.out" 'failures 0'
