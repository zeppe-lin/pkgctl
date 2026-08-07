#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_advance.h"
source="$srcdir/src/run_advance.cpp"
effect_header="$srcdir/include/pkgctl/effect.h"
restart_header="$srcdir/include/pkgctl/effect_restart.h"
reconcile_header="$srcdir/include/pkgctl/run_reconcile.h"
test_source="$srcdir/tests/unit/effect_test.cpp"

for file in "$header" "$source" "$effect_header" "$restart_header" \
            "$reconcile_header" "$test_source"; do
  [ -s "$file" ] || {
    echo "missing split effect-authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_effect_state_observer' \
  'class transaction_effect_publication_driver' \
  'struct transaction_effect_execution_drivers final' \
  'struct transaction_effect_recovery_drivers final' \
  'class transaction_effect_driver_source' \
  'acquire_execution_drivers(' \
  'acquire_recovery_drivers(' \
  'transaction_effect_driver_source* operation;' \
  'effect_restart_requires_continuation_driver(' \
  'effect_restart_requires_publication_driver(' \
  'operation_reconciliation_requires_continuation_driver(' \
  'operation_reconciliation_requires_state_observer(' \
  'operation_reconciliation_requires_publication_driver(' \
  'validate_target_mutation_lease_scope' \
  'continuation and resulting-state authorities use different leases' \
  'operation recovery source returned the wrong continuation authority' \
  'operation recovery source returned the wrong state-observer authority' \
  'operation recovery source returned the wrong publication authority'; do
  grep -F "$required" "$header" "$source" "$effect_header" \
      "$restart_header" "$reconcile_header" >/dev/null || {
    echo "missing split effect-authority contract: $required" >&2
    exit 1
  }
done

function_body()
{
  name=$1
  file=$2
  sed -n "/$name(/,/^}/p" "$file"
}

ordered_tokens()
{
  body=$1
  shift
  previous=0
  for token in "$@"; do
    line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
      'NR > previous && index($0, token) { print NR; exit }')
    [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
      echo "invalid split effect-authority order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

reservation_body=$(function_body advance_loaded_transaction_run_once "$source")
ordered_tokens "$reservation_body" \
  'reserve_next' \
  'commit_transaction_run_successor' \
  'execute_reserved'

execution_body=$(function_body execute_reserved "$source")
ordered_tokens "$execution_body" \
  'acquire_transaction_dispatch_execution_authority' \
  'acquire_execution_drivers' \
  'execute_operation_dispatch_durable'

acquisition_body=$(function_body acquire_execution_drivers "$source")
ordered_tokens "$acquisition_body" \
  'source.acquire_execution_drivers' \
  'validate_operation_driver' \
  'validate_state_observer' \
  'validate_shared_lease'

recovery_body=$(function_body acquire_recovery_drivers "$source")
ordered_tokens "$recovery_body" \
  'operation_reconciliation_requires_continuation_driver' \
  'operation_reconciliation_requires_state_observer' \
  'operation_reconciliation_requires_publication_driver' \
  'source.acquire_recovery_drivers'

for required_test in \
  'driver-source-execution' \
  'driver-source-recovery' \
  'state-observer' \
  'publication-driver' \
  'driver-source-rejected' \
  'injected effect-driver source refusal' \
  'value.outer_lease.release()' \
  'driver_source.recovery_calls() == 0U' \
  'lifecycle_failed_before_application'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing split effect-authority test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'transaction_effect_driver* operation;' \
  'std::shared_ptr<transaction_effect_driver>' \
  'static transaction_effect_driver' \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'posix::target_mutation_lease' \
  'std::thread' \
  'std::async' \
  'sleep('; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden split effect-authority shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'transaction_effect_driver_source|acquire_(execution|recovery)_drivers' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'split effect authority must not acquire a CLI' >&2
  exit 1
fi
