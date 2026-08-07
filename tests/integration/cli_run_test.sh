#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
interpreter=$4
case $interpreter in
  /*)
    ;;
  *)
    interpreter_dir=$(dirname "$interpreter")
    interpreter_name=$(basename "$interpreter")
    interpreter=$(
      cd "$interpreter_dir"
      printf '%s/%s\n' "$(pwd -P)" "$interpreter_name"
    )
    ;;
esac
fixture_collection=$5
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-run.XXXXXX")
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
target=$root/target
cp -R "$fixture_collection" "$collection"
binding=$($state_fixture "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

run_command()
{
  intent=$1
  nonce=$2
  maximum_steps=$3
  set -- run \
    --collection "core=$collection" \
    --canonical-store "$state" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --goal 'run=@base' \
    "$intent" "$nonce" \
    --runtime-root "$runtime" \
    --build-root "$build" \
    --target-root "$target" \
    --interpreter "$interpreter" \
    --user-id "$uid" \
    --group-id "$gid" \
    --source-date-epoch 0 \
    --max-steps "$maximum_steps"
  for group in $groups; do
    if [ "$group" != "$gid" ]; then
      set -- "$@" --supplementary-group "$group"
    fi
  done
  # The fixture emits these five option/value pairs as one trusted shell word
  # sequence so the CLI is exercised exactly as an operator would invoke it.
  # shellcheck disable=SC2086
  "$pkgctl" "$@" $binding
}

zero_nonce=$(printf '%064d' 9)
if run_command --start "$zero_nonce" 0 \
    >"$root/zero.out" 2>"$root/zero.err"; then
  echo 'zero-bounded native run unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'maximum step count must be greater than zero' "$root/zero.err" \
  >/dev/null
[ ! -e "$runtime" ]
[ ! -e "$build" ]
[ ! -e "$target" ]

mkdir -p "$runtime" "$build" "$target"
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

initial_state=$($state_inspect_fixture "$state")
printf '%s\n' "$initial_state" | grep -F 'packages 0' >/dev/null

run_nonce=$(printf '%064d' 1)
start=$(run_command --start "$run_nonce" 1)
printf '%s\n' "$start" | grep -F 'origin admitted' >/dev/null
printf '%s\n' "$start" | grep -F 'disposition step-limit-reached' >/dev/null
printf '%s\n' "$start" | grep -F 'steps 1' >/dev/null
printf '%s\n' "$start" | grep -F 'durable-steps 1' >/dev/null
printf '%s\n' "$start" | grep -F 'complete no' >/dev/null
printf '%s\n' "$start" | grep -F 'failed no' >/dev/null
transaction=$(printf '%s\n' "$start" | sed -n 's/^transaction //p')
journal=$(printf '%s\n' "$start" | sed -n 's/^journal //p')
[ "${#transaction}" -eq 64 ]
[ "${#journal}" -eq 64 ]

state_after_construction=$($state_inspect_fixture "$state")
[ "$initial_state" = "$state_after_construction" ]
[ ! -e "$target/usr/bin/pkgctl-fixture" ]
[ "$(find "$runtime/evidence" -type f | wc -l)" -gt 0 ]

inspection=$($pkgctl inspect-run --run-store "$runtime/run" --journal "$journal")
printf '%s\n' "$inspection" | grep -F "run.journal=$journal" >/dev/null
printf '%s\n' "$inspection" | grep -F 'run.complete=false' >/dev/null

if run_command --start "$run_nonce" 1 \
    >"$root/restart.out" 2>"$root/restart.err"; then
  echo 'second start of an admitted run unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'exact transaction run is already admitted; use --resume' \
  "$root/restart.err" >/dev/null

rm -rf "$collection"
resume=$(run_command --resume "$run_nonce" 8)
printf '%s\n' "$resume" | grep -F "transaction $transaction" >/dev/null
printf '%s\n' "$resume" | grep -F "journal $journal" >/dev/null
printf '%s\n' "$resume" | grep -F 'origin resumed' >/dev/null
printf '%s\n' "$resume" | grep -F 'disposition completed' >/dev/null
printf '%s\n' "$resume" | grep -F 'complete yes' >/dev/null
printf '%s\n' "$resume" | grep -F 'failed no' >/dev/null

[ -x "$target/usr/bin/pkgctl-fixture" ]
printf 'pkgctl integration fixture\n' >"$root/expected-payload"
cmp "$root/expected-payload" "$target/usr/bin/pkgctl-fixture"
final_state=$($state_inspect_fixture "$state")
printf '%s\n' "$final_state" | grep -F 'packages 1' >/dev/null
printf '%s\n' "$final_state" | grep -F 'package fixture 1.0-1' >/dev/null

target_before=$(sha256sum "$target/usr/bin/pkgctl-fixture")
state_before=$($state_inspect_fixture "$state")
repeat=$(run_command --resume "$run_nonce" 4)
printf '%s\n' "$repeat" | grep -F "transaction $transaction" >/dev/null
printf '%s\n' "$repeat" | grep -F "journal $journal" >/dev/null
printf '%s\n' "$repeat" | grep -F 'origin resumed' >/dev/null
printf '%s\n' "$repeat" | grep -F 'disposition completed' >/dev/null
printf '%s\n' "$repeat" | grep -F 'durable-steps 0' >/dev/null
printf '%s\n' "$repeat" | grep -F 'complete yes' >/dev/null
[ "$target_before" = "$(sha256sum "$target/usr/bin/pkgctl-fixture")" ]
[ "$state_before" = "$($state_inspect_fixture "$state")" ]
