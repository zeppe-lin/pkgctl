#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
run_evidence_inspect_fixture=$4
runtime_root_fixture=$5
fixture_collection=$6
root_view_fixture=$7
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-root-authority-matrix.XXXXXX")
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
package_objects=$root/package-objects
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-run-root-authority-matrix: %s\n' "$*" >&2
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
      "pkgctl:cli-run-root-authority-matrix: $label: missing expected text: $expected" >&2
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

seed_host_root()
{
  path=$1
  token=$2
  printf '%s\n' "$token-read-authority" >"$path/matrix-host-read-authority"
  printf '%s\n' "$token-write-authority" >"$path/matrix-host-write-authority"
}

require_host_root_unchanged()
{
  label=$1
  path=$2
  token=$3
  require_equal "$label-read-sentinel" "$token-read-authority" \
    "$(cat "$path/matrix-host-read-authority")"
  require_equal "$label-write-sentinel" "$token-write-authority" \
    "$(cat "$path/matrix-host-write-authority")"
}

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
  installed-resources \
  check-resources \
  check-temporary \
  lifecycle-sessions; do
  mkdir "$runtime/$directory"
done

"$root_view_fixture" "$build"
"$root_view_fixture" "$lifecycle"
build_interpreter=$("$runtime_root_fixture" "$build" /bin/sh)
lifecycle_interpreter=$("$runtime_root_fixture" "$lifecycle" /bin/sh)
require_equal interpreter "$build_interpreter" "$lifecycle_interpreter"
interpreter=$build_interpreter
case $interpreter in
  /*)
    ;;
  *)
    fail "runtime fixture returned non-absolute interpreter: $interpreter"
    ;;
esac

seed_host_root "$runtime" runtime-host
seed_host_root "$build" build-host
seed_host_root "$runtime/artifacts" artifact-host
printf '%s\n' build-root-view-authority >"$build/matrix-build-root-view-sentinel"
printf '%s\n' lifecycle-root-view-authority >"$lifecycle/matrix-lifecycle-root-view-sentinel"
printf '%s\n%s\n%s\n' "$runtime" "$build" "$runtime/artifacts" \
  >"$build/matrix-host-coordinates"
printf '%s\n%s\n%s\n' "$runtime" "$build" "$runtime/artifacts" \
  >"$lifecycle/matrix-host-coordinates"

initial_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

nonce=$(printf '%064d' 7)
set -- run \
  --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --goal 'run=@base' \
  --goal 'check=matrix' \
  --goal 'lifecycle:post-install=matrix' \
  --start "$nonce" \
  --build-parallelism 1 \
  --build-source-date-epoch 0 \
  --operation-policy strict-exclusive \
  --build-root-view "$(printf '%064d' 81)" \
  --lifecycle-root-view "$(printf '%064d' 82)" \
  --runtime-root "$runtime" \
  --package-object-store "$package_objects" \
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
  if grep -F -- 'native execution unavailable before transaction execution;' \
      "$root/run.err" >/dev/null; then
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the privileged native CLI integration path'
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
journal=$(sed -n 's/^journal //p' "$root/run.out")
[ -n "$journal" ] || fail 'terminal run did not expose journal identity'

"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" >"$root/evidence.out" || {
  dump_file 'durable evidence' "$root/evidence.out"
  fail 'terminal durable evidence is unavailable'
}
require_contains evidence "$root/evidence.out" 'complete yes'
require_contains evidence "$root/evidence.out" 'constructions 1'
require_contains evidence "$root/evidence.out" 'checks 1'

require_host_root_unchanged runtime-host "$runtime" runtime-host
require_host_root_unchanged build-host "$build" build-host
require_host_root_unchanged artifact-host "$runtime/artifacts" artifact-host
require_equal build-phase build-phase-ran "$(cat "$target/matrix-build-ran")"
require_equal lifecycle-phase lifecycle-phase-ran \
  "$(cat "$target/matrix-lifecycle-ran")"
require_equal build-root-view-sentinel build-root-view-authority \
  "$(cat "$build/matrix-build-root-view-sentinel")"
require_equal lifecycle-root-view-sentinel lifecycle-root-view-authority \
  "$(cat "$lifecycle/matrix-lifecycle-root-view-sentinel")"

final_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$final_state" >"$root/final-state.out"
require_contains final-state "$root/final-state.out" 'packages 1'

# Terminal cleanup owns only dispatch-scoped realizations. It must not erase or
# mutate caller root views, the private runtime hierarchy, or durable artifacts.
for directory in construction-sessions package-outputs installed-resources check-resources check-temporary lifecycle-sessions; do
  if [ -d "$runtime/$directory" ] && \
      find "$runtime/$directory" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "terminal cleanup retained private realization under $directory"
  fi
done
archive_count=$(find "$runtime/artifacts" -type f -name '*.tar' | wc -l)
[ "$archive_count" -eq 1 ] || \
  fail "artifact authority retained $archive_count package archives, expected 1"

# The run frontend deliberately owns a private artifact root beneath runtime.
# Repeat BUILD/CHECK through the build frontend with an external public
# --artifact-root so the public projection coordinate is attacked independently.
public_state=$root/public-state
public_runtime=$root/public-runtime
public_build=$root/public-build
public_artifacts=$root/public-artifacts
public_package_objects=$root/public-package-objects
public_binding=$("$state_fixture" "$public_state")
mkdir "$public_runtime" "$public_build" "$public_artifacts"
for directory in \
  command-evidence \
  run \
  evidence \
  effects \
  content \
  construction-sessions \
  package-outputs \
  installed-resources \
  check-resources \
  check-temporary; do
  mkdir "$public_runtime/$directory"
done
"$root_view_fixture" "$public_build"
public_interpreter=$("$runtime_root_fixture" "$public_build" /bin/sh)
require_equal public-interpreter "$interpreter" "$public_interpreter"

seed_host_root "$public_runtime" runtime-host
seed_host_root "$public_build" build-host
seed_host_root "$public_artifacts" artifact-host
printf '%s\n' build-root-view-authority \
  >"$public_build/matrix-build-root-view-sentinel"
printf '%s\n%s\n%s\n' \
  "$public_runtime" "$public_build" "$public_artifacts" \
  >"$public_build/matrix-host-coordinates"

public_nonce=$(printf '%064d' 8)
set -- build matrix --check \
  --canonical-store "$public_state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --start "$public_nonce" \
  --build-parallelism 1 \
  --build-source-date-epoch 0 \
  --build-root-view "$(printf '%064d' 83)" \
  --runtime-root "$public_runtime" \
  --package-object-store "$public_package_objects" \
  --build-root "$public_build" \
  --artifact-root "$public_artifacts" \
  --interpreter "$public_interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --max-steps 4
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done

set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $public_binding >"$root/public-build.out" 2>"$root/public-build.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
  dump_file 'public build stdout' "$root/public-build.out"
  dump_file 'public build stderr' "$root/public-build.err"
  fail "public build: expected status 0, got $status"
fi
require_contains public-build "$root/public-build.out" 'origin admitted'
require_contains public-build "$root/public-build.out" 'frontend build'
require_contains public-build "$root/public-build.out" 'disposition completed'
require_contains public-build "$root/public-build.out" 'complete yes'
require_contains public-build "$root/public-build.out" 'failed no'
public_journal=$(sed -n 's/^journal //p' "$root/public-build.out")
[ -n "$public_journal" ] || fail 'public build did not expose journal identity'
"$run_evidence_inspect_fixture" \
  "$public_runtime/run" "$public_runtime/evidence" "$public_journal" \
  >"$root/public-evidence.out" || {
  dump_file 'public build evidence' "$root/public-evidence.out"
  fail 'public build durable evidence is unavailable'
}
require_contains public-evidence "$root/public-evidence.out" 'complete yes'
require_contains public-evidence "$root/public-evidence.out" 'constructions 1'
require_contains public-evidence "$root/public-evidence.out" 'checks 1'
require_host_root_unchanged public-runtime-host "$public_runtime" runtime-host
require_host_root_unchanged public-build-host "$public_build" build-host
require_host_root_unchanged public-artifact-host "$public_artifacts" artifact-host
require_equal public-build-root-view-sentinel build-root-view-authority \
  "$(cat "$public_build/matrix-build-root-view-sentinel")"
public_archive_count=$(find "$public_artifacts" -type f -name '*.tar' | wc -l)
[ "$public_archive_count" -eq 1 ] || \
  fail "public artifact root retained $public_archive_count package archives, expected 1"
public_final_state=$("$state_inspect_fixture" "$public_state")
printf '%s\n' "$public_final_state" >"$root/public-final-state.out"
require_contains public-final-state "$root/public-final-state.out" 'packages 0'

exit 0
