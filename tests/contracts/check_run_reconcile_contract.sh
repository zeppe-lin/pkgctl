#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_reconcile.h"
source="$srcdir/src/run_reconcile.cpp"
restart_header="$srcdir/include/pkgctl/run_restart.h"
restart_source="$srcdir/src/run_restart.cpp"
effect_restart="$srcdir/src/effect_restart.cpp"
effect="$srcdir/src/effect.cpp"

for file in "$header" "$source" "$restart_header" "$restart_source" \
            "$effect_restart" "$effect"; do
  [ -s "$file" ] || {
    echo "missing durable restart-reconciliation source: $file" >&2
    exit 1
  }
done

for required in \
  'reserved_dispatch_reconciliation_checkpoint' \
  'construction_dispatch_reconciliation_checkpoint' \
  'check_dispatch_reconciliation_checkpoint' \
  'operation_dispatch_reconciliation_result' \
  'reconcile_reserved_dispatch_durable' \
  'reconcile_construction_dispatch_durable' \
  'reconcile_check_dispatch_durable' \
  'reconcile_operation_dispatch_durable' \
  'operation_reconciliation_requires_continuation_driver' \
  'operation_reconciliation_requires_state_observer' \
  'operation_reconciliation_requires_publication_driver' \
  'transaction_dispatch_restart_disposition::release_reserved' \
  'transaction_dispatch_restart_disposition::recover_construction' \
  'transaction_dispatch_restart_disposition::recover_check' \
  'transaction_dispatch_restart_disposition::inspect_effect_journal' \
  'recovered evidence belongs to another started dispatch session' \
  'effect checkpoint belongs to another durable operation attempt' \
  'effect reconciliation checkpoint is not the latest durable record' \
  'dispatch_already_observes' \
  'record->observations()' \
  'assess_effect_restart' \
  'resume_effectful_operation' \
  'effect_restart_disposition::external_resolution_required' \
  'release_unstarted_dispatch' \
  'complete_construction_dispatch' \
  'complete_check_dispatch' \
  'submit_operation_dispatch_result' \
  'commit_transaction_run_successor' \
  'detail::rehydrate_terminal_effectful_operation' \
  'checkpoint.publication_request()->transaction_evidence()'; do
  grep -F -- "$required" "$header" "$source" "$restart_header" \
      "$restart_source" "$effect_restart" "$effect" >/dev/null || {
    echo "missing durable restart-reconciliation contract: $required" >&2
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
      echo "invalid durable restart-reconciliation order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

reserved_body=$(sed -n \
  '/^reconcile_reserved_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$reserved_body" \
  'require_assessment' \
  'release_unstarted_dispatch' \
  'commit_transaction_run_successor'

construction_body=$(sed -n \
  '/^reconcile_construction_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$construction_body" \
  'require_assessment' \
  'validate_started_attempt' \
  'complete_construction_dispatch' \
  'commit_transaction_run_successor'

check_body=$(sed -n \
  '/^reconcile_check_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$check_body" \
  'require_assessment' \
  'validate_started_attempt' \
  'complete_check_dispatch' \
  'commit_transaction_run_successor'

operation_body=$(sed -n \
  '/^reconcile_operation_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$operation_body" \
  'require_assessment' \
  'validate_started_attempt' \
  'require_latest_effect_record' \
  'assess_effect_restart' \
  'automatically_continuable' \
  'resume_effectful_operation' \
  'dispatch_already_observes' \
  'effect_restart_disposition::external_resolution_required' \
  'validate_target_mutation_lease_scope' \
  'observer->read_state()' \
  'submit_operation_dispatch_result' \
  'commit_transaction_run_successor'

terminal_body=$(sed -n \
  '/^detail::rehydrate_terminal_effectful_operation(/,/^}/p' "$effect")
ordered_tokens "$terminal_body" \
  'effect_attempt_stage::terminal' \
  'checkpoint.publication_request()->transaction_evidence()' \
  'detail_effect_rehydration_access::seal' \
  'std::move(transaction)'

for required_test in \
  'reconcile_reserved_dispatch_durable' \
  'reconcile_construction_dispatch_durable' \
  'reconcile_check_dispatch_durable' \
  'reconcile_operation_dispatch_durable' \
  'external_resolution_required' \
  'identity() == result.identity()' \
  'effectful_operation_outcome::outer_lease_lost' \
  'durable_observation' \
  'injected run-store failure'; do
  grep -R -F "$required_test" \
    "$srcdir/tests/unit/construction_test.cpp" \
    "$srcdir/tests/unit/check_test.cpp" \
    "$srcdir/tests/unit/effect_test.cpp" >/dev/null || {
      echo "missing durable restart-reconciliation test: $required_test" >&2
      exit 1
    }
done

for forbidden in \
  'reserve_next(' \
  'execute_construction(' \
  'execute_transaction_check(' \
  'execute_effectful_operation_durable(' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'posix_effect_journal_store' \
  'posix_transaction_run_journal_store'; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden durable restart-reconciliation policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'reconcile_(reserved|construction|check|operation)_dispatch_durable' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'durable restart reconciliation must not acquire a command frontend' >&2
  exit 1
fi
