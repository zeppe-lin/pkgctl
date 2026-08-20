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
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli-build-archive-source.XXXXXX")
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
nonce=$(printf '%064d' 9)
cp -R "$fixture_collection" "$collection"
binding=$("$state_fixture" "$state")
uid=$(id -u)
gid=$(id -g)
groups=$(id -G | tr ' ' '\n' | sort -nu)

fail()
{
  printf 'pkgctl:cli-build-archive-source: %s\n' "$*" >&2
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
      "pkgctl:cli-build-archive-source: $label: missing expected text: $expected" >&2
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

mkdir "$runtime" "$build" "$artifacts"
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
  /*) ;;
  *) fail "host chmod did not resolve to an absolute path: $chmod_program" ;;
esac
interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$chmod_program")
case $interpreter in
  /*) ;;
  *) fail "runtime fixture returned non-absolute interpreter: $interpreter" ;;
esac

initial_state=$("$state_inspect_fixture" "$state")
set -- build archive-probe --check \
  --canonical-store "$state" \
  --collection "core=$collection" \
  --build-architecture x86_64 \
  --target-architecture x86_64 \
  --start "$nonce" \
  --build-parallelism 1 \
  --build-source-date-epoch 0 \
  --build-root-view "$(printf '%064d' 81)" \
  --runtime-root "$runtime" \
  --package-object-store "$root/package-objects" \
  --build-root "$build" \
  --artifact-root "$artifacts" \
  --interpreter "$interpreter" \
  --build-user-id "$uid" \
  --build-group-id "$gid" \
  --max-steps 2
for group in $groups; do
  if [ "$group" != "$gid" ]; then
    set -- "$@" --build-supplementary-group "$group"
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
    printf '%s\n' \
      'pkgctl:cli-build-archive-source: native execution preflight is unavailable;' \
      'privileged native execution is required for this case' >&2
    dump_file 'native execution preflight' "$root/run.err"
    if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
      fail 'release qualification requires the archive-source native CLI path'
    fi
    exit 77
  fi
  dump_file 'run stdout' "$root/run.out"
  dump_file 'run stderr' "$root/run.err"
  fail "build/check returned status $status, expected 0"
fi

require_contains run "$root/run.out" 'origin admitted'
require_contains run "$root/run.out" 'disposition step-limit-reached'
require_contains run "$root/run.out" 'durable-steps 2'
require_contains run "$root/run.out" 'complete no'
require_contains run "$root/run.out" 'failed no'
require_contains run "$root/run.out" 'frontend build'
require_contains run "$root/run.out" 'artifacts 2'
require_contains run "$root/run.out" '.package archive-dep'
require_contains run "$root/run.out" '.package archive-probe'

# Construction-private resource trees are not check authority. Remove them
# completely before resuming the check; recovery must realize fresh source/package
# resources from retained materialization and sealed artifact evidence.
find "$runtime/construction-sessions" "$runtime/package-outputs" \
  -type d -exec chmod u+w {} + 2>/dev/null || :
rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"
mkdir "$runtime/construction-sessions" "$runtime/package-outputs"

set -- build \
  --canonical-store "$state" \
  --resume "$nonce" \
  --runtime-root "$runtime" \
  --package-object-store "$root/package-objects" \
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

"$pkgctl" "$@" >"$root/resume.out" 2>"$root/resume.err" || {
  dump_file 'resume stdout' "$root/resume.out"
  dump_file 'resume stderr' "$root/resume.err"
  fail 'check resume after construction-residue removal failed'
}
require_contains resume "$root/resume.out" 'origin resumed'
require_contains resume "$root/resume.out" 'disposition completed'
require_contains resume "$root/resume.out" 'durable-steps 1'
require_contains resume "$root/resume.out" 'complete yes'
require_contains resume "$root/resume.out" 'failed no'
require_contains resume "$root/resume.out" 'artifacts 2'
require_contains resume "$root/resume.out" '.package archive-dep'
require_contains resume "$root/resume.out" '.package archive-probe'


final_state=$("$state_inspect_fixture" "$state")
require_equal canonical-state "$initial_state" "$final_state"

archive_count=$(find "$artifacts" -type f -name '*.tar' | wc -l | tr -d ' ')
[ "$archive_count" -eq 2 ] || fail "expected two public archives, got $archive_count"
probe_index=$(awk '
  $1 ~ /^artifact\.[0-9]+\.package$/ && $2 == "archive-probe" {
    split($1, fields, "."); print fields[2]; exit
  }
' "$root/resume.out")
[ -n "$probe_index" ] || fail 'archive-probe artifact index is absent'
artifact=$(awk -v key="artifact.$probe_index.path" '$1 == key { print $2; exit }' \
  "$root/resume.out")
[ -f "$artifact" ] || fail 'archive-probe public archive is absent'
require_equal archive-payload archive-source+archive-dependency \
  "$(tar -xOf "$artifact" archive-result)"

journal=$(sed -n 's/^journal //p' "$root/resume.out")
[ -n "$journal" ] || fail 'terminal report did not expose journal identity'
"$run_evidence_inspect_fixture" \
  "$runtime/run" "$runtime/evidence" "$journal" >"$root/terminal-evidence.out" || {
  dump_file 'terminal durable evidence' "$root/terminal-evidence.out"
  fail 'terminal durable construction/check evidence is unavailable after cleanup'
}
require_contains terminal-evidence "$root/terminal-evidence.out" 'complete yes'
require_contains terminal-evidence "$root/terminal-evidence.out" 'failed no'
require_contains terminal-evidence "$root/terminal-evidence.out" 'stopped no'
require_contains terminal-evidence "$root/terminal-evidence.out" 'constructions 2'
require_contains terminal-evidence "$root/terminal-evidence.out" 'checks 1'
require_contains terminal-evidence "$root/terminal-evidence.out" \
  'construction-evidence 2'
require_contains terminal-evidence "$root/terminal-evidence.out" \
  'check-evidence 1'

for directory in construction-sessions package-outputs check-resources check-temporary; do
  if [ -d "$runtime/$directory" ] && \
      find "$runtime/$directory" -mindepth 1 -print -quit | grep . >/dev/null; then
    fail "terminal cleanup retained private realization under $directory"
  fi
done
