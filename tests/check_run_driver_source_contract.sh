#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_advance.h"
source="$srcdir/src/run_advance.cpp"
restart_header="$srcdir/include/pkgctl/effect_restart.h"
restart_source="$srcdir/src/effect_restart.cpp"
reconcile_header="$srcdir/include/pkgctl/run_reconcile.h"
reconcile_source="$srcdir/src/run_reconcile.cpp"
test_source="$srcdir/tests/effect_test.cpp"

for file in "$header" "$source" "$restart_header" "$restart_source" \
            "$reconcile_header" "$reconcile_source" "$test_source"; do
  [ -s "$file" ] || {
    echo "missing per-dispatch effect-driver source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_effect_driver_source' \
  'std::unique_ptr<transaction_effect_driver>' \
  'acquire_execution_driver(' \
  'acquire_recovery_driver(' \
  'transaction_effect_driver_source* operation;' \
  'effect_restart_requires_driver(' \
  'operation_reconciliation_requires_driver(' \
  'transaction_effect_driver* driver,' \
  'validate_target_mutation_lease' \
  'operation driver state projection belongs to another state epoch' \
  'operation effect driver source returned no execution driver' \
  'operation effect driver source returned no recovery driver'; do
  grep -F "$required" "$header" "$source" "$restart_header" \
      "$restart_source" "$reconcile_header" "$reconcile_source" \
      >/dev/null || {
    echo "missing per-dispatch effect-driver contract: $required" >&2
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
      echo "invalid per-dispatch effect-driver order: $token" >&2
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
  'acquire_execution_driver' \
  'execute_operation_dispatch_durable'

recovery_body=$(function_body reconcile_active "$source")
ordered_tokens "$recovery_body" \
  'acquire_transaction_dispatch_recovery_authority' \
  'operation_reconciliation_requires_driver' \
  'acquire_recovery_driver' \
  'reconcile_operation_dispatch_durable'

validation_body=$(function_body acquire_execution_driver "$source")
ordered_tokens "$validation_body" \
  'source.acquire_execution_driver' \
  'validate_operation_driver'

for required_test in \
  'driver-source-execution' \
  'driver-source-recovery' \
  'driver-source-rejected' \
  'injected effect-driver source refusal' \
  'value.outer_lease.release()' \
  'driver_source.recovery_calls() == 0U' \
  'lifecycle_failed_before_application'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing per-dispatch effect-driver test: $required_test" >&2
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
    echo "forbidden per-dispatch effect-driver shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'transaction_effect_driver_source|acquire_(execution|recovery)_driver' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'per-dispatch effect-driver authority must not acquire a CLI' >&2
  exit 1
fi
