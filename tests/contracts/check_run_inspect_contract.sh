#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_inspect.h"
source="$srcdir/src/run_inspect.cpp"
restart_header="$srcdir/include/pkgctl/run_restart.h"
restart_source="$srcdir/src/run_restart.cpp"
report_header="$srcdir/include/pkgctl/report.h"
report_source="$srcdir/src/report.cpp"
test_source="$srcdir/tests/unit/run_journal_test.cpp"

for file in "$header" "$source" "$restart_header" "$restart_source" \
            "$report_header" "$report_source" "$test_source"; do
  [ -s "$file" ] || {
    echo "missing durable transaction-run inspection source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_run_inspection_disposition' \
  'completed = 1' \
  'stopped_after_failure = 2' \
  'active = 3' \
  'quiescent_incomplete = 4' \
  'transaction_run_inspection' \
  'inspect_transaction_run' \
  'assess_transaction_run_record' \
  'load_latest(journal)' \
  'run store returned foreign inspection authority' \
  'transaction-run inspection contradicts its durable record' \
  'render_report(const transaction_run_inspection& inspection)' \
  'session.kind=transaction-run' \
  'run.external-evidence-required=' \
  'prefix << "disposition="'; do
  grep -F "$required" "$header" "$source" "$restart_header" \
    "$restart_source" "$report_header" "$report_source" >/dev/null || {
    echo "missing durable transaction-run inspection contract: $required" >&2
    exit 1
  }
done

body=$(sed -n '/^transaction_run_inspection inspect_transaction_run(/,/^} \/\/ namespace pkgctl$/p' \
  "$source")
previous=0
for token in \
  'store.load_latest' \
  'record->journal() != journal' \
  'assess_transaction_run_record' \
  'classify' \
  'transaction_run_inspection('; do
  line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid durable transaction-run inspection order: $token" >&2
    exit 1
  }
  previous=$line
done

for required_test in \
  'check_read_only_run_inspection' \
  'inspection_store' \
  'transaction_run_inspection_disposition::quiescent_incomplete' \
  'transaction_run_inspection_disposition::active' \
  'transaction_run_inspection_disposition::' \
  'stopped_after_failure' \
  'release_reserved' \
  'recover_construction' \
  'run.disposition=quiescent-incomplete' \
  'run.disposition=stopped-after-failure' \
  'store_contract_violation' \
  'append_calls() == 0U'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing durable transaction-run inspection test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'append(' \
  'reserve_next(' \
  'advance_transaction_run_once(' \
  'drive_transaction_run(' \
  'launch_transaction_run(' \
  'rehydrate_transaction_run(' \
  'transaction_progress_source' \
  'effect_journal_store' \
  'std::thread' \
  'std::async' \
  'while (' \
  'for (' \
  'opendir(' \
  'readdir(' \
  'posix_transaction_run_journal_store'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden durable transaction-run inspection policy: $forbidden" >&2
    exit 1
  fi
done
