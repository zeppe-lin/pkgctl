#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
image_fixture=$5
fixture_collection=$6
root_view_fixture=$7
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-shared-ownership-image.XXXXXX")
cleanup()
{
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)
collection=$root/collection
cp -R "$fixture_collection" "$collection"

fail()
{
  printf 'pkgctl:cli-build-shared-ownership-image: %s\n' "$*" >&2
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
      "pkgctl:cli-build-shared-ownership-image: $label: missing expected text: $expected" >&2
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

build_one()
{
  package=$1
  nonce=$2
  prefix=$root/$package
  state=$prefix/state
  runtime=$prefix/runtime
  build=$prefix/build
  artifacts=$prefix/artifacts
  mkdir -p "$prefix" "$runtime" "$build" "$artifacts"
  for directory in \
    command-evidence \
    run \
    evidence \
    effects \
    content \
    construction-sessions \
    package-outputs \
    check-resources \
    check-temporary; do
    mkdir "$runtime/$directory"
  done

  binding=$("$state_fixture" "$state")
  initial_state=$("$state_inspect_fixture" "$state")
  printf '%s\n' "$initial_state" >"$prefix/initial-state.out"
  require_contains "$package initial-state" "$prefix/initial-state.out" 'packages 0'

  "$root_view_fixture" "$build"
  mkdir_program=$(command -v mkdir) || fail 'host mkdir is unavailable for runtime fixture'
  case $mkdir_program in
    /*) ;;
    *) fail "host mkdir did not resolve to an absolute path: $mkdir_program" ;;
  esac
  interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$mkdir_program")
  case $interpreter in
    /*) ;;
    *) fail "runtime fixture returned non-absolute interpreter: $interpreter" ;;
  esac

  set -- build "$package" \
    --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --start "$nonce" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --artifact-root "$artifacts" \
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
  # shellcheck disable=SC2086
  "$pkgctl" "$@" $binding >"$prefix/build.out" 2>"$prefix/build.err"
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$prefix/build.err" >/dev/null; then
      printf '%s\n' \
        'pkgctl:cli-build-shared-ownership-image: native execution preflight is unavailable;' \
        'privileged native execution is required for this case' >&2
      dump_file "$package native execution preflight" "$prefix/build.err"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires the privileged native CLI integration path'
      fi
      exit 77
    fi
    dump_file "$package build stdout" "$prefix/build.out"
    dump_file "$package build stderr" "$prefix/build.err"
    fail "$package build: expected status 0, got $status"
  fi

  require_contains "$package build" "$prefix/build.out" 'origin admitted'
  require_contains "$package build" "$prefix/build.out" 'disposition completed'
  require_contains "$package build" "$prefix/build.out" 'durable-steps 1'
  require_contains "$package build" "$prefix/build.out" 'complete yes'
  require_contains "$package build" "$prefix/build.out" 'failed no'
  require_contains "$package build" "$prefix/build.out" 'frontend build'
  require_contains "$package build" "$prefix/build.out" 'artifacts 1'
  require_contains "$package build" "$prefix/build.out" ".package $package"

  index=$(awk -v package="$package" '
    $1 ~ /^artifact\.[0-9]+\.package$/ && $2 == package {
      split($1, fields, "."); print fields[2]; exit
    }
  ' "$prefix/build.out")
  [ -n "$index" ] || fail "$package artifact index is absent"
  artifact=$(awk -v key="artifact.$index.path" '$1 == key { print $2; exit }' \
    "$prefix/build.out")
  sha256=$(awk -v key="artifact.$index.sha256" '$1 == key { print $2; exit }' \
    "$prefix/build.out")
  image_authority=$(awk -v key="artifact.$index.image-identity" \
    '$1 == key { print $2; exit }' "$prefix/build.out")
  [ -f "$artifact" ] || fail "$package public archive is absent"
  [ -n "$sha256" ] || fail "$package archive SHA-256 is absent"
  [ -n "$image_authority" ] || fail "$package retained build-image authority is absent"

  "$image_fixture" "$artifact" "$sha256" >"$prefix/image.out" 2>"$prefix/image.err" || {
    dump_file "$package image inspection" "$prefix/image.out"
    dump_file "$package image inspection stderr" "$prefix/image.err"
    fail "$package sealed image does not carry qualified shared ownership marker"
  }
  require_contains "$package image" "$prefix/image.out" \
    'path usr/lib/shared-ownership-marker'
  require_contains "$package image" "$prefix/image.out" 'type regular'
  require_contains "$package image" "$prefix/image.out" 'mode 0644'
  require_contains "$package image" "$prefix/image.out" 'size 17'
  require_contains "$package image" "$prefix/image.out" \
    'content v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06'
  require_contains "$package image" "$prefix/image.out" 'payload shared-authority\n'

  final_state=$("$state_inspect_fixture" "$state")
  require_equal "$package canonical-state" "$initial_state" "$final_state"
}

build_one base-files "$(printf '%064d' 31)"
build_one runtime-lib "$(printf '%064d' 32)"

base_semantics=$(grep -E '^(path|type|mode|uid|gid|size|mtime|mtime-nanoseconds|content|payload) ' \
  "$root/base-files/image.out")
runtime_semantics=$(grep -E '^(path|type|mode|uid|gid|size|mtime|mtime-nanoseconds|content|payload) ' \
  "$root/runtime-lib/image.out")
require_equal shared-marker-semantics "$base_semantics" "$runtime_semantics"
