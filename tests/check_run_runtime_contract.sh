#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_runtime.h"
source="$srcdir/src/run_runtime.cpp"
test_source="$srcdir/tests/construction_test.cpp"
meson="$srcdir/src/meson.build"
readme="$srcdir/README.md"
design="$srcdir/DESIGN.md"
manual="$srcdir/man/pkgctl_orchestration.7.scd"

for file in "$header" "$source" "$test_source" "$meson" \
            "$readme" "$design" "$manual"; do
  [ -s "$file" ] || {
    echo "missing POSIX transaction-run runtime source: $file" >&2
    exit 1
  }
done

for required in \
  'struct transaction_run_runtime_authorities final' \
  'transaction_run_nonce run_nonce' \
  'canonical_transaction_dispatch_nonce_source dispatch_nonces_' \
  'transaction_progress_rehydration_source& progress' \
  'transaction_dispatch_execution_authority_source& execution' \
  'transaction_dispatch_recovery_authority_source& recovery' \
  'transaction_effect_archive_source& archives' \
  'struct transaction_run_runtime_backends final' \
  'class posix_transaction_run_runtime final' \
  'from_directory_fds(' \
  'transaction_run_launch_result launch(' \
  'explicit_run_nonce_source' \
  'transaction_run_drive_result drive(' \
  'posix_transaction_run_journal_store::from_directory_fd' \
  'posix_effect_journal_store::from_directory_fd' \
  'posix_transaction_effect_driver_source::from_lock_directory_fd' \
  'native_construction_driver' \
  'native_transaction_check_driver' \
  'launch_transaction_run(' \
  'drive_transaction_run('; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing POSIX transaction-run runtime contract: $required" >&2
    exit 1
  }
done

for forbidden_authority in \
  'transaction_run_nonce_source& run_nonces' \
  'transaction_dispatch_nonce_source& dispatch_nonces'; do
  if grep -F "$forbidden_authority" "$header" >/dev/null 2>&1; then
    echo "runtime must not borrow nonce service: $forbidden_authority" >&2
    exit 1
  fi
done

body=$(sed -n '/implementation(/,/^  }/p' "$source")
previous=0
for token in \
  'posix_transaction_run_journal_store::from_directory_fd' \
  'posix_effect_journal_store::from_directory_fd' \
  'posix_transaction_effect_driver_source::from_lock_directory_fd'; do
  line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid POSIX transaction-run runtime assembly order: $token" >&2
    exit 1
  }
  previous=$line
done

for required_test in \
  'check_posix_transaction_run_runtime' \
  'posix_transaction_run_runtime::from_directory_fds' \
  'std::filesystem::rename(run_path, selected_run_path)' \
  'runtime->launch(' \
  'journal_nonce(211U)' \
  'result.record().nonce() == journal_nonce(211U)' \
  'runtime->drive(' \
  'transaction_run_launch_origin::admitted' \
  'transaction_run_drive_disposition::completed' \
  'executed_construction' \
  'archives.calls() == 0U' \
  'directory_entry_count(run_path) == 0U' \
  'directory_entry_count(effect_path) == 0U' \
  'directory_entry_count(lock_path) == 0U' \
  'transaction_run_journal_error_code::store_open_failed'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing POSIX transaction-run runtime test: $required_test" >&2
    exit 1
  }
done

for required_doc in \
  'Caller-configured POSIX transaction-run runtime' \
  'Release 0.25.0 native transaction-run runtime boundary' \
  'CALLER-CONFIGURED POSIX TRANSACTION-RUN RUNTIME'; do
  grep -F "$required_doc" "$srcdir/CHANGELOG.md" "$design" "$manual" \
      >/dev/null || {
    echo "missing POSIX transaction-run runtime documentation: $required_doc" >&2
    exit 1
  }
done

for forbidden in \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'usleep(' \
  'opendir(' \
  'readdir(' \
  'glob(' \
  'canonical_generation_store::initialize' \
  'open_existing(' \
  'list_' \
  'latest_'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden POSIX transaction-run runtime policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'posix_transaction_run_runtime|run_runtime' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'POSIX transaction-run runtime must not acquire a command frontend' >&2
  exit 1
fi
