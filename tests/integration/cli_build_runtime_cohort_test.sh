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
    set -- "$@" \
      --build-parallelism 1 \
      --build-source-date-epoch 0
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

# CHECK must realize fresh resources from durable artifacts/evidence, not consume
# construction meat.
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
  --build-parallelism 1 \
  --build-source-date-epoch 0 \
  --runtime-root "$runtime2" --build-root "$build2" --artifact-root "$artifacts2" \
  --interpreter "$interpreter2" --build-user-id "$uid" --build-group-id "$gid" \
  --max-steps 7
for group in $groups; do [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"; done
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding2 >"$root/hostile-build.out" 2>"$root/hostile-build.err" || {
  dump_file hostile-build "$root/hostile-build.err"
  fail 'hostile setup construction failed'
}
require_contains hostile-build "$root/hostile-build.out" 'disposition step-limit-reached'
require_contains hostile-build "$root/hostile-build.out" 'complete no'
require_contains hostile-build "$root/hostile-build.out" 'failed no'
require_contains hostile-build "$root/hostile-build.out" 'artifacts 7'
journal2=$(sed -n 's/^journal //p' "$root/hostile-build.out")
[ -n "$journal2" ] || fail 'hostile setup report omitted journal'
"$run_evidence_inspect_fixture" "$runtime2/run" "$runtime2/evidence" "$journal2" \
  >"$root/hostile-before.out" || {
  dump_file hostile-before "$root/hostile-before.out"
  fail 'hostile setup durable evidence inspection failed'
}
require_contains hostile-before "$root/hostile-before.out" 'complete no'
require_contains hostile-before "$root/hostile-before.out" 'failed no'
require_contains hostile-before "$root/hostile-before.out" 'constructions 7'
require_contains hostile-before "$root/hostile-before.out" 'checks 0'
before_record=$(sed -n 's/^record //p' "$root/hostile-before.out")
before_sequence=$(sed -n 's/^sequence //p' "$root/hostile-before.out")
before_dispatches=$(sed -n 's/^dispatches //p' "$root/hostile-before.out")
[ -n "$before_record" ] && [ -n "$before_sequence" ] && \
  [ -n "$before_dispatches" ] ||
  fail 'hostile setup durable head identity is unavailable'

idx=$(sed -n 's/^artifact\.\([0-9][0-9]*\)\.package cohort-libgcc$/\1/p' "$root/hostile-build.out")
[ -n "$idx" ] || fail 'hostile setup cannot locate cohort-libgcc artifact'
libgcc_artifact=$(sed -n "s/^artifact\.$idx\.path //p" "$root/hostile-build.out")
[ -f "$libgcc_artifact" ] || fail 'cohort-libgcc artifact path is absent'
cp "$libgcc_artifact" "$root/cohort-libgcc.clean"
printf '%s\n' hostile >>"$libgcc_artifact"

set_hostile_resume()
{
  set -- build --canonical-store "$state2" --resume "$nonce2" \
    --runtime-root "$runtime2" --build-root "$build2" --artifact-root "$artifacts2" \
    --interpreter "$interpreter2" --build-user-id "$uid" --build-group-id "$gid" \
    --max-steps 1
  for group in $groups; do
    [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"
  done
  HOSTILE_RESUME_ARGS=$(printf '%s\n' "$@")
}
run_hostile_refusal()
{
  label=$1
  set_hostile_resume
  set --
  while IFS= read -r arg; do set -- "$@" "$arg"; done <<EOF_HOSTILE_ARGS
$HOSTILE_RESUME_ARGS
EOF_HOSTILE_ARGS
  set +e
  "$pkgctl" "$@" >"$root/$label.out" 2>"$root/$label.err"
  status=$?
  set -e
  [ "$status" -ne 0 ] || fail "$label: corrupted cohort check input was accepted"
  require_contains "$label" "$root/$label.err" \
    'native check package realization failed:'
  if find "$runtime2/check-temporary" -type f -name cohort-check-ran -print -quit | \
      grep . >/dev/null; then
    fail "$label: check program executed after retained cohort authority corruption"
  fi
}

# The first refusal happens after the CHECK attempt/session authority is durably
# started. It must retain one recoverable active check, not invent failed truth.
run_hostile_refusal hostile-check
"$run_evidence_inspect_fixture" "$runtime2/run" "$runtime2/evidence" "$journal2" \
  >"$root/hostile-started.out" || {
  dump_file hostile-started "$root/hostile-started.out"
  fail 'started cohort check evidence became unreadable after authority refusal'
}
require_contains hostile-started "$root/hostile-started.out" 'complete no'
require_contains hostile-started "$root/hostile-started.out" 'failed no'
require_contains hostile-started "$root/hostile-started.out" 'constructions 7'
require_contains hostile-started "$root/hostile-started.out" 'checks 0'
started_index=$(sed -n 's/^dispatch\.\([0-9][0-9]*\)\.state started$/\1/p' \
  "$root/hostile-started.out")
[ -n "$started_index" ] || fail 'authority refusal retained no started dispatch'
case $started_index in
  *[!0-9]*|'') fail 'authority refusal retained ambiguous started dispatches' ;;
esac
[ "$(grep -c '\.state started$' "$root/hostile-started.out" || :)" -eq 1 ] ||
  fail 'authority refusal retained more than one started dispatch'
require_contains hostile-started "$root/hostile-started.out" \
  "dispatch.$started_index.kind check"
require_contains hostile-started "$root/hostile-started.out" \
  "dispatch.$started_index.attempt yes"
require_contains hostile-started "$root/hostile-started.out" \
  "dispatch.$started_index.terminal-evidence no"
started_record=$(sed -n 's/^record //p' "$root/hostile-started.out")
started_sequence=$(sed -n 's/^sequence //p' "$root/hostile-started.out")
started_dispatches=$(sed -n 's/^dispatches //p' "$root/hostile-started.out")
[ "$started_record" != "$before_record" ] ||
  fail 'retained authority refusal did not durably start the check attempt'
[ "$started_dispatches" -eq $((before_dispatches + 1)) ] ||
  fail 'retained authority refusal did not reserve exactly one check dispatch'
[ "$started_index" -eq "$before_dispatches" ] ||
  fail 'retained authority refusal did not append the started check dispatch'
# A new executable dispatch is two durable transitions: reservation, then start.
[ "$started_sequence" -eq $((before_sequence + 2)) ] ||
  fail 'retained authority refusal changed history beyond check reservation and start'
started_snapshot=$(cat "$root/hostile-started.out")
find "$runtime2/construction-sessions" -mindepth 1 -print -quit | grep . >/dev/null ||
  fail 'authority-refused cohort check cleaned construction residue'
find "$runtime2/package-outputs" -mindepth 1 -print -quit | grep . >/dev/null ||
  fail 'authority-refused cohort check cleaned package-output residue'

# Repeating the same refusal must recover the retained started attempt, not
# publish another start record or mint a second attempt session.
run_hostile_refusal hostile-check-repeat
"$run_evidence_inspect_fixture" "$runtime2/run" "$runtime2/evidence" "$journal2" \
  >"$root/hostile-started-repeat.out" || {
  dump_file hostile-started-repeat "$root/hostile-started-repeat.out"
  fail 'repeated authority refusal made started check evidence unreadable'
}
started_repeat_snapshot=$(cat "$root/hostile-started-repeat.out")
[ "$started_snapshot" = "$started_repeat_snapshot" ] || {
  dump_file hostile-started "$root/hostile-started.out"
  dump_file hostile-started-repeat "$root/hostile-started-repeat.out"
  fail 'repeated retained authority refusal changed the durable started checkpoint'
}

# Restore the exact public artifact bytes. Recovery must execute the same
# retained CHECK attempt and complete; refusal above is not terminal failure.
cp "$root/cohort-libgcc.clean" "$libgcc_artifact"
set_hostile_resume
set --
while IFS= read -r arg; do set -- "$@" "$arg"; done <<EOF_HOSTILE_RECOVERY_ARGS
$HOSTILE_RESUME_ARGS
EOF_HOSTILE_RECOVERY_ARGS
"$pkgctl" "$@" >"$root/hostile-recovered.out" 2>"$root/hostile-recovered.err" || {
  dump_file hostile-recovered-stdout "$root/hostile-recovered.out"
  dump_file hostile-recovered-stderr "$root/hostile-recovered.err"
  fail 'restored cohort authority did not recover the pending check'
}
require_contains hostile-recovered "$root/hostile-recovered.out" 'origin resumed'
require_contains hostile-recovered "$root/hostile-recovered.out" 'disposition completed'
require_contains hostile-recovered "$root/hostile-recovered.out" 'complete yes'
require_contains hostile-recovered "$root/hostile-recovered.out" 'failed no'
for directory in construction-sessions package-outputs check-resources check-temporary; do
  if find "$runtime2/$directory" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "recovered cohort terminal cleanup retained private realization: $directory"
  fi
done
"$run_evidence_inspect_fixture" "$runtime2/run" "$runtime2/evidence" "$journal2" \
  >"$root/hostile-recovered-evidence.out" || {
  dump_file hostile-recovered-evidence "$root/hostile-recovered-evidence.out"
  fail 'recovered cohort durable evidence inspection failed'
}
require_contains hostile-recovered-evidence "$root/hostile-recovered-evidence.out" 'constructions 7'
require_contains hostile-recovered-evidence "$root/hostile-recovered-evidence.out" 'checks 1'

printf '%s\n' 'pkgctl:cli-build-runtime-cohort: ok'
