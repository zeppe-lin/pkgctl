#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
fixture="$srcdir/tests/fixtures/native_target_lock_holder.cpp"
test="$srcdir/tests/integration/cli_run_lease_contention_test.sh"
command="$srcdir/cli/run_command.cpp"
meson="$srcdir/tests/meson.build"
direct="$srcdir/tests/run-direct.sh"

for path in "$fixture" "$test" "$command" "$meson" "$direct"; do
  [ -s "$path" ] || {
    echo "missing CLI lease-contention qualification source: $path" >&2
    exit 1
  }
done

for required in \
  'pkgctl/native-command-mutation-domain/1' \
  'libpkgapply-posix/3.1' \
  '"mutation-domain"' \
  'target_mutation_lease::acquire'; do
  grep -F "$required" "$fixture" >/dev/null || {
    echo "native lock holder omits CLI mutation-domain contract: $required" >&2
    exit 1
  }
done

for required in \
  'pkgctl/native-command-mutation-domain/1' \
  'libpkgapply-posix/3.1' \
  'fields("mutation-domain", runtime_path(command, "target-locks"))'; do
  grep -F "$required" "$command" >/dev/null || {
    echo "run command omits mutation-domain derivation: $required" >&2
    exit 1
  }
done

for required in \
  "set -- resolve" \
  "state.target-binding=" \
  "set -- transaction" \
  "--start" \
  "disposition mutation-authority-unavailable" \
  "expected_blocked_steps" \
  'durable-steps $expected_blocked_steps' \
  "released-unstarted" \
  'blocked run retains $operation_count operation dispatches, expected 1' \
  "exact transaction run is already admitted; use --resume" \
  "rm -rf \"\$collection\"" \
  "release_holder" \
  "--resume" \
  "disposition completed" \
  "durable-steps 1" \
  'completed run retains $completed_operation_count completed operation dispatches, expected 1' \
  "resume reused the released-unstarted operation dispatch"; do
  grep -F -- "$required" "$test" >/dev/null || {
    echo "CLI lease-contention test omits boundary assertion: $required" >&2
    exit 1
  }
done

for forbidden in \
  'sleep 1' \
  'while true' \
  'flock ' \
  'target_mutation_lease_error'; do
  if grep -F -- "$forbidden" "$test" >/dev/null; then
    echo "CLI lease-contention test owns forbidden mechanism/policy: $forbidden" >&2
    exit 1
  fi
done

grep -F "'cli-run-lease-contention'" "$meson" >/dev/null || {
  echo 'Meson omits privileged CLI lease-contention vertical' >&2
  exit 1
}
grep -F "suite: 'integration-privileged'" "$meson" >/dev/null || {
  echo 'Meson omits privileged integration suite' >&2
  exit 1
}
grep -F 'native_target_lock_holder' "$meson" >/dev/null || {
  echo 'Meson omits native target lock-holder fixture' >&2
  exit 1
}
grep -F 'cli_run_lease_contention_test.sh' "$direct" >/dev/null || {
  echo 'direct compiler path omits CLI lease-contention vertical' >&2
  exit 1
}
