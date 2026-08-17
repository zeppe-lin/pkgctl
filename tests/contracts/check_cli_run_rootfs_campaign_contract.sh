#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_run_rootfs_campaign_test.sh"
fixture_root="$srcdir/tests/fixtures/collections/rootfs-campaign"
profiles="$fixture_root/profiles.yml"
build_tool="$fixture_root/build-tool/recipe.yml"
base_files="$fixture_root/base-files/recipe.yml"
runtime_lib="$fixture_root/runtime-lib/recipe.yml"
rootfs_probe="$fixture_root/rootfs-probe/recipe.yml"
state_fixture="$srcdir/tests/fixtures/state_fixture.cpp"
audit_fixture="$srcdir/tests/fixtures/rootfs_audit_fixture.cpp"

fail()
{
  echo "cli-run-rootfs-campaign-contract: $*" >&2
  exit 1
}

for path in \
  "$test_source" \
  "$profiles" \
  "$build_tool" \
  "$base_files" \
  "$runtime_lib" \
  "$rootfs_probe" \
  "$state_fixture" \
  "$audit_fixture"; do
  [ -s "$path" ] || fail "qualification source is absent: $path"
done

for required in \
  "'cli-run-rootfs-campaign'" \
  "'integration/cli_run_rootfs_campaign_test.sh'" \
  "'tests/fixtures/collections/rootfs-campaign'" \
  "'rootfs-audit-fixture'" \
  "'libpkgaudit'" \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || \
    fail "Meson qualification omits: $required"
done

for required in \
  "--goal 'build=@rootfs-test'" \
  "--goal 'run=@rootfs-test'" \
  "--goal 'check=rootfs-probe'" \
  '--converge-exact' \
  'artifacts 4' \
  'artifact.0.package base-files' \
  'artifact.1.package build-tool' \
  'artifact.2.package rootfs-probe' \
  'artifact.3.package runtime-lib' \
  'artifact.0.path $runtime/artifacts/' \
  "'frontend build'" \
  'foreign-not-construction-authority' \
  'artifact_field' \
  'report names absent bytes' \
  'foreign-after-terminal.tar' \
  'origin resumed' \
  'durable-steps 0' \
  'terminal-resume-artifact-' \
  'packages 3' \
  'package base-files 1.0-1' \
  'package runtime-lib 1.0-1' \
  'package rootfs-probe 1.0-1' \
  "'package build-tool '" \
  'build-only-target "$target/build-tool-token"' \
  'package archives, expected 4' \
  '"$run_evidence_inspect_fixture"' \
  'construction-evidence 4' \
  'check-evidence 1' \
  'terminal cleanup retained private realization under $directory' \
  '"$rootfs_audit_fixture" "$state" "$target"' \
  'findings 0' \
  'finding missing-object runtime-lib runtime-lib-marker'; do
  grep -F -- "$required" "$test_source" >/dev/null || \
    fail "process proof omits: $required"
done

for required in \
  'require_absent initial-canonical-store "$state"' \
  'binding=$("$state_fixture" "$state")' \
  'packages 0'; do
  grep -F -- "$required" "$test_source" >/dev/null || \
    fail "empty-state bootstrap proof omits: $required"
done

for required in \
  '"$root_view_fixture" "$build"' \
  '"$root_view_fixture" "$lifecycle"' \
  '"$runtime_root_fixture" "$build" /bin/sh' \
  '"$runtime_root_fixture" "$lifecycle" /bin/sh'; do
  grep -F -- "$required" "$test_source" >/dev/null || \
    fail "real native runtime proof omits: $required"
done
if grep -F -- 'native-test-interpreter' "$test_source" >/dev/null; then
  fail 'rootfs campaign regressed to the synthetic interpreter fixture'
fi

for required in \
  'members:' \
  '- package: base-files' \
  '- package: rootfs-probe'; do
  grep -F -- "$required" "$profiles" >/dev/null || \
    fail "rootfs profile omits: $required"
done
for forbidden in 'package: build-tool' 'package: runtime-lib'; do
  if grep -F -- "$forbidden" "$profiles" >/dev/null; then
    fail "rootfs profile directly names transitive-only package: $forbidden"
  fi
done

for required in \
  'build:' \
  '- package: build-tool' \
  'run:' \
  '- package: runtime-lib' \
  '$PKG_BUILD_INPUT_ROOT/build-tool/build-tool-token' \
  '$PKG_DESTDIR/rootfs-probe-marker' \
  '$PKG_PACKAGE_ROOT/rootfs-probe-marker' \
  '/tmp/rootfs-check-ran'; do
  grep -F -- "$required" "$rootfs_probe" >/dev/null || \
    fail "scope-separation probe omits: $required"
done

for pair in \
  "$build_tool|build-tool-token" \
  "$base_files|base-files-marker" \
  "$runtime_lib|runtime-lib-marker"; do
  file=${pair%%|*}
  marker=${pair#*|}
  grep -F -- "$marker" "$file" >/dev/null || \
    fail "fixture recipe omits expected package marker: $marker"
done

grep -F -- 'canonical_generation_store store(root, binding());' \
  "$srcdir/tests/support/test_support.h" >/dev/null || \
  fail 'state fixture no longer uses explicit provider initialization authority'

for required in \
  'pkgaudit::check::object_state' \
  'pkgaudit::check::symlink_resolution' \
  'pkgaudit::check::symlink_ownership' \
  'pkgaudit::auditor().run' \
  'make_posix_filesystem_backend' \
  'canonical_generation_store::open_existing'; do
  grep -F -- "$required" "$audit_fixture" >/dev/null || \
    fail "independent audit oracle omits: $required"
done
