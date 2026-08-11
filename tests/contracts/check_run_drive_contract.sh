#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_drive.h"
source="$srcdir/src/run_drive.cpp"
advance_header="$srcdir/include/pkgctl/run_advance.h"
advance_source="$srcdir/src/run_advance.cpp"
construction_test="$srcdir/tests/unit/construction_test.cpp"
effect_test="$srcdir/tests/unit/effect_test.cpp"

for file in "$header" "$source" "$advance_header" "$advance_source" \
            "$construction_test" "$effect_test"; do
  [ -s "$file" ] || {
    echo "missing bounded transaction-drive source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_dispatch_nonce_source' \
  'issue(' \
  'transaction_run_drive_policy' \
  'maximum_steps' \
  'transaction_run_drive_disposition' \
  'completed' \
  'stopped_after_failure' \
  'external_resolution_required' \
  'quiescent_incomplete' \
  'step_limit_reached' \
  'mutation_authority_unavailable' \
  'transaction_run_drive_result' \
  'retained_operation_requires_external_resolution' \
  'transaction_dispatch_state::started' \
  'operation->result->identity()' \
  'record->observations()' \
  'drive_transaction_run' \
  'advance_transaction_run_once' \
  'policy.maximum_steps()' \
  'transaction drive requires a positive step bound' \
  'transaction drive did not advance the durable run head'; do
  grep -F -- "$required" "$header" "$source" "$advance_header" \
      "$advance_source" >/dev/null || {
    echo "missing bounded transaction-drive contract: $required" >&2
    exit 1
  }
done

advance_body=$(sed -n \
  '/^transaction_run_advance_result advance_loaded_transaction_run_once(/,/^} \/\/ namespace$/p' \
  "$advance_source")
ordered_tokens()
{
  body=$1
  shift
  previous=0
  for token in "$@"; do
    line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
      'NR > previous && index($0, token) { print NR; exit }')
    [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
      echo "invalid bounded transaction-drive order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

ordered_tokens "$advance_body" \
  'load_committed_head' \
  'rehydrate_transaction_run' \
  'assessment().active().empty()' \
  'reconcile_active' \
  'ready_units().empty()' \
  'nonce_source->issue' \
  'reserve_next' \
  'require_execution_dependencies' \
  'commit_transaction_run_successor' \
  'execute_reserved'

drive_body=$(sed -n '/^transaction_run_drive_result drive_transaction_run(/,$p' \
  "$source")
ordered_tokens "$drive_body" \
  'steps.reserve(policy.maximum_steps())' \
  'index < policy.maximum_steps()' \
  'advance_transaction_run_once' \
  'classify_stop' \
  'steps.push_back' \
  'stops_before_limit' \
  'step_limit_reached'

for required_test in \
  'head_derived_nonce_source' \
  'drive_transaction_run' \
  'stopped_after_failure' \
  'step_limit_reached' \
  'released_reserved' \
  'nonces.calls() == 2U' \
  'std::vector<pkgctl::session_identity>(2U, admitted.identity())' \
  'nonces.calls() == 0U' \
  'forbidden_dispatch_nonce_source' \
  'effectful_operation_outcome::outer_lease_lost' \
  'durable_step_count() == 1U' \
  'external_resolution_required' \
  'mutation_authority_unavailable'; do
  grep -F -- "$required_test" "$construction_test" "$effect_test" >/dev/null || {
    echo "missing bounded transaction-drive test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'while (' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'usleep(' \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'posix_effect_journal_store' \
  'posix_transaction_run_journal_store'; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden bounded transaction-drive policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E 'drive_transaction_run|transaction_dispatch_nonce_source' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'bounded transaction drive must not acquire a command frontend' >&2
  exit 1
fi
