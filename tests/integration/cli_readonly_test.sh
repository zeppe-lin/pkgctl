#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
run_store_fixture=$3
effect_store_fixture=$4
expected_release=$5
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli.XXXXXX")
cleanup()
{
  # Canonical generation directories are sealed read-only. Restore owner
  # write permission only while removing the test fixture.
  find "$root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$root"
}
trap cleanup EXIT HUP INT TERM
collection=$root/collection
state=$root/state
mkdir -p "$collection/app" "$collection/libfoo" "$collection/tool"
cat >"$collection/profiles.yml" <<'YAML'
format: zeppe-lin.profiles/1

profiles:
  base:
    members:
      - package: app
YAML
recipe()
{
  name=$1
  requirements=$2
  cat <<YAML
format: zeppe-lin.recipe/1

package:
  name: $name
  version: 1.0
  release: 1
  summary: $name package
  licenses:
    - GPL-3.0-or-later

requirements:
$requirements

sources: []

build:
  language: posix-shell
  script: |
    true

architectures:
  build:
    - x86_64
  target:
    - x86_64
YAML
}
recipe app '  build:
    - package: tool
  run:
    - package: libfoo' >"$collection/app/recipe.yml"
recipe libfoo '  {}' >"$collection/libfoo/recipe.yml"
recipe tool '  {}' >"$collection/tool/recipe.yml"

binding=$($state_fixture "$state")
actual_version=$($pkgctl --version)
expected_version="pkgctl $expected_release"
[ "$actual_version" = "$expected_version" ] || {
  echo "unexpected pkgctl version: got '$actual_version', expected '$expected_version'" >&2
  exit 1
}
help=$($pkgctl --help)
require_help_text()
{
  expected=$1
  printf '%s\n' "$help" | grep -F -- "$expected" >/dev/null || {
    echo "pkgctl:cli-readonly: help missing expected text: $expected" >&2
    exit 1
  }
}
require_help_text 'pkgctl run OPTIONS --goal SCOPE=SUBJECT [--goal ...] --start SHA256 RUN-AUTHORITY'
require_help_text 'pkgctl run --canonical-store PATH --resume SHA256 RUN-AUTHORITY'
require_help_text 'pkgctl build PACKAGE [--check] OPTIONS --start SHA256 BUILD-AUTHORITY'
require_help_text 'pkgctl build --canonical-store PATH --resume SHA256 BUILD-AUTHORITY'
require_help_text '--artifact-root PATH'
require_help_text 'performs at most --max-steps controller advances'
require_help_text 'pkgctl inspect-run --run-store PATH --journal SHA256'
require_help_text 'pkgctl inspect-effect --effect-store PATH --attempt SHA256'

catalog=$($pkgctl catalog --collection "core=$collection")
printf '%s\n' "$catalog" | grep -F 'session.kind=catalog' >/dev/null
printf '%s\n' "$catalog" | grep -F 'catalog.candidates=3' >/dev/null

if $pkgctl catalog --collection "core=$collection" --lifecycle-root "$root/ignored" \
    >"$root/run-only.out" 2>"$root/run-only.err"; then
  echo 'catalog unexpectedly accepted run-only lifecycle authority' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'native execution authority options are valid only for run or build' \
  "$root/run-only.err" >/dev/null
[ ! -e "$root/ignored" ]

build_nonce=$(printf '%064d' 9)
# Build owns its exact build/check goals; generic goal policy is not a second
# route into the frontend. This refusal happens at parse time and creates none
# of the supplied runtime coordinates.
# shellcheck disable=SC2086
if $pkgctl build app \
    --collection "core=$collection" \
    --canonical-store "$state" $binding \
    --build-architecture x86_64 --target-architecture x86_64 \
    --goal 'build=app' \
    --start "$build_nonce" \
    --runtime-root "$root/build-policy-runtime" \
    --build-root "$root/build-policy-root" \
    --artifact-root "$root/build-policy-artifacts" \
    --interpreter /bin/false \
    --build-user-id 0 --build-group-id 0 \
    --source-date-epoch 0 --max-steps 1 \
    >"$root/build-policy.out" 2>"$root/build-policy.err"; then
  echo 'build unexpectedly accepted caller-supplied goal policy' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'build owns its build/check goals; --goal is invalid' \
  "$root/build-policy.err" >/dev/null
[ ! -e "$root/build-policy-runtime" ]
[ ! -e "$root/build-policy-root" ]
[ ! -e "$root/build-policy-artifacts" ]

# Target-operation authority is structurally invalid for the build frontend.
# shellcheck disable=SC2086
if $pkgctl build app \
    --collection "core=$collection" \
    --canonical-store "$state" $binding \
    --build-architecture x86_64 --target-architecture x86_64 \
    --start "$build_nonce" \
    --runtime-root "$root/build-target-runtime" \
    --build-root "$root/build-target-root" \
    --artifact-root "$root/build-target-artifacts" \
    --target-root "$root/build-forbidden-target" \
    --interpreter /bin/false \
    --build-user-id 0 --build-group-id 0 \
    --source-date-epoch 0 --max-steps 1 \
    >"$root/build-target.out" 2>"$root/build-target.err"; then
  echo 'build unexpectedly accepted target-operation authority' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'target-operation authority options are invalid for build' \
  "$root/build-target.err" >/dev/null
[ ! -e "$root/build-forbidden-target" ]

if $pkgctl build app --check --canonical-store "$state" \
    --resume "$build_nonce" >"$root/build-resume-policy.out" \
    2>"$root/build-resume-policy.err"; then
  echo 'build resume unexpectedly accepted fresh package/check semantics' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F -- '--resume uses retained transaction semantics' \
  "$root/build-resume-policy.err" >/dev/null

invalid_collection=$root/invalid-collection
mkdir -p "$invalid_collection/broken"
cat >"$invalid_collection/broken/recipe.yml" <<'YAML'
format: zeppe-lin.recipe/1
package: [
YAML
if $pkgctl catalog --collection "broken=$invalid_collection" \
    >"$root/invalid-yaml.out" 2>"$root/invalid-yaml.err"; then
  echo 'malformed YAML unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'pkgctl: yaml: ' "$root/invalid-yaml.err" >/dev/null
grep -F -- "$invalid_collection/broken/recipe.yml:" \
  "$root/invalid-yaml.err" >/dev/null
[ ! -s "$root/invalid-yaml.out" ]

before=$(find "$state" -type f -print | sort | xargs sha256sum)
# shellcheck disable=SC2086
resolution=$($pkgctl resolve \
  --collection "core=$collection" \
  --canonical-store "$state" $binding \
  --build-architecture x86_64 --target-architecture x86_64 \
  --goal 'build=@base' --goal 'run=@base')
printf '%s\n' "$resolution" | grep -F 'session.kind=resolution' >/dev/null
printf '%s\n' "$resolution" | grep -F 'goal.0.subject=@base' >/dev/null

# shellcheck disable=SC2086
transaction=$($pkgctl transaction \
  --collection "core=$collection" \
  --canonical-store "$state" $binding \
  --build-architecture x86_64 --target-architecture x86_64 \
  --goal 'build=@base' --goal 'run=@base')
printf '%s\n' "$transaction" | grep -F 'session.kind=transaction' >/dev/null
printf '%s\n' "$transaction" | grep -F 'transaction.convergence=preserve-unselected' >/dev/null

# shellcheck disable=SC2086
exact=$($pkgctl transaction \
  --collection "core=$collection" \
  --canonical-store "$state" $binding \
  --build-architecture x86_64 --target-architecture x86_64 \
  --goal 'build=@base' --goal 'run=@base' --converge-exact)
printf '%s\n' "$exact" | grep -F 'transaction.convergence=converge-exact' >/dev/null

after=$(find "$state" -type f -print | sort | xargs sha256sum)
[ "$before" = "$after" ]

if $pkgctl resolve --collection "core=$collection" >/dev/null 2>&1; then
  echo 'incomplete resolve command unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi

missing=$root/missing-state
# shellcheck disable=SC2086
if $pkgctl resolve \
  --collection "core=$collection" \
  --canonical-store "$missing" $binding \
  --build-architecture x86_64 --target-architecture x86_64 \
  --goal 'run=@base' >/dev/null 2>&1; then
  echo 'missing state unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
[ ! -e "$missing" ]


run_store=$root/run-store
journal=$($run_store_fixture "$run_store")
[ "${#journal}" -eq 64 ]
# A read-only load must not require or recreate the writer lock.
rm -f "$run_store/.pkgctl-run.lock"
run_before=$(find "$run_store" -type f -print | sort | xargs sha256sum)
inspection=$($pkgctl inspect-run --run-store "$run_store" --journal "$journal")
printf '%s\n' "$inspection" | grep -F 'session.kind=transaction-run' >/dev/null
printf '%s\n' "$inspection" | grep -F "run.journal=$journal" >/dev/null
printf '%s\n' "$inspection" | grep -F 'run.sequence=0' >/dev/null
printf '%s\n' "$inspection" | grep -F 'run.disposition=quiescent-incomplete' >/dev/null
run_after=$(find "$run_store" -type f -print | sort | xargs sha256sum)
[ "$run_before" = "$run_after" ]
[ ! -e "$run_store/.pkgctl-run.lock" ]

if $pkgctl inspect-run --run-store "$run_store" --journal invalid \
    >"$root/invalid.out" 2>"$root/invalid.err"; then
  echo 'invalid run journal identity unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'invalid --journal' "$root/invalid.err" >/dev/null

missing_journal=$(printf '%064d' 0)
if $pkgctl inspect-run --run-store "$run_store" --journal "$missing_journal" \
    >"$root/missing-run.out" 2>"$root/missing-run.err"; then
  echo 'missing run journal head unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'store-conflict' "$root/missing-run.err" >/dev/null
grep -F 'no committed store head' "$root/missing-run.err" >/dev/null

missing_run_store=$root/missing-run-store
if $pkgctl inspect-run --run-store "$missing_run_store" --journal "$journal" \
    >"$root/missing-store.out" 2>"$root/missing-store.err"; then
  echo 'missing run store unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'store-open-failed' "$root/missing-store.err" >/dev/null
[ ! -e "$missing_run_store" ]

head=$run_store/$journal.pjh
chmod u+w "$head"
printf 'X' | dd of="$head" bs=1 seek=0 conv=notrunc status=none
chmod a-w "$head"
if $pkgctl inspect-run --run-store "$run_store" --journal "$journal" \
    >"$root/corrupt.out" 2>"$root/corrupt.err"; then
  echo 'corrupt run journal head unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'store-corrupt' "$root/corrupt.err" >/dev/null


effect_store=$root/effect-store
attempt=$($effect_store_fixture "$effect_store")
[ "${#attempt}" -eq 64 ]
# A read-only effect load must not require or recreate the writer lock.
rm -f "$effect_store/.pkgctl-effect.lock"
effect_before=$(find "$effect_store" -type f -print | sort | xargs sha256sum)
effect_inspection=$($pkgctl inspect-effect \
  --effect-store "$effect_store" --attempt "$attempt")
printf '%s\n' "$effect_inspection" | grep -F 'session.kind=effect-attempt' >/dev/null
printf '%s\n' "$effect_inspection" | grep -F "effect.attempt=$attempt" >/dev/null
printf '%s\n' "$effect_inspection" | grep -F 'effect.sequence=0' >/dev/null
printf '%s\n' "$effect_inspection" | grep -F 'effect.stage=admitted' >/dev/null
printf '%s\n' "$effect_inspection" | grep -F 'effect.disposition=start-application' >/dev/null
printf '%s\n' "$effect_inspection" | grep -F \
  'effect.automatically-continuable=true' >/dev/null
printf '%s\n' "$effect_inspection" | grep -F \
  'effect.external-resolution-required=false' >/dev/null
for absent in \
  effect.previous= \
  effect.active-index= \
  effect.application-receipt= \
  effect.transaction-evidence= \
  effect.publication-request= \
  effect.publication-receipt= \
  effect.terminal-outcome= \
  effect.reconciled-state=; do
  if printf '%s\n' "$effect_inspection" | grep -F -- "$absent" >/dev/null; then
    echo "absent effect evidence was synthesized: $absent" >&2
    exit 1
  fi
done
repeat_effect_inspection=$($pkgctl inspect-effect \
  --effect-store "$effect_store" --attempt "$attempt")
[ "$effect_inspection" = "$repeat_effect_inspection" ]
effect_after=$(find "$effect_store" -type f -print | sort | xargs sha256sum)
[ "$effect_before" = "$effect_after" ]
[ ! -e "$effect_store/.pkgctl-effect.lock" ]

if $pkgctl inspect-effect --effect-store "$effect_store" --attempt invalid \
    >"$root/invalid-effect.out" 2>"$root/invalid-effect.err"; then
  echo 'invalid effect attempt identity unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 2 ]
fi
grep -F 'invalid --attempt' "$root/invalid-effect.err" >/dev/null

missing_attempt=$(printf '%064d' 0)
if $pkgctl inspect-effect \
    --effect-store "$effect_store" --attempt "$missing_attempt" \
    >"$root/missing-effect.out" 2>"$root/missing-effect.err"; then
  echo 'missing effect attempt head unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'pkgctl: effect journal: store-conflict:' \
  "$root/missing-effect.err" >/dev/null
grep -F 'has no committed store head' \
  "$root/missing-effect.err" >/dev/null

missing_effect_store=$root/missing-effect-store
if $pkgctl inspect-effect \
    --effect-store "$missing_effect_store" --attempt "$attempt" \
    >"$root/missing-effect-store.out" \
    2>"$root/missing-effect-store.err"; then
  echo 'missing effect store unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'pkgctl: effect journal: store-open-failed:' \
  "$root/missing-effect-store.err" >/dev/null
[ ! -e "$missing_effect_store" ]

effect_head=$effect_store/$attempt.pjeh
chmod u+w "$effect_head"
printf 'X' | dd of="$effect_head" bs=1 seek=0 conv=notrunc status=none
chmod a-w "$effect_head"
if $pkgctl inspect-effect \
    --effect-store "$effect_store" --attempt "$attempt" \
    >"$root/corrupt-effect.out" 2>"$root/corrupt-effect.err"; then
  echo 'corrupt effect attempt head unexpectedly succeeded' >&2
  exit 1
else
  [ "$?" -eq 1 ]
fi
grep -F 'pkgctl: effect journal: store-corrupt:' \
  "$root/corrupt-effect.err" >/dev/null
