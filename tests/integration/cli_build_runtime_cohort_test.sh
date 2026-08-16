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

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-runtime-cohort.XXXXXX")
cleanup()
{
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM

fail() { printf 'pkgctl:cli-build-runtime-cohort: %s\n' "$*" >&2; exit 1; }
dump_file() { printf '%s\n' "--- $1 ---" >&2; [ ! -s "$2" ] || cat "$2" >&2; }
require_contains()
{
  grep -F -- "$3" "$2" >/dev/null || {
    dump_file "$1" "$2"
    fail "$1: missing expected text: $3"
  }
}
require_private_residue()
{
  find "$runtime/$1" -mindepth 1 -print -quit | grep . >/dev/null ||
    fail "private realization disappeared before terminal completion: $1"
}
require_private_empty()
{
  if find "$runtime/$1" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "terminal cleanup retained private realization: $1"
  fi
}

collection=$root/collection
state=$root/state
runtime=$root/runtime
build=$root/build
artifacts=$root/artifacts
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)
mkdir_program=$(command -v mkdir)
ln_program=$(command -v ln)
readlink_program=$(command -v readlink)
[ -n "$mkdir_program" ] && [ -n "$ln_program" ] && [ -n "$readlink_program" ] ||
  fail 'host lacks runtime-cohort fixture utility'
mkdir "$runtime" "$build" "$artifacts"
for directory in command-evidence run evidence effects content construction-sessions package-outputs check-resources check-temporary; do
  mkdir "$runtime/$directory"
done
"$root_view_fixture" "$build"
interpreter=$("$runtime_root_fixture" "$build" /bin/sh \
  "$mkdir_program" "$ln_program" "$readlink_program")

set_command()
{
  mode=$1
  steps=$2
  if [ "$mode" = start ]; then
    set -- build cohort-probe --check \
      --canonical-store "$state" \
      --collection "core=$collection" \
      --build-architecture x86_64 \
      --target-architecture x86_64 \
      --start "$(printf '%064d' 71)"
  else
    set -- build \
      --canonical-store "$state" \
      --resume "$(printf '%064d' 71)"
  fi
  set -- "$@" \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --artifact-root "$artifacts" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --source-date-epoch 0 \
    --max-steps "$steps"
  for group in $groups; do
    [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"
  done
  COMMAND_ARGS=$(printf '%s\n' "$@")
}
run_command()
{
  label=$1
  mode=$2
  steps=$3
  set_command "$mode" "$steps"
  set --
  while IFS= read -r arg; do set -- "$@" "$arg"; done <<EOF_ARGS
$COMMAND_ARGS
EOF_ARGS
  set +e
  if [ "$mode" = start ]; then
    # shellcheck disable=SC2086
    "$pkgctl" "$@" $binding >"$root/$label.out" 2>"$root/$label.err"
  else
    "$pkgctl" "$@" >"$root/$label.out" 2>"$root/$label.err"
  fi
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' "$root/$label.err" >/dev/null; then
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires runtime-cohort native execution'
      fi
      exit 77
    fi
    dump_file "$label stdout" "$root/$label.out"
    dump_file "$label stderr" "$root/$label.err"
    fail "$label returned status $status"
  fi
}

run_command first start 1
require_contains first "$root/first.out" 'disposition step-limit-reached'
require_contains first "$root/first.out" 'durable-steps 1'
require_contains first "$root/first.out" 'complete no'
require_contains first "$root/first.out" 'artifacts 1'
require_private_residue construction-sessions
require_private_residue package-outputs

run_command precheck resume 6
require_contains precheck "$root/precheck.out" 'origin resumed'
require_contains precheck "$root/precheck.out" 'disposition step-limit-reached'
require_contains precheck "$root/precheck.out" 'durable-steps 6'
require_contains precheck "$root/precheck.out" 'complete no'
require_contains precheck "$root/precheck.out" 'artifacts 7'
for package in cohort-headers cohort-libc-bootstrap cohort-libc cohort-libgcc cohort-filesystem cohort-checker cohort-probe; do
  grep -E "^artifact\.[0-9]+\.package $package$" "$root/precheck.out" >/dev/null ||
    fail "pre-check artifact set omits $package"
done
require_private_residue construction-sessions
require_private_residue package-outputs
journal=$(sed -n 's/^journal //p' "$root/precheck.out")
[ -n "$journal" ] || fail 'pre-check report omitted journal'

# CHECK must reconstruct from durable artifacts/evidence, not construction meat.
rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"
mkdir "$runtime/construction-sessions" "$runtime/package-outputs"
run_command terminal resume 1
require_contains terminal "$root/terminal.out" 'disposition completed'
require_contains terminal "$root/terminal.out" 'durable-steps 1'
require_contains terminal "$root/terminal.out" 'complete yes'
require_contains terminal "$root/terminal.out" 'failed no'
require_contains terminal "$root/terminal.out" 'artifacts 7'
for directory in construction-sessions package-outputs check-resources check-temporary; do
  require_private_empty "$directory"
done
"$run_evidence_inspect_fixture" "$runtime/run" "$runtime/evidence" "$journal" >"$root/evidence.out" || {
  dump_file evidence "$root/evidence.out"
  fail 'durable evidence inspection failed after cohort cleanup'
}
require_contains evidence "$root/evidence.out" 'constructions 7'
require_contains evidence "$root/evidence.out" 'checks 1'
require_contains evidence "$root/evidence.out" 'construction-evidence 7'
require_contains evidence "$root/evidence.out" 'check-evidence 1'

# Terminal resume has no catalog authority and must do no work.
rm -rf "$collection"
run_command repeat resume 1
require_contains repeat "$root/repeat.out" 'origin resumed'
require_contains repeat "$root/repeat.out" 'disposition completed'
require_contains repeat "$root/repeat.out" 'durable-steps 0'
require_contains repeat "$root/repeat.out" 'artifacts 7'
for directory in construction-sessions package-outputs check-resources check-temporary; do
  require_private_empty "$directory"
done

# Second transaction: corrupt one retained cohort member after all seven builds.
state2=$root/state-hostile
runtime2=$root/runtime-hostile
build2=$root/build-hostile
artifacts2=$root/artifacts-hostile
collection2=$root/collection-hostile
cp -R "$fixture_collection" "$collection2"
binding2=$("$state_fixture" "$state2")
mkdir "$runtime2" "$build2" "$artifacts2"
for directory in command-evidence run evidence effects content construction-sessions package-outputs check-resources check-temporary; do
  mkdir "$runtime2/$directory"
done
"$root_view_fixture" "$build2"
interpreter2=$("$runtime_root_fixture" "$build2" /bin/sh \
  "$mkdir_program" "$ln_program" "$readlink_program")
nonce2=$(printf '%064d' 72)
set -- build cohort-probe --check \
  --canonical-store "$state2" --collection "core=$collection2" \
  --build-architecture x86_64 --target-architecture x86_64 --start "$nonce2" \
  --runtime-root "$runtime2" --build-root "$build2" --artifact-root "$artifacts2" \
  --interpreter "$interpreter2" --build-user-id "$uid" --build-group-id "$gid" \
  --source-date-epoch 0 --max-steps 7
for group in $groups; do [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"; done
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding2 >"$root/hostile-build.out" 2>"$root/hostile-build.err" || {
  dump_file hostile-build "$root/hostile-build.err"
  fail 'hostile setup construction failed'
}
require_contains hostile-build "$root/hostile-build.out" 'disposition step-limit-reached'
require_contains hostile-build "$root/hostile-build.out" 'artifacts 7'
idx=$(sed -n 's/^artifact\.\([0-9][0-9]*\)\.package cohort-libgcc$/\1/p' "$root/hostile-build.out")
[ -n "$idx" ] || fail 'hostile setup cannot locate cohort-libgcc artifact'
libgcc_artifact=$(sed -n "s/^artifact\.$idx\.path //p" "$root/hostile-build.out")
[ -f "$libgcc_artifact" ] || fail 'cohort-libgcc artifact path is absent'
printf '%s\n' hostile >>"$libgcc_artifact"

set -- build --canonical-store "$state2" --resume "$nonce2" \
  --runtime-root "$runtime2" --build-root "$build2" --artifact-root "$artifacts2" \
  --interpreter "$interpreter2" --build-user-id "$uid" --build-group-id "$gid" \
  --source-date-epoch 0 --max-steps 1
for group in $groups; do [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"; done
set +e
"$pkgctl" "$@" >"$root/hostile-check.out" 2>"$root/hostile-check.err"
status=$?
set -e
[ "$status" -ne 0 ] || fail 'corrupted cohort check input was accepted'
require_contains hostile-check "$root/hostile-check.out" 'failed yes'
find "$runtime2/construction-sessions" -mindepth 1 -print -quit | grep . >/dev/null ||
  fail 'failed cohort check cleaned construction residue'
find "$runtime2/package-outputs" -mindepth 1 -print -quit | grep . >/dev/null ||
  fail 'failed cohort check cleaned package-output residue'

printf '%s\n' 'pkgctl:cli-build-runtime-cohort: ok'
