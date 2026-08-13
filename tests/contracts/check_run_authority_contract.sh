#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_authority.h"
source="$srcdir/src/run_authority.cpp"
construction_test="$srcdir/tests/unit/construction_test.cpp"
check_test="$srcdir/tests/unit/check_test.cpp"
effect_test="$srcdir/tests/unit/effect_test.cpp"

for file in "$header" "$source" "$construction_test" "$check_test" \
            "$effect_test"; do
  [ -s "$file" ] || {
    echo "missing exact run-authority source: $file" >&2
    exit 1
  }
done

for required in \
  'operation_dispatch_execution_authority' \
  'transaction_dispatch_execution_authority_body' \
  'transaction_dispatch_recovery_authority_body' \
  'transaction_progress_rehydration_source' \
  'transaction_dispatch_session_source' \
  'transaction_operation_execution_authority_source' \
  'transaction_dispatch_execution_authority_source' \
  'composed_transaction_dispatch_execution_authority_source' \
  'transaction_operation_execution_authority_source* operations_' \
  'transaction_dispatch_recovery_authority_source' \
  'sessions_.construction(record, run.progress(), dispatch)' \
  'sessions_.check(record, run.progress(), dispatch)' \
  'operation execution requested without operation authority' \
  'operations_->operation(record, run, dispatch)' \
  'transaction_dispatch_execution_handoff' \
  'transaction_dispatch_recovery_handoff' \
  'rehydrate_transaction_run' \
  'acquire_transaction_dispatch_execution_authority' \
  'acquire_transaction_dispatch_recovery_authority' \
  'record.reopen(run.progress())' \
  'start_construction_dispatch' \
  'start_check_dispatch' \
  'start_operation_dispatch' \
  'transaction_dispatch_restart_disposition::release_reserved' \
  'transaction_dispatch_restart_disposition::recover_construction' \
  'transaction_dispatch_restart_disposition::recover_check' \
  'transaction_dispatch_restart_disposition::inspect_effect_journal' \
  'fresh execution authority requires an exact reserved dispatch' \
  'construction recovery authority belongs to another started attempt' \
  'check recovery authority belongs to another started attempt' \
  'operation recovery authority belongs to another durable attempt' \
  'pkgctl.transaction-dispatch-execution-authority.v1' \
  'pkgctl.transaction-dispatch-recovery-authority.v1'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "missing exact run-authority contract: $required" >&2
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
      echo "invalid exact run-authority order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

rehydrate_body=$(sed -n \
  '/^transaction_run_restart_checkpoint rehydrate_transaction_run(/,/^}/p' \
  "$source")
ordered_tokens "$rehydrate_body" \
  'source.rehydrate_progress(record)' \
  'transaction_run_restart_checkpoint::make'

execution_body=$(sed -n \
  '/^acquire_transaction_dispatch_execution_authority(/,/^}/p' "$source")
ordered_tokens "$execution_body" \
  'validate_record_run(record, run)' \
  'require_dispatch_record(run, dispatch)' \
  'transaction_dispatch_state::reserved' \
  'source.construction(record, run, dispatch)' \
  'start_construction_dispatch(run, dispatch, session)' \
  'source.check(record, run, dispatch)' \
  'start_check_dispatch(run, dispatch, session)' \
  'source.operation(record, run, dispatch)' \
  'start_operation_dispatch(' \
  'execution_identity(record, run, dispatch, authority)' \
  'transaction_dispatch_execution_handoff('

recovery_body=$(sed -n \
  '/^acquire_transaction_dispatch_recovery_authority(/,/^}/p' "$source")
ordered_tokens "$recovery_body" \
  'require_assessment(checkpoint, dispatch)' \
  'transaction_dispatch_restart_disposition::release_reserved' \
  'transaction_dispatch_restart_disposition::recover_construction' \
  'source.construction(checkpoint, assessment, dispatch)' \
  'validate_construction_recovery(assessment, result)' \
  'transaction_dispatch_restart_disposition::recover_check' \
  'source.check(checkpoint, assessment, dispatch)' \
  'validate_check_recovery(assessment, result)' \
  'transaction_dispatch_restart_disposition::inspect_effect_journal' \
  'source.operation(checkpoint, assessment, dispatch)' \
  'validate_operation_recovery(assessment, result)' \
  'recovery_identity(' \
  'transaction_dispatch_recovery_handoff('

for required_test in \
  'fixed_progress_source' \
  'construction_execution_authority_source' \
  'unreachable_operation_execution_authority_source' \
  'construction_recovery_authority_source' \
  'check_execution_authority_source' \
  'check_recovery_authority_source' \
  'operation_execution_authority_source' \
  'operation_recovery_authority_source' \
  'rehydrate_transaction_run' \
  'acquire_transaction_dispatch_execution_authority' \
  'acquire_transaction_dispatch_recovery_authority' \
  'releases_reserved()' \
  'construction_recovery_authority_source foreign_recovery' \
  'check_execution_authority_source alternate_execution' \
  'operation_recovery_authority_source foreign_recovery'; do
  grep -R -F "$required_test" "$construction_test" "$check_test" \
    "$effect_test" >/dev/null || {
      echo "missing exact run-authority test: $required_test" >&2
      exit 1
    }
done

for forbidden in \
  'reserve_next(' \
  'execute_construction_dispatch_durable(' \
  'execute_check_dispatch_durable(' \
  'execute_operation_dispatch_durable(' \
  'reconcile_reserved_dispatch_durable(' \
  'reconcile_construction_dispatch_durable(' \
  'reconcile_check_dispatch_durable(' \
  'reconcile_operation_dispatch_durable(' \
  'commit_transaction_run_successor(' \
  'append(' \
  'load_latest(' \
  'posix_effect_journal_store' \
  'posix_transaction_run_journal_store' \
  'std::thread' \
  'std::async' \
  'sleep('; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden exact run-authority policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'rehydrate_transaction_run|acquire_transaction_dispatch_(execution|recovery)_authority' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'exact run authority must not acquire a command frontend' >&2
  exit 1
fi
