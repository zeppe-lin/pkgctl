#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
state_ownership_inspect_fixture=$5
rootfs_audit_fixture=$6
fixture_collection=$7
root_view_fixture=$8
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-shared-ownership-compatible-owner.XXXXXX")
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
  printf 'pkgctl:cli-run-shared-ownership-compatible-owner: %s\n' "$*" >&2
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
      "pkgctl:cli-run-shared-ownership-compatible-owner: $label: missing expected text: $expected" >&2
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
      "pkgctl:cli-run-shared-ownership-compatible-owner: $label: contains forbidden text: $forbidden" >&2
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
  lines=$(printf '%s\n' "$value" | wc -l | tr -d ' ')
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

run_package()
{
  label=$1
  package=$2
  policy=$3
  nonce=$4

  set -- run --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal "run=$package" \
    --start "$(printf '%064d' "$nonce")" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy "$policy" \
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
  "$pkgctl" "$@" $binding >"$root/$label.out" 2>"$root/$label.err"
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$root/$label.err" >/dev/null; then
      printf '%s\n' \
        'pkgctl:cli-run-shared-ownership-compatible-owner: native execution preflight is unavailable;' \
        'privileged native execution is required for this case' >&2
      dump_file 'native execution preflight' "$root/$label.err"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the compatible-owner operation path'
      fi
      exit 77
    fi
    dump_file "$label stdout" "$root/$label.out"
    dump_file "$label stderr" "$root/$label.err"
    fail "$label: expected status 0, got $status"
  fi

  require_contains "$label" "$root/$label.out" 'origin admitted'
  require_contains "$label" "$root/$label.out" 'disposition completed'
  require_contains "$label" "$root/$label.out" 'complete yes'
  require_contains "$label" "$root/$label.out" 'failed no'
  require_contains "$label" "$root/$label.out" 'artifacts 1'
  require_contains "$label" "$root/$label.out" "artifact.0.package $package"
}

# Layer 2 is independently qualified elsewhere. Establish exactly that starting
# authority here, then stop observing until the compatible-owner transition.
run_package base base-files strict-exclusive 51
"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/base-before.out" 2>"$root/base-before.err" || {
  dump_file 'base ownership before sharing' "$root/base-before.out"
  dump_file 'base ownership before sharing stderr' "$root/base-before.err"
  fail 'compatible-owner case did not establish its qualified sole-owner precondition'
}
require_contains base-before "$root/base-before.out" 'packages 1'
require_contains base-before "$root/base-before.out" 'origin incoming-payload'
require_contains base-before "$root/base-before.out" 'owners 1'
require_contains base-before "$root/base-before.out" 'owner.0 base-files '
[ -f "$target/usr/lib/shared-ownership-marker" ] || \
  fail 'sole-owner precondition lacks the shared marker'
[ ! -e "$target/runtime-lib-marker" ] || \
  fail 'sole-owner precondition already contains runtime-lib payload'

base_package_identity=$(field 'package-identity ' "$root/base-before.out")
base_operation_plan=$(field 'operation-plan ' "$root/base-before.out")
base_application_evidence=$(field 'application-evidence ' "$root/base-before.out")
base_transaction_evidence=$(field 'transaction-evidence ' "$root/base-before.out")
marker_fingerprint_before=$(stat -c '%d:%i:%f:%u:%g:%s:%y:%z' \
  "$target/usr/lib/shared-ownership-marker")
marker_digest_before=$(sha256sum "$target/usr/lib/shared-ownership-marker" | awk '{print $1}')
marker_payload_before=$(cat "$target/usr/lib/shared-ownership-marker")
require_equal pre-share-payload shared-authority "$marker_payload_before"
require_equal pre-share-sha256 \
  6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06 \
  "$marker_digest_before"

# The second command admits the complete compatible-sharing policy. The shared
# path is already present with the exact qualified object authority from Layer 1.
run_package compatible runtime-lib exact-compatible-sharing 52
require_not_contains compatible "$root/compatible.out" 'artifact.0.package base-files'

journal=$(sed -n 's/^journal //p' "$root/compatible.out")
[ "${#journal}" -eq 64 ] || fail 'compatible-owner run omits canonical journal identity'
"$pkgctl" inspect-run --run-store "$runtime/run" --journal "$journal" \
  >"$root/compatible-run.out" 2>"$root/compatible-run.err" || {
  dump_file 'compatible run inspection stdout' "$root/compatible-run.out"
  dump_file 'compatible run inspection stderr' "$root/compatible-run.err"
  fail 'compatible-owner terminal run inspection failed'
}
require_contains compatible-run "$root/compatible-run.out" 'run.complete=true'
require_contains compatible-run "$root/compatible-run.out" 'run.failed=false'
operation_index=$(sed -n \
  's/^dispatch\.\([0-9][0-9]*\)\.kind=operation$/\1/p' \
  "$root/compatible-run.out")
[ -n "$operation_index" ] || fail 'compatible-owner run has no operation dispatch'
operation_lines=$(printf '%s\n' "$operation_index" | wc -l | tr -d ' ')
[ "$operation_lines" -eq 1 ] || fail 'compatible-owner run has multiple operation dispatches'
effect=$(sed -n \
  "s/^dispatch\.$operation_index\.effect-attempt=//p" \
  "$root/compatible-run.out")
[ "${#effect}" -eq 64 ] || fail 'compatible-owner dispatch omits effect-attempt identity'

"$pkgctl" inspect-effect --effect-store "$runtime/effects" --attempt "$effect" \
  >"$root/compatible-effect.out" 2>"$root/compatible-effect.err" || {
  dump_file 'compatible effect inspection stdout' "$root/compatible-effect.out"
  dump_file 'compatible effect inspection stderr' "$root/compatible-effect.err"
  fail 'compatible-owner terminal effect inspection failed'
}
require_contains compatible-effect "$root/compatible-effect.out" 'effect.stage=terminal'
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.application-outcome=completed'
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.application-completed-evidence='
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.transaction-evidence='
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.publication-outcome=published'
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.publication-resulting-snapshot='
require_contains compatible-effect "$root/compatible-effect.out" \
  'effect.terminal-outcome=completed'
application_evidence=$(field 'effect.application-completed-evidence=' \
  "$root/compatible-effect.out")
transaction_evidence=$(field 'effect.transaction-evidence=' \
  "$root/compatible-effect.out")
resulting_snapshot=$(field 'effect.publication-resulting-snapshot=' \
  "$root/compatible-effect.out")

"$state_ownership_inspect_fixture" \
  "$state" runtime-lib usr/lib/shared-ownership-marker \
  >"$root/runtime-owner.out" 2>"$root/runtime-owner.err" || {
  dump_file 'runtime-lib ownership stdout' "$root/runtime-owner.out"
  dump_file 'runtime-lib ownership stderr' "$root/runtime-owner.err"
  fail 'compatible second owner was not published'
}
require_contains runtime-owner "$root/runtime-owner.out" 'packages 2'
require_contains runtime-owner "$root/runtime-owner.out" 'package runtime-lib 1.0-1'
require_contains runtime-owner "$root/runtime-owner.out" 'path usr/lib/shared-ownership-marker'
require_contains runtime-owner "$root/runtime-owner.out" 'kind regular'
require_contains runtime-owner "$root/runtime-owner.out" 'origin retained-existing'
require_contains runtime-owner "$root/runtime-owner.out" 'mode 0644'
require_contains runtime-owner "$root/runtime-owner.out" 'size 17'
require_contains runtime-owner "$root/runtime-owner.out" \
  'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
require_contains runtime-owner "$root/runtime-owner.out" 'owners 2'
require_contains runtime-owner "$root/runtime-owner.out" 'owner.0 base-files '
require_contains runtime-owner "$root/runtime-owner.out" 'owner.1 runtime-lib '
runtime_manifest=$(field 'manifest ' "$root/runtime-owner.out")
[ "$runtime_manifest" -gt 0 ] || fail 'runtime-lib published an empty manifest'
runtime_snapshot=$(field 'snapshot ' "$root/runtime-owner.out")
runtime_application=$(field 'application-evidence ' "$root/runtime-owner.out")
runtime_transaction=$(field 'transaction-evidence ' "$root/runtime-owner.out")
require_equal compatible-publication-snapshot "$resulting_snapshot" "$runtime_snapshot"
require_equal compatible-application-binding "$application_evidence" "$runtime_application"
require_equal compatible-transaction-binding "$transaction_evidence" "$runtime_transaction"

"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/base-after.out" 2>"$root/base-after.err" || {
  dump_file 'base ownership after sharing stdout' "$root/base-after.out"
  dump_file 'base ownership after sharing stderr' "$root/base-after.err"
  fail 'first owner disappeared after compatible publication'
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
    "$(field "$semantic_field" "$root/runtime-owner.out")"
done

# This is hostile observation only: state meaning above came from owner APIs.
# A retain-observed compatible share must not rewrite the already-active object.
marker_fingerprint_after=$(stat -c '%d:%i:%f:%u:%g:%s:%y:%z' \
  "$target/usr/lib/shared-ownership-marker")
marker_digest_after=$(sha256sum "$target/usr/lib/shared-ownership-marker" | awk '{print $1}')
marker_payload_after=$(cat "$target/usr/lib/shared-ownership-marker")
require_equal shared-path-no-rewrite "$marker_fingerprint_before" "$marker_fingerprint_after"
require_equal shared-path-payload "$marker_payload_before" "$marker_payload_after"
require_equal shared-path-digest "$marker_digest_before" "$marker_digest_after"
[ -f "$target/runtime-lib-marker" ] || \
  fail 'compatible operation completed without its unique runtime-lib payload'
require_equal runtime-payload runtime-lib-source "$(cat "$target/runtime-lib-marker")"

final_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 2'
require_contains final-state "$root/final-state.out" 'package base-files 1.0-1'
require_contains final-state "$root/final-state.out" 'package runtime-lib 1.0-1'

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
require_contains audit "$root/audit.out" 'packages 2'
require_contains audit "$root/audit.out" 'findings 0'
require_contains audit "$root/audit.out" 'failures 0'
