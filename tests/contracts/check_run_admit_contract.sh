#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_admit.h"
source="$srcdir/src/run_admit.cpp"
internal="$srcdir/src/run_admit_internal.h"
test_source="$srcdir/tests/unit/construction_test.cpp"

for file in "$header" "$source" "$internal" "$test_source"; do
  [ -s "$file" ] || {
    echo "missing durable run-admission source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_run_nonce_source' \
  'issue(' \
  'transaction_run_admission_checkpoint' \
  'admit_transaction_run' \
  'prepared_transaction_run_admission' \
  'prepare_transaction_run_admission' \
  'commit_transaction_run_admission' \
  'transaction_run::begin' \
  'transaction_run_journal_record::admit' \
  'store.append' \
  'validate_exact_committed_admission' \
  'committed.reopen' \
  'run store returned foreign transaction-run admission authority'; do
  grep -F -- "$required" "$header" "$source" "$internal" >/dev/null || {
    echo "missing durable run-admission contract: $required" >&2
    exit 1
  }
done

prepare_body=$(sed -n '/^prepared_transaction_run_admission prepare_transaction_run_admission(/,/^}/p' \
  "$source")
previous=0
for token in \
  'transaction_run::begin' \
  'nonces.issue' \
  'transaction_run_journal_record::admit'; do
  line=$(printf '%s\n' "$prepare_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid durable run-admission preparation order: $token" >&2
    exit 1
  }
  previous=$line
done

commit_body=$(sed -n '/^transaction_run_admission_checkpoint commit_transaction_run_admission(/,/^}/p' \
  "$source")
previous=0
for token in \
  'store.append' \
  'validate_exact_committed_admission' \
  'committed.reopen'; do
  line=$(printf '%s\n' "$commit_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid durable run-admission commit order: $token" >&2
    exit 1
  }
  previous=$line
done

public_body=$(sed -n '/^transaction_run_admission_checkpoint admit_transaction_run(/,/^} \/\/ namespace pkgctl$/p' \
  "$source")
previous=0
for token in \
  'prepare_transaction_run_admission' \
  'commit_transaction_run_admission'; do
  line=$(printf '%s\n' "$public_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid durable run-admission public order: $token" >&2
    exit 1
  }
  previous=$line
done

for required_test in \
  'replay_run_nonce_source' \
  'admission_run_store' \
  'check_durable_transaction_run_admission' \
  'injected run-nonce refusal' \
  'injected admission-store failure' \
  'store_contract_violation' \
  'trace == std::vector<std::string>({"nonce", "append"})' \
  '2U, expected_run.identity()'; do
  grep -F -- "$required_test" "$test_source" >/dev/null || {
    echo "missing durable run-admission test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'reserve_next' \
  'advance_transaction_run_once' \
  'drive_transaction_run' \
  'execute_reserved' \
  'reconcile_transaction_run_dispatch' \
  'effect_journal_store' \
  'std::thread' \
  'std::async' \
  'while (' \
  'sleep(' \
  'usleep(' \
  'posix_transaction_run_journal_store'; do
  if grep -F -- "$forbidden" "$header" "$source" "$internal" >/dev/null 2>&1; then
    echo "forbidden durable run-admission policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E 'admit_transaction_run|transaction_run_nonce_source' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'durable run admission must not acquire a command frontend' >&2
  exit 1
fi
