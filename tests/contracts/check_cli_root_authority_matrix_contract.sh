#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_run_root_authority_matrix_test.sh"
recipe="$srcdir/tests/fixtures/collections/root-authority-matrix/matrix/recipe.yml"
profile="$srcdir/tests/fixtures/collections/root-authority-matrix/profiles.yml"
testing="$srcdir/TESTING.md"
locator_test="$srcdir/tests/unit/run_locator_test.cpp"
runtime_test="$srcdir/tests/unit/construction_test.cpp"

for path in "$meson" "$test_source" "$recipe" "$profile" "$testing" \
    "$locator_test" "$runtime_test"; do
  [ -s "$path" ] || {
    echo "root-authority matrix qualification source is absent: $path" >&2
    exit 1
  }
done

for required in \
  "'cli-run-root-authority-matrix'" \
  "'integration/cli_run_root_authority_matrix_test.sh'" \
  "'tests/fixtures/collections/root-authority-matrix'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "root-authority matrix Meson qualification omits: $required" >&2
    exit 1
  }
done

for phase in BUILD CHECK LIFECYCLE; do
  grep -F -- "phase=$phase" "$recipe" >/dev/null || {
    echo "root-authority matrix omits phase: $phase" >&2
    exit 1
  }
done

# This is a behavioral 3x3 matrix. Each real phase receives exact host
# coordinates deliberately and must fail the recipe if it observes the
# sentinel stored at any of them. The shell contract only prevents accidental
# weakening/removal of cells; the privileged execution is the proof.
for root in runtime-root build-root artifact-root; do
  count=$(grep -F -- "probe_host_authority $root" "$recipe" | wc -l)
  [ "$count" -eq 3 ] || {
    echo "root-authority matrix does not contain 3 phase probes for $root" >&2
    exit 1
  }
done

build_root_mutations=$(grep -F -- 'mutated read-only build root view' "$recipe" | wc -l)
[ "$build_root_mutations" -eq 2 ] || {
  echo 'root-authority matrix lost BUILD/CHECK logical-root mutation probes' >&2
  exit 1
}
lifecycle_root_mutations=$(grep -F -- 'mutated read-only lifecycle root view' "$recipe" | wc -l)
[ "$lifecycle_root_mutations" -eq 1 ] || {
  echo 'root-authority matrix lost lifecycle logical-root mutation probe' >&2
  exit 1
}

for required in \
  '/matrix-build-root-view-sentinel' \
  '/matrix-lifecycle-root-view-sentinel' \
  '/matrix-host-coordinates' \
  'runtime-host-read-authority' \
  'build-host-read-authority' \
  'artifact-host-read-authority' \
  'matrix-host-write-authority' \
  'matrix-build-ran' \
  'matrix-check-ran' \
  'matrix-lifecycle-ran'; do
  grep -F -- "$required" "$recipe" "$test_source" >/dev/null || {
    echo "root-authority matrix omits hostile witness: $required" >&2
    exit 1
  }
done

for required in \
  "--goal 'run=@base'" \
  "--goal 'check=matrix'" \
  "--goal 'lifecycle:post-install=matrix'" \
  'constructions 1' \
  'checks 1' \
  'require_host_root_unchanged runtime-host' \
  'require_host_root_unchanged build-host' \
  'require_host_root_unchanged artifact-host' \
  'terminal cleanup retained private realization' \
  'build matrix --check' \
  '--artifact-root "$public_artifacts"' \
  'require_host_root_unchanged public-artifact-host' \
  'public artifact root retained $public_archive_count package archives, expected 1'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "root-authority matrix process proof omits: $required" >&2
    exit 1
  }
done


# Admission matrices complement the privileged observation test. They prevent a
# future root from dropping out of the pairwise session firewall or the
# construction/check-versus-lifecycle separation firewall. The C++ tests, not
# these grep assertions, prove the actual combinations.
grep -F -- 'CHECK(cases == 84U);' "$locator_test" >/dev/null || {
  echo 'run-locator lost the exhaustive session-root overlap matrix' >&2
  exit 1
}
grep -F -- 'CHECK(lifecycle_overlap_cases == 72U);' "$runtime_test" >/dev/null || {
  echo 'native runtime lost the session/lifecycle overlap matrix' >&2
  exit 1
}

grep -F -- 'BUILD/CHECK/lifecycle × host-root authority matrix' "$testing" >/dev/null || {
  echo 'TESTING.md omits the root-authority matrix qualification' >&2
  exit 1
}
