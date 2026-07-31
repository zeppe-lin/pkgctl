#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_launch.h"
source="$srcdir/src/run_launch.cpp"
internal="$srcdir/src/run_admit_internal.h"
admit_source="$srcdir/src/run_admit.cpp"
test_source="$srcdir/tests/construction_test.cpp"

for file in "$header" "$source" "$internal" "$admit_source" "$test_source"; do
  [ -s "$file" ] || {
    echo "missing restart-safe transaction-launch source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_run_launch_origin' \
  'admitted = 1' \
  'resumed = 2' \
  'transaction_run_launch_result' \
  'starting_record' \
  'admission_committed' \
  'launch_transaction_run' \
  'prepare_transaction_run_admission' \
  'load_latest(prepared.record.journal())' \
  'validate_existing_transaction_run_admission' \
  'commit_transaction_run_admission' \
  'drive_transaction_run' \
  'transaction launch crossed durable admission authority' \
  'transaction launch moved behind its starting record'; do
  grep -F "$required" "$header" "$source" "$internal" "$admit_source" \
    >/dev/null || {
    echo "missing restart-safe transaction-launch contract: $required" >&2
    exit 1
  }
done

body=$(sed -n '/^transaction_run_launch_result launch_transaction_run(/,/^} \/\/ namespace pkgctl$/p' \
  "$source")
previous=0
for token in \
  'prepare_transaction_run_admission' \
  'load_latest' \
  'if (existing)' \
  'validate_existing_transaction_run_admission' \
  'commit_transaction_run_admission' \
  'drive_transaction_run'; do
  line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid restart-safe transaction-launch order: $token" >&2
    exit 1
  }
  previous=$line
done

for required_test in \
  'check_restart_safe_transaction_launch' \
  'launch_run_store' \
  'foreign_launch_head_store' \
  'transaction_run_launch_origin::admitted' \
  'transaction_run_launch_origin::resumed' \
  'run_store.append_calls() == append_count' \
  'progress_source.calls() == 0U' \
  'dispatch_nonces.calls() == 0U' \
  'run_store.latest().sequence() == 0U' \
  'store_contract_violation'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing restart-safe transaction-launch test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'while (' \
  'for (' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'usleep(' \
  'opendir(' \
  'readdir(' \
  'canonical_generation_store' \
  'posix_effect_journal_store' \
  'posix_transaction_run_journal_store'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden restart-safe transaction-launch policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -F 'launch_transaction_run' "$srcdir/cli" >/dev/null 2>&1; then
  echo 'restart-safe transaction launch must not acquire a command frontend' >&2
  exit 1
fi
