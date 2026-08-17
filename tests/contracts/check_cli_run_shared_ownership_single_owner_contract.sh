#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
test_source=$root/tests/integration/cli_run_shared_ownership_single_owner_test.sh
fixture=$root/tests/fixtures/state_ownership_inspect_fixture.cpp
meson=$root/tests/meson.build
collection=$root/tests/fixtures/collections/shared-ownership-image

for file in "$test_source" "$fixture" "$meson"; do
  [ -s "$file" ] || {
    echo "single-owner shared-ownership qualification source is absent: $file" >&2
    exit 1
  }
done

for required in \
  '<libpkgstate-posix/canonical_generation_store.h>' \
  'state.find_package(argv[2])' \
  'package->find(path)' \
  'state.owners(path)' \
  'entry->object()' \
  'origin_name(entry->origin())' \
  'receipt.operation_plan().string()' \
  'receipt.application_evidence().string()' \
  'receipt.transaction_evidence()->string()'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "state ownership owner-side inspection omits: $required" >&2
    exit 1
  }
done

for required in \
  "--goal 'run=base-files'" \
  '--operation-policy strict-exclusive' \
  'mkdir_program=$(command -v mkdir)' \
  'inspect-run --run-store "$runtime/run" --journal "$journal"' \
  'dispatch\.\([0-9][0-9]*\)\.kind=operation' \
  'inspect-effect --effect-store "$runtime/effects" --attempt "$effect"' \
  'effect.application-outcome=completed' \
  'effect.application-completed-evidence=' \
  'effect.transaction-evidence=' \
  'effect.publication-request=' \
  'effect.publication-outcome=published' \
  'effect.publication-resulting-snapshot=' \
  '"$state_ownership_inspect_fixture"' \
  'path usr/lib/shared-ownership-marker' \
  'origin incoming-payload' \
  'owners 1' \
  'owner.0 base-files ' \
  'installed package published an empty manifest' \
  'require_equal publication-snapshot' \
  'require_equal completed-application-binding' \
  'require_equal transaction-evidence-binding' \
  'sha256sum "$target/usr/lib/shared-ownership-marker"' \
  '"$rootfs_audit_fixture" "$state" "$target"' \
  'findings 0'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "single-owner shared-ownership process proof omits: $required" >&2
    exit 1
  }
done

for forbidden in \
  'tar -t' \
  'tar -x' \
  'operation-session-' \
  'publication-request-' \
  'publication-receipt-' \
  'application-*.bin'; do
  if grep -F -- "$forbidden" "$test_source" >/dev/null 2>&1; then
    echo "single-owner proof decodes private transport instead of owner authority: $forbidden" >&2
    exit 1
  fi
done

[ ! -e "$collection/profiles.yml" ] || {
  echo 'single-owner direct-package proof must not gain fixture profile semantics' >&2
  exit 1
}

for required in \
  "'state-ownership-inspect-fixture'" \
  "'cli-run-shared-ownership-single-owner'" \
  "'integration/cli_run_shared_ownership_single_owner_test.sh'" \
  "'tests/fixtures/collections/shared-ownership-image'" \
  "suite: 'integration-privileged'" \
  "'contracts/check_cli_run_shared_ownership_single_owner_contract.sh'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "single-owner shared-ownership Meson qualification omits: $required" >&2
    exit 1
  }
done
