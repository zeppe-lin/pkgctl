#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
test_source=$root/tests/integration/cli_run_shared_ownership_compatible_owner_test.sh
fixture=$root/tests/fixtures/state_ownership_inspect_fixture.cpp
meson=$root/tests/meson.build
collection=$root/tests/fixtures/collections/shared-ownership-image

fail()
{
  echo "compatible-owner shared-ownership contract: $*" >&2
  exit 1
}

for file in "$test_source" "$fixture" "$meson"; do
  [ -s "$file" ] || fail "qualification source is absent: $file"
done
[ -d "$collection" ] || fail 'qualified Layer-1 fixture collection is absent'
[ ! -e "$collection/profiles.yml" ] || \
  fail 'compatible-owner direct-package proof must not gain fixture profile semantics'

for required in \
  'run_package base base-files strict-exclusive 51' \
  'run_package compatible runtime-lib exact-compatible-sharing 52' \
  'artifact.0.package $package' \
  "'artifact.0.package base-files'" \
  'inspect-run --run-store "$runtime/run" --journal "$journal"' \
  'inspect-effect --effect-store "$runtime/effects" --attempt "$effect"' \
  'effect.application-outcome=completed' \
  'effect.application-completed-evidence=' \
  'effect.transaction-evidence=' \
  'effect.publication-outcome=published' \
  'effect.publication-resulting-snapshot=' \
  'origin retained-existing' \
  'owners 2' \
  'owner.0 base-files ' \
  'owner.1 runtime-lib ' \
  'compatible-publication-snapshot' \
  'compatible-application-binding' \
  'compatible-transaction-binding' \
  'base-package-stability' \
  'base-plan-stability' \
  'base-application-stability' \
  'base-transaction-stability' \
  "stat -c '%d:%i:%f:%u:%g:%s:%y:%z'" \
  'shared-path-no-rewrite' \
  'shared-path-payload' \
  'shared-path-digest' \
  'runtime-payload runtime-lib-source' \
  'packages 2' \
  'package base-files 1.0-1' \
  'package runtime-lib 1.0-1' \
  '"$rootfs_audit_fixture" "$state" "$target"' \
  'findings 0' \
  'failures 0'; do
  grep -F -- "$required" "$test_source" >/dev/null || \
    fail "compatible-owner process proof omits: $required"
done

# Owner-side state vocabulary is the only semantic bridge used to distinguish
# activation from retained sharing. The shell test may correlate identities and
# make hostile target observations, but it must not decode foreign durable bytes.
for required in \
  'state.find_package(argv[2])' \
  'package->find(path)' \
  'state.owners(path)' \
  'origin_name(entry->origin())' \
  'receipt.operation_plan().string()' \
  'receipt.application_evidence().string()' \
  'receipt.transaction_evidence()->string()'; do
  grep -F -- "$required" "$fixture" >/dev/null || \
    fail "owner-side state inspection omits: $required"
done

for forbidden in \
  'tar -t' \
  'tar -x' \
  'operation-session-' \
  'publication-request-' \
  'publication-receipt-' \
  'application-*.bin' \
  'completed-*.bin' \
  'effect-bodies/'; do
  if grep -F -- "$forbidden" "$test_source" >/dev/null 2>&1; then
    fail "compatible-owner proof decodes private transport instead of owner authority: $forbidden"
  fi
done

for required in \
  "'cli-run-shared-ownership-compatible-owner'" \
  "'integration/cli_run_shared_ownership_compatible_owner_test.sh'" \
  "'tests/fixtures/collections/shared-ownership-image'" \
  "suite: 'integration-privileged'" \
  "'contracts/check_cli_run_shared_ownership_compatible_owner_contract.sh'"; do
  grep -F -- "$required" "$meson" >/dev/null || \
    fail "compatible-owner Meson qualification omits: $required"
done
