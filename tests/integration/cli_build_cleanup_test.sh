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

root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-cleanup.XXXXXX")
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
artifacts=$root/artifacts
external=$root/external-must-survive
nonce=$(printf '%064d' 4)
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-build-cleanup: %s\n' "$*" >&2
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
      "pkgctl:cli-build-cleanup: $label: missing expected text: $expected" >&2
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
      "pkgctl:cli-build-cleanup: $label: unexpected text: $forbidden" >&2
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

require_private_residue()
{
  directory=$1
  [ -d "$runtime/$directory" ] || \
    fail "private realization root is absent before terminal completion: $directory"
  find "$runtime/$directory" -mindepth 1 -print -quit | grep . >/dev/null || \
    fail "private realization was cleaned while transaction remained resumable: $directory"
}

require_private_empty()
{
  directory=$1
  if [ -d "$runtime/$directory" ] && \
      find "$runtime/$directory" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "terminal cleanup retained private realization under $directory"
  fi
}

mkdir "$runtime" "$build" "$artifacts" "$external"
printf '%s\n' 'do-not-follow' >"$external/sentinel"
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
"$root_view_fixture" "$build"
chmod_program=$(command -v chmod) || fail 'host chmod is unavailable for runtime fixture'
case $chmod_program in
  /*)
    ;;
  *)
    fail "host chmod did not resolve to an absolute path: $chmod_program"
    ;;
esac
interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$chmod_program")
case $interpreter in
  /*)
    ;;
  *)
    fail "runtime fixture returned non-absolute interpreter: $interpreter"
    ;;
esac

initial_state=$("$state_inspect_fixture" "$state")
printf '%s\n' "$initial_state" >"$root/initial-state.out"
require_contains initial-state "$root/initial-state.out" 'packages 0'

set -- build archive-probe --check \
  --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --start "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
set +e
# shellcheck disable=SC2086
"$pkgctl" "$@" $binding >"$root/start.out" 2>"$root/start.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
  if grep -F 'native execution unavailable before transaction execution;' \
      "$root/start.err" >/dev/null; then
    printf '%s\n' \
      'pkgctl:cli-build-cleanup: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/start.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the terminal cleanup native CLI path'
    fi
    exit 77
  fi
  dump_file 'start stdout' "$root/start.out"
  dump_file 'start stderr' "$root/start.err"
  fail "start returned status $status, expected 0"
fi

require_contains start "$root/start.out" 'disposition step-limit-reached'
require_contains start "$root/start.out" 'durable-steps 1'
require_contains start "$root/start.out" 'complete no'
require_contains start "$root/start.out" 'artifacts 1'
require_contains start "$root/start.out" 'artifact.0.package archive-dep'
require_not_contains start-stderr "$root/start.err" \
  'private realization cleanup incomplete:'
require_private_residue construction-sessions
require_private_residue package-outputs
journal=$(sed -n 's/^journal //p' "$root/start.out")
[ -n "$journal" ] || fail 'start report did not expose journal identity'

set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
"$pkgctl" "$@" >"$root/build-tool.out" 2>"$root/build-tool.err" || {
  dump_file 'tool construction stdout' "$root/build-tool.out"
  dump_file 'tool construction stderr' "$root/build-tool.err"
  fail 'tool construction resume failed'
}
require_contains tool-construction "$root/build-tool.out" 'origin resumed'
require_contains tool-construction "$root/build-tool.out" \
  'disposition step-limit-reached'
require_contains tool-construction "$root/build-tool.out" 'durable-steps 1'
require_contains tool-construction "$root/build-tool.out" 'complete no'
require_contains tool-construction "$root/build-tool.out" 'artifacts 2'
require_not_contains tool-construction-stderr "$root/build-tool.err" \
  'private realization cleanup incomplete:'
require_private_residue construction-sessions
require_private_residue package-outputs

# Replace one exact private package-output dispatch leaf with a symlink to an
# external directory. Cleanup must neither follow nor unlink this hostile
# top-level substitution. The final check does not need package-output
# realization: it reconstructs its checked package from durable public artifact
# authority, so this attack is isolated to the cleanup boundary.
package_journal=$runtime/package-outputs/$journal
[ -d "$package_journal" ] || fail 'package-output journal directory is absent'
hostile_target=$(find "$package_journal" -mindepth 1 -maxdepth 1 -type d | \
  sort | sed -n '1p')
[ -n "$hostile_target" ] || fail 'no package-output dispatch leaf to assault'
rm -rf "$hostile_target"
ln -s "$external" "$hostile_target"
[ -L "$hostile_target" ] || fail 'hostile package-output symlink was not installed'

set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
"$pkgctl" "$@" >"$root/check.out" 2>"$root/check.err" || {
  dump_file 'terminal check stdout' "$root/check.out"
  dump_file 'terminal check stderr' "$root/check.err"
  fail 'terminal check returned nonzero despite cleanup being operational'
}
require_contains check "$root/check.out" 'disposition completed'
require_contains check "$root/check.out" 'durable-steps 1'
require_contains check "$root/check.out" 'complete yes'
require_contains check "$root/check.out" 'failed no'
require_contains check "$root/check.out" 'artifacts 2'
require_contains check-cleanup "$root/check.err" \
  'pkgctl: private realization cleanup incomplete:'
require_contains check-cleanup-path "$root/check.err" "$hostile_target"
[ -L "$hostile_target" ] || \
  fail 'cleanup mutated hostile exact-target symlink instead of refusing it'
require_equal external-sentinel do-not-follow "$(cat "$external/sentinel")"
require_private_empty construction-sessions
require_private_empty check-resources
require_private_empty check-temporary

# The package-output root should contain only the refused exact-target symlink
# beneath its journal. Every other private realization was independently
# attempted and removed despite that failure.
remaining=$(find "$runtime/package-outputs" -mindepth 1 -maxdepth 2 -print | sort)
expected=$(printf '%s\n%s\n' "$package_journal" "$hostile_target" | sort)
require_equal hostile-package-output-residue "$expected" "$remaining"

"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" >"$root/evidence.out" || {
  dump_file 'durable evidence inspection' "$root/evidence.out"
  fail 'durable evidence inspection failed after realization cleanup'
}
require_contains evidence "$root/evidence.out" 'complete yes'
require_contains evidence "$root/evidence.out" 'failed no'
require_contains evidence "$root/evidence.out" 'stopped no'
require_contains evidence "$root/evidence.out" 'constructions 2'
require_contains evidence "$root/evidence.out" 'checks 1'
require_contains evidence "$root/evidence.out" 'construction-evidence 2'
require_contains evidence "$root/evidence.out" 'check-evidence 1'

artifact_list=$root/artifacts.list
find "$artifacts" -type f -name '*.tar' | sort >"$artifact_list"
artifact_count=$(wc -l <"$artifact_list")
[ "$artifact_count" -eq 2 ] || \
  fail "terminal cleanup changed public artifact count to $artifact_count"

final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"

# The operator removes only the hostile foreign substitution. A terminal resume
# has no durable transaction work left, derives the same cleanup targets again,
# and finishes the previously partial sweep without consulting the live catalog.
rm "$hostile_target"
rm -rf "$collection"
set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --source-date-epoch 0 \
  --max-steps 1
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
  fi
done
"$pkgctl" "$@" >"$root/retry.out" 2>"$root/retry.err" || {
  dump_file 'terminal cleanup retry stdout' "$root/retry.out"
  dump_file 'terminal cleanup retry stderr' "$root/retry.err"
  fail 'terminal cleanup retry failed'
}
require_contains retry "$root/retry.out" 'origin resumed'
require_contains retry "$root/retry.out" 'disposition completed'
require_contains retry "$root/retry.out" 'durable-steps 0'
require_contains retry "$root/retry.out" 'complete yes'
require_not_contains retry-stderr "$root/retry.err" \
  'private realization cleanup incomplete:'
require_not_contains retry-stderr "$root/retry.err" \
  'private realization cleanup unavailable:'
for directory in construction-sessions package-outputs check-resources check-temporary; do
  require_private_empty "$directory"
done
require_equal external-sentinel-after-retry do-not-follow "$(cat "$external/sentinel")"

"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" >"$root/retry-evidence.out" || \
  fail 'durable evidence inspection failed after cleanup retry'
cmp "$root/evidence.out" "$root/retry-evidence.out" >/dev/null || \
  fail 'cleanup retry changed durable transaction/evidence projection'
find "$artifacts" -type f -name '*.tar' | sort >"$root/retry-artifacts.list"
cmp "$artifact_list" "$root/retry-artifacts.list" >/dev/null || \
  fail 'cleanup retry changed public artifact inventory'
