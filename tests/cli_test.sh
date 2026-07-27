#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-cli.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM
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
[ "$($pkgctl --version)" = 'pkgctl 0.1.0' ]
$pkgctl --help | grep -F 'The commands are read-only.' >/dev/null

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
