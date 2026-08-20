#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options="$srcdir/cli/options.cpp"
header="$srcdir/cli/options.h"
run="$srcdir/cli/run_command.cpp"
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_build_test.sh"
selection_test="$srcdir/tests/integration/build_frontend_selection_test.cpp"
options_test="$srcdir/tests/unit/cli_options_test.cpp"
readonly="$srcdir/tests/integration/cli_readonly_test.sh"

for path in "$options" "$header" "$run" "$meson" "$test_source" \
    "$selection_test" "$options_test" "$readonly"; do
  [ -s "$path" ] || {
    echo "build frontend qualification source is absent: $path" >&2
    exit 1
  }
done

for required in \
  'transaction_run_command_frontend::build' \
  'command_kind::build' \
  'build owns its build/check goals; --goal is invalid' \
  'build owns direct-subject catalog authority; global --prefer-catalog is invalid' \
  '--converge-exact is invalid for build' \
  'target-operation authority options are invalid for build' \
  'pkgsource::requirement_scope::build()' \
  'pkgsource::requirement_scope::check()'; do
  grep -F -- "$required" "$options" >/dev/null || {
    echo "build parser authority contract omits: $required" >&2
    exit 1
  }
done

count=$(grep -F -c -- 'parsed.prefer_catalog = true;' "$options" || :)
[ "$count" -eq 1 ] || {
  echo 'build parser manufactures or loses global preference option authority' >&2
  exit 1
}

for required in \
  'std::filesystem::path artifact_root;' \
  'std::optional<std::filesystem::path> lifecycle_root;' \
  'std::optional<std::filesystem::path> target_root;' \
  'std::optional<pkgexec::credential_policy> lifecycle_credentials;'; do
  grep -F -- "$required" "$header" >/dev/null || {
    echo "build command authority shape omits: $required" >&2
    exit 1
  }
done

for required in \
  'command_frontend_tag' \
  'append_text(bytes, artifact_root.string())' \
  'retained_evidence->frontend != command.frontend' \
  'retained_evidence->artifact_root != command.artifact_root' \
  'build artifact root must be disjoint from private runtime root' \
  'build frontend composed target-operation transaction authority' \
  'build frontend carries surplus target-operation authority' \
  'require_build_frontend_transaction' \
  'pkgresolve::installed_preference::retain_compatible' \
  'build frontend transaction carries global catalog preference authority' \
  'goal.members().size() != 1U' \
  'selection->identity() == *build_selection' \
  'selection->identity() == *check_selection' \
  'build frontend direct subject lacks catalog-backed construction' \
  'build frontend requested check lacks an executable check node' \
  'render_construction_artifacts' \
  'public_build_frontend' \
  'frontend build' \
  'successful construction lacks complete retained artifact authority'; do
  grep -F -- "$required" "$run" >/dev/null || {
    echo "build runtime/evidence contract omits: $required" >&2
    exit 1
  }
done

for required in \
  'installed_preference::retain_compatible' \
  'selection_authority_kind::installed_package' \
  '!has_build_node(transaction, "dep")' \
  'pkgbuild::input_scope::build' \
  'pkgbuild::input_scope::check' \
  'installed_preference::prefer_catalog' \
  'has_build_node(globally_preferred, "dep")'; do
  grep -F -- "$required" "$selection_test" >/dev/null || {
    echo "build selection authority proof omits: $required" >&2
    exit 1
  }
done

for required in \
  'installed_preference::retain_compatible' \
  'global --prefer-catalog is invalid'; do
  grep -F -- "$required" "$options_test" >/dev/null || {
    echo "build parser policy proof omits: $required" >&2
    exit 1
  }
done

for required in \
  "'build-frontend-selection'" \
  "'cli-options'" \
  "'cli-build'" \
  "'integration/cli_build_test.sh'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "build Meson qualification omits: $required" >&2
    exit 1
  }
done

for required in \
  'build tool --check' \
  'command -v chmod' \
  '"$runtime_root_fixture" "$build" /bin/sh "$chmod_program"' \
  '--artifact-root "$runtime/content"' \
  'build artifact root must be disjoint from private runtime root' \
  '--artifact-root "$artifacts"' \
  '--package-object-store "$root/package-objects"' \
  '--build-root-view' \
  '--artifact-root "$wrong_artifacts"' \
  'current artifact root differs from admitted command authority' \
  'disposition step-limit-reached' \
  'durable-steps 1' \
  'durable-steps 2' \
  'durable-steps 0' \
  'artifact.0.package dep' \
  'artifact.1.package tool' \
  'private-artifact-root "$runtime/artifacts"' \
  'dependency-payload dependency-source' \
  'tool-payload tool-source+dependency-source' \
  'build retained $package_object_count durable package objects, expected 2' \
  'package-object-cleanup' \
  'installed-resources' \
  '"$run_evidence_inspect_fixture"' \
  'construction-evidence 2' \
  'check-evidence 1' \
  'terminal cleanup retained private realization under $directory' \
  'rm -rf "$collection"'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "build process proof omits: $required" >&2
    exit 1
  }
done

for forbidden in \
  '--lifecycle-root' \
  '--lifecycle-root-view' \
  '--target-root' \
  '--lifecycle-user-id' \
  '--lifecycle-group-id'; do
  if grep -F -- "$forbidden" "$test_source" >/dev/null; then
    echo "build process proof carries forbidden target-operation authority: $forbidden" >&2
    exit 1
  fi
done
for required in \
  'pkgctl build PACKAGE [--check] OPTIONS --start SHA256 BUILD-AUTHORITY' \
  'build owns its build/check goals; --goal is invalid' \
  'target-operation authority options are invalid for build' \
  '--resume uses retained transaction semantics'; do
  grep -F -- "$required" "$readonly" >/dev/null || {
    echo "build unprivileged CLI qualification omits: $required" >&2
    exit 1
  }
done
