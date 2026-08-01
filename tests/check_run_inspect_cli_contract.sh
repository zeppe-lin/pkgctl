#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options_header="$srcdir/cli/options.h"
options_source="$srcdir/cli/options.cpp"
main_source="$srcdir/cli/main.cpp"
store_source="$srcdir/src/run_store.cpp"
cli_test="$srcdir/tests/cli_test.sh"
fixture="$srcdir/tests/run_store_fixture.cpp"
manual="$srcdir/man/pkgctl.1.scd"

for file in "$options_header" "$options_source" "$main_source" \
            "$store_source" "$cli_test" "$fixture" "$manual"; do
  [ -s "$file" ] || {
    echo "missing exact run-inspection command source: $file" >&2
    exit 1
  }
done

for required in \
  'run_inspection_command' \
  'inspect-run' \
  '--run-store' \
  '--journal' \
  'session_identity::from_hex' \
  'posix_transaction_run_journal_store::open' \
  'inspect_transaction_run' \
  'render_report' \
  'store-open-failed' \
  'store-conflict' \
  'store-corrupt' \
  'store-contract-violation'; do
  grep -R -F -- "$required" "$options_header" "$options_source" \
    "$main_source" "$manual" >/dev/null || {
      echo "missing exact run-inspection command contract: $required" >&2
      exit 1
    }
done

parse_body=$(sed -n '/^run_inspection_command parse_run_inspection(/,/^}/p' \
  "$options_source")
previous=0
for token in \
  '"--run-store"' \
  '"--journal"' \
  'if (!store || store->empty())' \
  'if (!journal)' \
  'session_identity::from_hex'; do
  line=$(printf '%s\n' "$parse_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid exact run-inspection parse order: $token" >&2
    exit 1
  }
  previous=$line
done

execute_body=$(sed -n \
  '/auto store = pkgctl::posix_transaction_run_journal_store::open/,/std::cout << pkgctl::render_report(inspection);/p' \
  "$main_source")
previous=0
for token in \
  'posix_transaction_run_journal_store::open' \
  'inspect_transaction_run' \
  'render_report'; do
  line=$(printf '%s\n' "$execute_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid exact run-inspection execution order: $token" >&2
    exit 1
  }
  previous=$line
done

read_lock_body=$(sed -n '/^std::optional<fd_guard> lock_store_read_only(/,/^}/p' \
  "$store_source")
for required in 'O_RDONLY' 'LOCK_SH' 'errno == ENOENT'; do
  grep -F "$required" <<EOF_INNER >/dev/null || {
$read_lock_body
EOF_INNER
    echo "missing read-only run-store lock contract: $required" >&2
    exit 1
  }
done
for forbidden in 'O_CREAT' 'O_RDWR' 'LOCK_EX'; do
  if grep -F "$forbidden" <<EOF_INNER >/dev/null 2>&1
$read_lock_body
EOF_INNER
  then
    echo "read-only run-store lock acquired mutation authority: $forbidden" >&2
    exit 1
  fi
done

load_body=$(sed -n '/^posix_transaction_run_journal_store::load_latest(/,/^}/p' \
  "$store_source")
grep -F 'lock_store_read_only' <<EOF_INNER >/dev/null || {
$load_body
EOF_INNER
  echo 'POSIX run-store load does not use read-only lock authority' >&2
  exit 1
}
if grep -F 'lock_store(directory_fd_)' <<EOF_INNER >/dev/null 2>&1
$load_body
EOF_INNER
then
  echo 'POSIX run-store load still acquires writer lock' >&2
  exit 1
fi

for required_test in \
  'run_store_fixture' \
  'run.disposition=quiescent-incomplete' \
  'run_before' \
  'run_after' \
  '[ ! -e "$run_store/.pkgctl-run.lock" ]' \
  'invalid --journal' \
  'no committed store head' \
  'store-open-failed' \
  'store-corrupt'; do
  grep -F "$required_test" "$cli_test" >/dev/null || {
    echo "missing exact run-inspection command test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'append(' \
  'reserve_next(' \
  'advance_transaction_run_once(' \
  'drive_transaction_run(' \
  'launch_transaction_run(' \
  'reconcile_transaction_run' \
  'effect_journal_store' \
  'opendir(' \
  'readdir(' \
  'std::thread' \
  'std::async'; do
  if printf '%s\n%s\n' "$parse_body" "$execute_body" | \
      grep -F "$forbidden" >/dev/null 2>&1; then
    echo "forbidden exact run-inspection command policy: $forbidden" >&2
    exit 1
  fi
done
