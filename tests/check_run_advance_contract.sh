#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_advance.h"
source="$srcdir/src/run_advance.cpp"
construction_test="$srcdir/tests/construction_test.cpp"
check_test="$srcdir/tests/check_test.cpp"
effect_test="$srcdir/tests/effect_test.cpp"

for file in "$header" "$source" "$construction_test" "$check_test" \
            "$effect_test"; do
  [ -s "$file" ] || {
    echo "missing one-step transaction-advancement source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_dispatch_nonce_source' \
  'transaction_run_advance_authorities' \
  'transaction_run_advance_drivers' \
  'transaction_run_advance_stores' \
  'transaction_run_advance_disposition' \
  'transaction_run_operation_advance_evidence' \
  'transaction_run_advance_evidence' \
  'transaction_run_advance_result' \
  'advance_transaction_run_once' \
  'load_latest(journal)' \
  'rehydrate_transaction_run' \
  'assessment().active().front()' \
  'acquire_transaction_dispatch_recovery_authority' \
  'reconcile_reserved_dispatch_durable' \
  'reconcile_construction_dispatch_durable' \
  'reconcile_check_dispatch_durable' \
  'reconcile_operation_dispatch_durable' \
  'reserve_next' \
  'require_execution_dependencies' \
  'commit_transaction_run_successor' \
  'acquire_transaction_dispatch_execution_authority' \
  'execute_construction_dispatch_durable' \
  'execute_check_dispatch_durable' \
  'execute_operation_dispatch_durable' \
  'run advancement journal has no committed store head' \
  'run store returned foreign advancement authority' \
  'quiescent advancement carries dispatch or semantic evidence' \
  'externally blocked advancement has malformed effect evidence'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing one-step transaction-advancement contract: $required" >&2
    exit 1
  }
done

ordered_tokens()
{
  body=$1
  shift
  previous=0
  for token in "$@"; do
    line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
      'NR > previous && index($0, token) { print NR; exit }')
    [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
      echo "invalid one-step transaction-advancement order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

advance_body=$(sed -n \
  '/^transaction_run_advance_result advance_loaded_transaction_run_once(/,/^} \/\/ namespace$/p' \
  "$source")
ordered_tokens "$advance_body" \
  'load_committed_head' \
  'rehydrate_transaction_run' \
  'assessment().active().empty()' \
  'assessment().active().front()' \
  'reconcile_active' \
  'ready_units().empty()' \
  'reserve_next' \
  'require_execution_dependencies' \
  'commit_transaction_run_successor' \
  'execute_reserved'

reconcile_body=$(sed -n \
  '/^transaction_run_advance_result reconcile_active(/,/^void validate_advance_result(/p' \
  "$source")
ordered_tokens "$reconcile_body" \
  'acquire_transaction_dispatch_recovery_authority' \
  'reconcile_reserved_dispatch_durable' \
  'reconcile_construction_dispatch_durable' \
  'reconcile_check_dispatch_durable' \
  'reconcile_operation_dispatch_durable' \
  'external_resolution_required'

execute_body=$(sed -n \
  '/^transaction_run_advance_result execute_reserved(/,/^} \/\/ namespace$/p' \
  "$source")
ordered_tokens "$execute_body" \
  'acquire_transaction_dispatch_execution_authority' \
  'execute_construction_dispatch_durable' \
  'execute_check_dispatch_durable' \
  'execute_operation_dispatch_durable'

for required_test in \
  'advance_transaction_run_once' \
  'executed_construction' \
  'released_reserved' \
  'reconciled_construction' \
  'quiescent' \
  'injected execution-authority failure' \
  'executed_check' \
  'reconciled_check' \
  'executed_operation' \
  'reconciled_operation' \
  'external_resolution_required' \
  'run-1' \
  'run-2' \
  'run-3'; do
  grep -R -F "$required_test" "$construction_test" "$check_test" \
    "$effect_test" >/dev/null || {
      echo "missing one-step transaction-advancement test: $required_test" >&2
      exit 1
    }
done

for forbidden in \
  'while (' \
  'for (' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'posix_effect_journal_store' \
  'posix_transaction_run_journal_store'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden one-step transaction-advancement policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -F 'advance_transaction_run_once' "$srcdir/cli" \
    >/dev/null 2>&1; then
  echo 'one-step transaction advancement must not acquire a command frontend' >&2
  exit 1
fi
