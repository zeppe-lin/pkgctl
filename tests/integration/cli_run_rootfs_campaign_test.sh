#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
run_evidence_inspect_fixture=$4
runtime_root_fixture=$5
rootfs_authority_audit_fixture=$6
state_ownership_inspect_fixture=$7
fixture_collection=$8
root_view_fixture=$9
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
  printf '%s\n' "--- $1 ---" >&2
  if [ -s "$2" ]; then
    cat "$2" >&2
  else
    printf '%s\n' '<empty>' >&2
  fi
}

require_contains()
{
  if ! grep -F -- "$3" "$2" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-rootfs-campaign: $1: missing expected text: $3" \
      >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_not_contains()
{
  if grep -F -- "$3" "$2" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-rootfs-campaign: $1: contains forbidden text: $3" \
      >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_equal()
{
  [ "$3" = "$2" ] || \
    fail "$1: expected '$2', got '$3'"
}

require_absent()
{
  [ ! -e "$2" ] || fail "$1: unexpected path exists: $2"
}

artifact_field()
{
  report=$1
  index=$2
  field=$3
  value=$(sed -n "s/^artifact\.$index\.$field //p" "$report")
  [ -n "$value" ] || fail "artifact $index report omits $field"
  lines=$(printf '%s\n' "$value" | wc -l)
  [ "$lines" -eq 1 ] || fail "artifact $index report repeats $field"
  printf '%s\n' "$value"
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

# Poison the private artifact namespace before admission.  Artifact reporting
# must derive only from retained construction authority, never by scanning the
# directory for plausible archives.
mkdir "$root/foreign-artifact"
printf '%s\n' 'foreign-not-construction-authority' >"$root/foreign-artifact/foreign-marker"
tar -cf "$runtime/artifacts/foreign.tar" -C "$root/foreign-artifact" foreign-marker
foreign_artifact_before=$(sha256sum "$runtime/artifacts/foreign.tar")

"$root_view_fixture" "$build"
"$root_view_fixture" "$lifecycle"
mkdir_program=$(command -v mkdir) || fail 'host mkdir is unavailable for rootfs build fixture'
case $mkdir_program in
  /*)
    ;;
  *)
    fail "host mkdir did not resolve to an absolute path: $mkdir_program"
    ;;
esac
build_interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$mkdir_program")
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
  --operation-policy exact-compatible-sharing \
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
require_contains run "$root/run.out" 'artifacts 4'
require_contains run "$root/run.out" 'artifact.0.package base-files'
require_contains run "$root/run.out" 'artifact.1.package build-tool'
require_contains run "$root/run.out" 'artifact.2.package rootfs-probe'
require_contains run "$root/run.out" 'artifact.3.package runtime-lib'
require_contains run "$root/run.out" "artifact.0.path $runtime/artifacts/"
require_not_contains run "$root/run.out" 'frontend build'
require_not_contains run "$root/run.out" 'foreign.tar'
require_not_contains run "$root/run.out" 'foreign-marker'
require_equal foreign-artifact-preserved "$foreign_artifact_before" \
  "$(sha256sum "$runtime/artifacts/foreign.tar")"

# Every reported artifact must still exist after terminal cleanup and its bytes
# must agree with the retained digest.  This prevents a report that merely
# prints plausible metadata from an in-memory construction object.
base_files_archive=
base_files_digest=
build_tool_archive=
build_tool_digest=
rootfs_probe_archive=
rootfs_probe_digest=
runtime_lib_archive=
runtime_lib_digest=
for index in 0 1 2 3; do
  path=$(artifact_field "$root/run.out" "$index" path)
  digest=$(artifact_field "$root/run.out" "$index" sha256)
  identity=$(artifact_field "$root/run.out" "$index" identity)
  binding=$(artifact_field "$root/run.out" "$index" binding-identity)
  image=$(artifact_field "$root/run.out" "$index" image-identity)
  case $path in
    "$runtime/artifacts/"*)
      ;;
    *)
      fail "artifact $index escaped private artifact authority: $path"
      ;;
  esac
  [ -f "$path" ] || fail "artifact $index report names absent bytes: $path"
  require_equal "artifact-$index-sha256" "$digest" "$(sha256sum "$path" | awk '{print $1}')"
  for value in "$digest" "$identity" "$binding" "$image"; do
    case $value in
      *[!0-9a-f]*|'')
        fail "artifact $index report contains malformed digest identity: $value"
        ;;
    esac
    [ "${#value}" -eq 64 ] || \
      fail "artifact $index report digest identity has length ${#value}, expected 64"
  done
  case $index in
    0)
      base_files_archive=$path
      base_files_digest=$digest
      ;;
    1)
      build_tool_archive=$path
      build_tool_digest=$digest
      ;;
    2)
      rootfs_probe_archive=$path
      rootfs_probe_digest=$digest
      ;;
    3)
      runtime_lib_archive=$path
      runtime_lib_digest=$digest
      ;;
  esac
done

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

"$state_ownership_inspect_fixture" \
  "$state" base-files usr/lib/shared-ownership-marker \
  >"$root/shared-base.out" 2>"$root/shared-base.err" || {
  dump_file 'rootfs base-files shared ownership stdout' "$root/shared-base.out"
  dump_file 'rootfs base-files shared ownership stderr' "$root/shared-base.err"
  fail 'rootfs campaign did not publish base-files shared ownership'
}
"$state_ownership_inspect_fixture" \
  "$state" runtime-lib usr/lib/shared-ownership-marker \
  >"$root/shared-runtime.out" 2>"$root/shared-runtime.err" || {
  dump_file 'rootfs runtime-lib shared ownership stdout' "$root/shared-runtime.out"
  dump_file 'rootfs runtime-lib shared ownership stderr' "$root/shared-runtime.err"
  fail 'rootfs campaign did not publish runtime-lib shared ownership'
}
for report in "$root/shared-base.out" "$root/shared-runtime.out"; do
  require_contains shared-owner "$report" 'packages 3'
  require_contains shared-owner "$report" 'path usr/lib/shared-ownership-marker'
  require_contains shared-owner "$report" 'kind regular'
  require_contains shared-owner "$report" 'mode 0644'
  require_contains shared-owner "$report" 'size 17'
  require_contains shared-owner "$report" \
    'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
  require_contains shared-owner "$report" 'owners 2'
  require_contains shared-owner "$report" 'owner.0 base-files '
  require_contains shared-owner "$report" 'owner.1 runtime-lib '
done
base_origin=$(sed -n 's/^origin //p' "$root/shared-base.out")
runtime_origin=$(sed -n 's/^origin //p' "$root/shared-runtime.out")
case "$base_origin:$runtime_origin" in
  incoming-payload:retained-existing|retained-existing:incoming-payload)
    ;;
  *)
    fail "rootfs shared ownership origins are not one incoming/one retained: $base_origin/$runtime_origin"
    ;;
esac
require_equal shared-owned-target shared-authority \
  "$(cat "$target/usr/lib/shared-ownership-marker")"

require_equal base-files-target base-files-source \
  "$(cat "$target/base-files-marker")"
require_equal runtime-dependency-target runtime-lib-source \
  "$(cat "$target/runtime-lib-marker")"
require_equal rootfs-probe-target rootfs-probe-source+build-tool-source \
  "$(cat "$target/rootfs-probe-marker")"
require_absent build-only-target "$target/build-tool-token"

artifact_list=$root/artifacts.list
find "$runtime/artifacts" -type f -name '*.tar' ! -name foreign.tar | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 4 ] || \
  fail "rootfs campaign retained $artifact_count package archives, expected 4"

# Package/image meaning comes from retained artifact bindings plus libpkgimage,
# not from tar directory scans.  The authority audit opens every reported
# archive under its exact whole-byte digest, replays one package-specific
# payload witness, requires build-only authority to stay out of canonical
# state, and derives the installed audit inventory from the three sealed images.
run_authority_audit()
{
  "$rootfs_authority_audit_fixture" "$state" "$target" \
    base-files installed \
      "$base_files_archive" "$base_files_digest" \
      base-files-marker base-files-source \
    build-tool build-only \
      "$build_tool_archive" "$build_tool_digest" \
      build-tool-token build-tool-source \
    rootfs-probe installed \
      "$rootfs_probe_archive" "$rootfs_probe_digest" \
      rootfs-probe-marker rootfs-probe-source+build-tool-source \
    runtime-lib installed \
      "$runtime_lib_archive" "$runtime_lib_digest" \
      runtime-lib-marker runtime-lib-source
}

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

# A fresh process must recover and report the exact same artifact authority
# after terminal cleanup.  Poison the namespace again to make directory scans
# observable, remove the collection so semantic rediscovery is impossible, and
# override the interpreter with an unusable path: zero-work terminal resume
# must need none of them.
cp "$root/run.out" "$root/admitted-terminal.out"
rm -rf "$collection"
printf '%s\n' 'second-foreign-not-authority' >"$root/foreign-artifact/foreign-marker"
tar -cf "$runtime/artifacts/foreign-after-terminal.tar" \
  -C "$root/foreign-artifact" foreign-marker

set -- run --canonical-store "$state" \
  --resume "$(printf '%064d' 6)" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --lifecycle-root "$lifecycle" \
  --target-root "$target" \
  --interpreter /bin/false \
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
"$pkgctl" "$@" >"$root/resume.out" 2>"$root/resume.err"
resume_status=$?
set -e
[ "$resume_status" -eq 0 ] || {
  dump_file 'terminal resume stdout' "$root/resume.out"
  dump_file 'terminal resume stderr' "$root/resume.err"
  fail "terminal resume: expected status 0, got $resume_status"
}
require_contains terminal-resume "$root/resume.out" 'origin resumed'
require_contains terminal-resume "$root/resume.out" 'disposition completed'
require_contains terminal-resume "$root/resume.out" 'durable-steps 0'
require_contains terminal-resume "$root/resume.out" 'complete yes'
require_contains terminal-resume "$root/resume.out" 'failed no'
require_contains terminal-resume "$root/resume.out" 'artifacts 4'
require_not_contains terminal-resume "$root/resume.out" 'frontend build'
require_not_contains terminal-resume "$root/resume.out" 'foreign.tar'
require_not_contains terminal-resume "$root/resume.out" 'foreign-after-terminal.tar'

for index in 0 1 2 3; do
  for field in package version release release-identity path identity sha256 bytes \
      build-result-identity binding-identity image-identity; do
    require_equal "terminal-resume-artifact-$index-$field" \
      "$(artifact_field "$root/admitted-terminal.out" "$index" "$field")" \
      "$(artifact_field "$root/resume.out" "$index" "$field")"
  done
done
require_equal terminal-resume-state "$final_state" \
  "$("$state_inspect_fixture" "$state")"

set +e
run_authority_audit >"$root/audit-clean.out" 2>"$root/audit-clean.err"
audit_status=$?
set -e
if [ "$audit_status" -ne 0 ]; then
  dump_file 'clean audit stdout' "$root/audit-clean.out"
  dump_file 'clean audit stderr' "$root/audit-clean.err"
  fail "clean rootfs audit: expected status 0, got $audit_status"
fi
require_contains clean-audit "$root/audit-clean.out" 'complete yes'
require_contains clean-audit "$root/audit-clean.out" 'packages 3'
require_contains clean-audit "$root/audit-clean.out" 'artifacts 4'
audit_objects=$(sed -n 's/^objects //p' "$root/audit-clean.out")
[ -n "$audit_objects" ] && [ "$audit_objects" -gt 0 ] || \
  fail 'independent rootfs image authority produced no expected objects'
require_contains clean-audit "$root/audit-clean.out" 'findings 0'
require_contains clean-audit "$root/audit-clean.out" 'failures 0'

rm "$target/runtime-lib-marker"
set +e
run_authority_audit >"$root/audit-drift.out" 2>"$root/audit-drift.err"
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
