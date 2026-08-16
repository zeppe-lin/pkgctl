#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
srcdir=${1:-.}
meson=$srcdir/tests/meson.build
test_source=$srcdir/tests/integration/cli_build_runtime_cohort_test.sh
fixture=$srcdir/tests/fixtures/collections/runtime-cohort

for path in "$meson" "$test_source" "$fixture/cohort-probe/recipe.yml"; do
  [ -s "$path" ] || { echo "runtime-cohort qualification source absent: $path" >&2; exit 1; }
done
for package in cohort-headers cohort-libc-bootstrap cohort-libc cohort-libgcc cohort-filesystem cohort-checker cohort-probe; do
  [ -s "$fixture/$package/recipe.yml" ] || { echo "runtime-cohort package absent: $package" >&2; exit 1; }
done
for required in \
  'package: cohort-libgcc' \
  'package: cohort-libc'; do
  grep -F -- "$required" "$fixture/cohort-libc/recipe.yml" "$fixture/cohort-libgcc/recipe.yml" >/dev/null || {
    echo "runtime-cohort reciprocal authority missing: $required" >&2; exit 1;
  }
done
probe=$fixture/cohort-probe/recipe.yml
[ "$(grep -F '    - package: cohort-libc' "$probe" | wc -l | tr -d ' ')" -eq 2 ] || {
  echo 'runtime-cohort probe must consume libc once in build and once in check' >&2; exit 1;
}
[ "$(grep -F '    - package: cohort-libgcc' "$probe" | wc -l | tr -d ' ')" -eq 2 ] || {
  echo 'runtime-cohort probe must consume libgcc once in build and once in check' >&2; exit 1;
}
for required in \
  'build input vocabulary cardinality differs from direct authority' \
  'build input vocabulary omits direct authority' \
  'runtime closure contaminated libc package tree' \
  'runtime closure contaminated libgcc package tree' \
  'check input vocabulary cardinality differs from direct authority' \
  'check input vocabulary omits direct authority' \
  'checked package is writable' \
  'check input is writable'; do
  grep -F -- "$required" "$probe" >/dev/null || { echo "runtime-cohort recipe assault omits: $required" >&2; exit 1; }
done
for required in \
  '--max-steps 1' \
  'durable-steps 6' \
  'artifacts 7' \
  'rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"' \
  'durable-steps 0' \
  'corrupted cohort check input was accepted' \
  'native check package realization failed:' \
  'retained authority refusal did not durably start the check attempt' \
  'retained authority refusal advanced more than the check-start checkpoint' \
  'repeated retained authority refusal changed the durable started checkpoint' \
  'authority-refused cohort check cleaned construction residue' \
  'restored cohort authority did not recover the pending check' \
  'construction-evidence 7' \
  'check-evidence 1'; do
  grep -F -- "$required" "$test_source" >/dev/null || { echo "runtime-cohort CLI assault omits: $required" >&2; exit 1; }
done
for required in \
  "'cli-build-runtime-cohort'" \
  "'integration/cli_build_runtime_cohort_test.sh'" \
  "'tests/fixtures/collections/runtime-cohort'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || { echo "runtime-cohort Meson wiring omits: $required" >&2; exit 1; }
done
printf '%s\n' 'runtime-cohort CLI contract: ok'
