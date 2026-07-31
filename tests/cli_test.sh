#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
run_store_fixture=$3
expected_release=$4
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
$pkgctl --help | grep -F 'The commands are read-only.' >/dev/null
$pkgctl --help | grep -F 'pkgctl inspect-run --run-store PATH --journal SHA256' >/dev/null

catalog=$($pkgctl catalog --collection "core=$collection")
printf '%s\n' "$catalog" | grep -F 'session.kind=catalog' >/dev/null
printf '%s\n' "$catalog" | grep -F 'catalog.candidates=3' >/dev/null

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
