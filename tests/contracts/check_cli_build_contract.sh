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
readonly="$srcdir/tests/integration/cli_readonly_test.sh"

for path in "$options" "$header" "$run" "$meson" "$test_source" "$readonly"; do
  [ -s "$path" ] || {
    echo "build frontend qualification source is absent: $path" >&2
    exit 1
  }
done

for required in \
  'transaction_run_command_frontend::build' \
  'command_kind::build' \
  'build owns its build/check goals; --goal is invalid' \
  'build already requires catalog authority; --prefer-catalog is invalid' \
  '--converge-exact is invalid for build' \
  'target-operation authority options are invalid for build' \
  'pkgsource::requirement_scope::build()' \
  'pkgsource::requirement_scope::check()' \
  'parsed.prefer_catalog = true'; do
  grep -F -- "$required" "$options" >/dev/null || {
    echo "build parser authority contract omits: $required" >&2
    exit 1
  }
done

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
  'build frontend direct subject lacks catalog-backed construction' \
  'build frontend requested check lacks an executable check node' \
  'render_build_artifacts' \
  'frontend build' \
  'successful construction lacks complete retained artifact authority'; do
  grep -F -- "$required" "$run" >/dev/null || {
    echo "build runtime/evidence contract omits: $required" >&2
    exit 1
  }
done

for required in \
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
  '--artifact-root "$runtime/content"' \
  'build artifact root must be disjoint from private runtime root' \
  '--artifact-root "$artifacts"' \
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
  'check-payload checked:tool-source+dependency-source' \
  'rm -rf "$collection"'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "build process proof omits: $required" >&2
    exit 1
  }
done

for forbidden in \
  '--lifecycle-root' \
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
