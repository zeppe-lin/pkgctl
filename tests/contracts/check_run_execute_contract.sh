#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_execute.h"
source="$srcdir/src/run_execute.cpp"
commit_header="$srcdir/include/pkgctl/run_commit.h"
commit_source="$srcdir/src/run_commit.cpp"
support="$srcdir/tests/support/run_execute_support.h"

for file in "$header" "$source" "$commit_header" "$commit_source" "$support"; do
  [ -s "$file" ] || {
    echo "missing durable dispatch execution source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_run_commit_checkpoint' \
  'commit_transaction_run_successor' \
  'construction_dispatch_execution_checkpoint' \
  'check_dispatch_execution_checkpoint' \
  'operation_dispatch_execution_checkpoint' \
  'transaction_run_evidence_store& evidence_store' \
  'construction_dispatch_evidence_record::admit' \
  'check_dispatch_evidence_record::admit' \
  'evidence_store.publish' \
  'execute_construction_dispatch_durable' \
  'execute_check_dispatch_durable' \
  'execute_operation_dispatch_durable' \
  'start_construction_dispatch' \
  'start_check_dispatch' \
  'commit_operation_dispatch_start' \
  'execute_construction' \
  'execute_transaction_check' \
  'execute_effectful_operation_durable' \
  'complete_construction_dispatch' \
  'complete_check_dispatch' \
  'submit_operation_dispatch_result' \
  'transaction_effect_state_observer& resulting_state' \
  'validate_target_mutation_lease_scope' \
  'resulting_state.read_state()' \
  'run store returned foreign committed authority'; do
  grep -F "$required" "$header" "$source" "$commit_header" "$commit_source" \
    >/dev/null || {
      echo "missing durable dispatch execution contract: $required" >&2
      exit 1
    }
done

ordered_tokens()
{
  body=$1
  shift
  previous=0
  for token in "$@"; do
    line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" 'NR > previous && index($0, token) { print NR; exit }')
    [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
      echo "invalid durable dispatch execution order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

construction_body=$(sed -n \
  '/^execute_construction_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$construction_body" \
  'start_construction_dispatch' \
  'commit_transaction_run_successor' \
  'execute_construction' \
  'construction_dispatch_evidence_record::admit' \
  'evidence_store.publish' \
  'complete_construction_dispatch' \
  'commit_transaction_run_successor'

check_body=$(sed -n '/^execute_check_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$check_body" \
  'start_check_dispatch' \
  'commit_transaction_run_successor' \
  'execute_transaction_check' \
  'check_dispatch_evidence_record::admit' \
  'evidence_store.publish' \
  'complete_check_dispatch' \
  'commit_transaction_run_successor'

operation_body=$(sed -n \
  '/^execute_operation_dispatch_durable(/,/^}/p' "$source")
ordered_tokens "$operation_body" \
  'commit_operation_dispatch_start' \
  'validate_target_mutation_lease_scope' \
  'execute_effectful_operation_durable' \
  'resulting_state.read_state()' \
  'submit_operation_dispatch_result' \
  'commit_transaction_run_successor'

for required_test in \
  'injected run-store failure' \
  'injected effect-store failure' \
  'injected construction-evidence failure' \
  'injected check-evidence failure' \
  'evidence-construction' \
  'evidence-check' \
  'recover_construction' \
  'recover_check' \
  'inspect_effect_journal' \
  'application_intent' \
  'driver escaped without materialization evidence' \
  'run-1' \
  'run-2'; do
  grep -R -F "$required_test" \
    "$srcdir/tests/unit/construction_test.cpp" \
    "$srcdir/tests/unit/check_test.cpp" \
    "$srcdir/tests/unit/effect_test.cpp" \
    "$support" >/dev/null || {
      echo "missing durable dispatch execution test: $required_test" >&2
      exit 1
    }
done

for forbidden in \
  'reserve_next(' \
  'release_unstarted_dispatch(' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'while (' \
  'for ('; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden durable dispatch execution policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'execute_(construction|check|operation)_dispatch_durable' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'durable dispatch execution must not acquire a command frontend' >&2
  exit 1
fi
