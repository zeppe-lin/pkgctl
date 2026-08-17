#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
state_ownership_inspect_fixture=$5
compatible_image_fixture=$6
hostile_image_fixture=$7
rootfs_audit_fixture=$8
compatible_collection=$9
hostile_collection=${10}
root_view_fixture=${11}
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run-shared-ownership-refusal.XXXXXX")
cleanup()
{
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-run-shared-ownership-refusal: %s\n' "$*" >&2
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
      "pkgctl:cli-run-shared-ownership-refusal: $1: missing expected text: $3" >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_not_contains()
{
  if grep -F -- "$3" "$2" >/dev/null; then
    printf '%s\n' \
      "pkgctl:cli-run-shared-ownership-refusal: $1: contains forbidden text: $3" >&2
    dump_file "$1" "$2"
    exit 1
  fi
}

require_equal()
{
  [ "$3" = "$2" ] || fail "$1: values differ"
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

setup_case()
{
  case_name=$1
  case_root=$root/$case_name
  state=$case_root/state
  runtime=$case_root/runtime
  build=$case_root/build
  lifecycle=$case_root/lifecycle
  target=$case_root/target
  qualified_artifacts=$case_root/qualified-artifacts
  mkdir -p "$case_root"

  binding=$("$state_fixture" "$state")
  initial_state=$("$state_inspect_fixture" "$state")
  printf '%s\n' "$initial_state" >"$case_root/initial-state.out"
  require_contains "$case_name initial-state" "$case_root/initial-state.out" 'packages 0'

  mkdir "$runtime" "$build" "$lifecycle" "$target" "$qualified_artifacts"
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
  require_equal "$case_name interpreter-authority" "$interpreter" "$lifecycle_interpreter"
  case $interpreter in
    /*) ;;
    *) fail "runtime fixture returned non-absolute interpreter: $interpreter" ;;
  esac
}

run_package_success()
{
  label=$1
  collection=$2
  package=$3
  policy=$4
  nonce=$5

  set -- run --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal "run=$package" \
    --start "$(printf '%064d' "$nonce")" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy "$policy" \
    --runtime-root "$runtime" \
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
  "$pkgctl" "$@" $binding >"$case_root/$label.out" 2>"$case_root/$label.err"
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$case_root/$label.err" >/dev/null; then
      dump_file "$label native execution preflight" "$case_root/$label.err"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the refusal integration path'
      fi
      exit 77
    fi
    dump_file "$label stdout" "$case_root/$label.out"
    dump_file "$label stderr" "$case_root/$label.err"
    fail "$label: expected status 0, got $status"
  fi

  require_contains "$label" "$case_root/$label.out" 'origin admitted'
  require_contains "$label" "$case_root/$label.out" 'disposition completed'
  require_contains "$label" "$case_root/$label.out" 'complete yes'
  require_contains "$label" "$case_root/$label.out" 'failed no'
  require_contains "$label" "$case_root/$label.out" 'artifacts 1'
  require_contains "$label" "$case_root/$label.out" "artifact.0.package $package"
}

run_package_refusal()
{
  label=$1
  collection=$2
  package=$3
  policy=$4
  nonce=$5

  set -- run --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal "run=$package" \
    --start "$(printf '%064d' "$nonce")" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --operation-policy "$policy" \
    --runtime-root "$runtime" \
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
  "$pkgctl" "$@" $binding >"$case_root/$label.out" 2>"$case_root/$label.err"
  status=$?
  set -e
  [ "$status" -ne 0 ] || fail "$label: expected planning refusal, got status 0"
  require_contains "$label stderr" "$case_root/$label.err" \
    'native operation planning refused'
  require_not_contains "$label stderr" "$case_root/$label.err" 'with code '
  require_not_contains "$label stdout" "$case_root/$label.out" 'disposition completed'
}

build_qualified_image()
{
  label=$1
  collection=$2
  package=$3
  nonce=$4
  image_fixture=$5
  expected_size=$6
  expected_content=$7
  expected_payload=$8

  set -- build "$package" \
    --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --start "$(printf '%064d' "$nonce")" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --artifact-root "$qualified_artifacts" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --max-steps 1
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --build-supplementary-group "$group"
    fi
  done

  set +e
  "$pkgctl" "$@" $binding >"$case_root/$label.out" 2>"$case_root/$label.err"
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$case_root/$label.err" >/dev/null; then
      require_equal "$label unavailable-state" "$initial_state" \
        "$("$state_inspect_fixture" "$state")"
      if find "$target" -mindepth 1 -print -quit | grep . >/dev/null; then
        fail "$label native preflight refusal mutated the empty target"
      fi
      printf '%s\n' \
        'pkgctl:cli-run-shared-ownership-refusal: native execution preflight is unavailable;' \
        'privileged native execution is required for this case' >&2
      dump_file "$label native execution preflight" "$case_root/$label.err"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the refusal integration path'
      fi
      exit 77
    fi
    dump_file "$label stdout" "$case_root/$label.out"
    dump_file "$label stderr" "$case_root/$label.err"
    fail "$package image construction failed with status $status"
  fi
  require_contains "$label" "$case_root/$label.out" 'frontend build'
  require_contains "$label" "$case_root/$label.out" 'disposition completed'
  require_contains "$label" "$case_root/$label.out" "artifact.0.package $package"

  artifact=$(field 'artifact.0.path ' "$case_root/$label.out")
  sha256=$(field 'artifact.0.sha256 ' "$case_root/$label.out")
  [ -f "$artifact" ] || fail "$package public archive is absent"
  "$image_fixture" "$artifact" "$sha256" \
    >"$case_root/$label-image.out" 2>"$case_root/$label-image.err" || {
    dump_file "$package image inspection stdout" "$case_root/$label-image.out"
    dump_file "$package image inspection stderr" "$case_root/$label-image.err"
    fail "$package image authority is not qualified"
  }
  require_contains "$package image" "$case_root/$label-image.out" \
    'path usr/lib/shared-ownership-marker'
  require_contains "$package image" "$case_root/$label-image.out" 'mode 0644'
  require_contains "$package image" "$case_root/$label-image.out" \
    "size $expected_size"
  require_contains "$package image" "$case_root/$label-image.out" \
    "content $expected_content"
  require_contains "$package image" "$case_root/$label-image.out" \
    "$expected_payload"

  post_build_state=$("$state_inspect_fixture" "$state")
  require_equal "$package build-state" "$initial_state" "$post_build_state"
}

target_fingerprint()
{
  output=$1
  (
    cd "$target"
    find . ! -name . -exec stat -c 'meta %n|%F|%f|%u|%g|%s|%y|%z' {} \;
    find . -type f -exec sha256sum {} \; | sed 's/^/sha256 /'
  ) | LC_ALL=C sort >"$output"
}

capture_sole_owner()
{
  "$state_ownership_inspect_fixture" \
    "$state" base-files usr/lib/shared-ownership-marker \
    >"$case_root/base-before.out" 2>"$case_root/base-before.err" || {
    dump_file 'base ownership before refusal' "$case_root/base-before.out"
    dump_file 'base ownership before refusal stderr' "$case_root/base-before.err"
    fail 'refusal case lacks qualified sole-owner precondition'
  }
  require_contains base-before "$case_root/base-before.out" 'packages 1'
  require_contains base-before "$case_root/base-before.out" 'origin incoming-payload'
  require_contains base-before "$case_root/base-before.out" 'owners 1'
  require_contains base-before "$case_root/base-before.out" 'owner.0 base-files '
  manifest=$(field 'manifest ' "$case_root/base-before.out")
  [ "$manifest" -gt 0 ] || fail 'sole-owner precondition has empty manifest'

  before_state=$("$state_inspect_fixture" "$state")
  printf '%s\n' "$before_state" >"$case_root/state-before.out"
  cp "$case_root/base-before.out" "$case_root/base-authority-before.out"
  target_fingerprint "$case_root/target-before.out"
  [ -f "$target/usr/lib/shared-ownership-marker" ] || \
    fail 'sole-owner precondition lacks shared marker'
  require_equal shared-marker-before shared-authority \
    "$(cat "$target/usr/lib/shared-ownership-marker")"
}

assert_refusal_preserved_authority()
{
  label=$1
  rejected_package=$2
  unique_marker=$3

  after_state=$("$state_inspect_fixture" "$state")
  require_equal "$label canonical-state" "$before_state" "$after_state"

  "$state_ownership_inspect_fixture" \
    "$state" base-files usr/lib/shared-ownership-marker \
    >"$case_root/base-after.out" 2>"$case_root/base-after.err" || {
    dump_file "$label base ownership after refusal" "$case_root/base-after.out"
    dump_file "$label base ownership stderr" "$case_root/base-after.err"
    fail "$label damaged first-owner authority"
  }
  require_equal "$label base-authority" \
    "$(cat "$case_root/base-authority-before.out")" \
    "$(cat "$case_root/base-after.out")"

  set +e
  "$state_ownership_inspect_fixture" \
    "$state" "$rejected_package" usr/lib/shared-ownership-marker \
    >"$case_root/rejected-owner.out" 2>"$case_root/rejected-owner.err"
  rejected_owner_status=$?
  set -e
  [ "$rejected_owner_status" -ne 0 ] || \
    fail "$label published ownership for refused package"
  require_contains "$label rejected owner" "$case_root/rejected-owner.err" \
    'selected installed package is absent'

  target_fingerprint "$case_root/target-after.out"
  if ! cmp -s "$case_root/target-before.out" "$case_root/target-after.out"; then
    diff -u "$case_root/target-before.out" "$case_root/target-after.out" >&2 || :
    fail "$label mutated the target before refusal"
  fi
  [ ! -e "$target/$unique_marker" ] || \
    fail "$label activated rejected package payload"
  require_equal "$label shared-marker" shared-authority \
    "$(cat "$target/usr/lib/shared-ownership-marker")"

  "$rootfs_audit_fixture" "$state" "$target" \
    >"$case_root/$label-audit.out" 2>"$case_root/$label-audit.err" || {
    dump_file "$label audit stdout" "$case_root/$label-audit.out"
    dump_file "$label audit stderr" "$case_root/$label-audit.err"
    fail "$label left canonical state/target inconsistent"
  }
  require_contains "$label audit" "$case_root/$label-audit.out" 'complete yes'
  require_contains "$label audit" "$case_root/$label-audit.out" 'packages 1'
  require_contains "$label audit" "$case_root/$label-audit.out" 'findings 0'
  require_contains "$label audit" "$case_root/$label-audit.out" 'failures 0'
}

# Layer 4a: exact compatible bytes exist, but the admitted complete policy
# forbids shared ownership. Planner refusal must precede target/state mutation.
setup_case strict
build_qualified_image compatible-build "$compatible_collection" runtime-lib 60 \
  "$compatible_image_fixture" 17 \
  v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06 \
  'payload shared-authority\n'
run_package_success base "$compatible_collection" base-files strict-exclusive 61
capture_sole_owner
run_package_refusal forbidden "$compatible_collection" runtime-lib strict-exclusive 62
assert_refusal_preserved_authority forbidden runtime-lib runtime-lib-marker

# Layer 4b: sharing is admitted, but owner-qualified incoming image authority is
# deliberately incompatible with the active shared object. Again, refuse first.
setup_case incompatible
build_qualified_image hostile-build "$hostile_collection" runtime-lib-hostile 71 \
  "$hostile_image_fixture" 18 \
  v1:sha256:724f65e1fb4870e360aeea5c62e71c41fa94578e90dce25ed64a425096fbf9cb \
  'payload hostile-authority\n'
run_package_success base "$compatible_collection" base-files strict-exclusive 72
capture_sole_owner
run_package_refusal incompatible "$hostile_collection" runtime-lib-hostile \
  exact-compatible-sharing 73
assert_refusal_preserved_authority incompatible runtime-lib-hostile \
  runtime-lib-hostile-marker
