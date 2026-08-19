#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
test="$srcdir/tests/integration/cli_run_recovery_lease_contention_test.sh"
fixture="$srcdir/tests/fixtures/native_target_lock_holder.cpp"
interrupt="$srcdir/tests/fixtures/application_intent_interrupt_fixture.cpp"
meson="$srcdir/tests/meson.build"
direct="$srcdir/tests/run-direct.sh"

for path in "$test" "$fixture" "$interrupt" "$meson" "$direct"; do
  [ -s "$path" ] || {
    echo "missing CLI recovery lease-contention qualification source: $path" >&2
    exit 1
  }
done

for required in \
  'effect.stage=application-intent' \
  'effect.disposition=resume-application' \
  'dispatch.$operation_index.state=started' \
  "'run.active=1'" \
  'dispatch.$operation_index.effect-attempt=$effect' \
  'rm -rf "$collection"' \
  'disposition mutation-authority-unavailable' \
  "'steps 1'" \
  "'durable-steps 0'" \
  'blocked-run-record' \
  'blocked-effect-record' \
  'contended resume retains $blocked_operation_count operation dispatches, expected 1' \
  'contended resume retains $blocked_effect_count effect objects, expected 1' \
  'release_holder' \
  "'durable-steps 1'" \
  'completed recovery retains $resumed_operation_count operation dispatches, expected 1' \
  'dispatch.$operation_index.identity=$operation_dispatch' \
  'effect.terminal-outcome=completed' \
  "'durable-steps 0'"; do
  grep -F -- "$required" "$test" >/dev/null || {
    echo "CLI recovery lease-contention test omits boundary assertion: $required" >&2
    exit 1
  }
done

for required in \
  'target_mutation_lease::acquire' \
  'pkgctl/native-command-mutation-domain/1' \
  'libpkgapply-posix/4.0'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "native lock holder omits recovery contention mechanism: $required" >&2
    exit 1
  }
done

grep -F 'has_active_reference' "$interrupt" >/dev/null || {
  echo 'application interruption fixture no longer proves durable application intent' >&2
  exit 1
}

for forbidden in \
  'flock ' \
  'target_mutation_lease_error' \
  'sleep 1' \
  'while true'; do
  if grep -F -- "$forbidden" "$test" >/dev/null; then
    echo "CLI recovery lease-contention test owns forbidden mechanism/policy: $forbidden" >&2
    exit 1
  fi
done

grep -F "'cli-run-recovery-lease-contention'" "$meson" >/dev/null || {
  echo 'Meson omits privileged CLI recovery lease-contention vertical' >&2
  exit 1
}
grep -F "suite: 'integration-privileged'" "$meson" >/dev/null || {
  echo 'Meson omits privileged integration suite' >&2
  exit 1
}
grep -F 'application_intent_interrupt_fixture' "$meson" >/dev/null || {
  echo 'Meson omits application-intent interruption dependency' >&2
  exit 1
}
grep -F 'native_target_lock_holder' "$meson" >/dev/null || {
  echo 'Meson omits native target lock-holder dependency' >&2
  exit 1
}
grep -F 'cli_run_recovery_lease_contention_test.sh' "$direct" >/dev/null || {
  echo 'direct compiler path omits CLI recovery lease-contention vertical' >&2
  exit 1
}
