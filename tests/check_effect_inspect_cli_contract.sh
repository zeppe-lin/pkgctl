#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options_header="$srcdir/cli/options.h"
options_source="$srcdir/cli/options.cpp"
main_source="$srcdir/cli/main.cpp"
store_source="$srcdir/src/effect_store.cpp"
cli_test="$srcdir/tests/cli_test.sh"
fixture="$srcdir/tests/effect_store_fixture.cpp"
manual="$srcdir/man/pkgctl.1.scd"

for file in "$options_header" "$options_source" "$main_source" \
            "$store_source" "$cli_test" "$fixture" "$manual"; do
  [ -s "$file" ] || {
    echo "missing exact effect-inspection command source: $file" >&2
    exit 1
  }
done

for required in \
  'effect_inspection_command' \
  'inspect-effect' \
  '--effect-store' \
  '--attempt' \
  'session_identity::from_hex' \
  'posix_effect_journal_store::open' \
  'inspect_effect_attempt' \
  'render_report' \
  'pkgctl: effect journal:' \
  'store-open-failed' \
  'store-conflict' \
  'store-corrupt' \
  'store-contract-violation'; do
  grep -R -F -- "$required" "$options_header" "$options_source" \
    "$main_source" "$manual" >/dev/null || {
      echo "missing exact effect-inspection command contract: $required" >&2
      exit 1
    }
done

parse_body=$(sed -n '/^effect_inspection_command parse_effect_inspection(/,/^}/p' \
  "$options_source")
previous=0
for token in \
  '"--effect-store"' \
  '"--attempt"' \
  'if (!store || store->empty())' \
  'if (!attempt)' \
  'session_identity::from_hex'; do
  line=$(printf '%s\n' "$parse_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid exact effect-inspection parse order: $token" >&2
    exit 1
  }
  previous=$line
done

execute_body=$(sed -n \
  '/auto store = pkgctl::posix_effect_journal_store::open/,/std::cout << pkgctl::render_report(inspection);/p' \
  "$main_source")
previous=0
for token in \
  'posix_effect_journal_store::open' \
  'inspect_effect_attempt' \
  'render_report'; do
  line=$(printf '%s\n' "$execute_body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid exact effect-inspection execution order: $token" >&2
    exit 1
  }
  previous=$line
done

read_lock_body=$(sed -n '/^std::optional<fd_guard> lock_store_read_only(/,/^}/p' \
  "$store_source")
for required in 'O_RDONLY' 'LOCK_SH' 'errno == ENOENT'; do
  printf '%s\n' "$read_lock_body" | grep -F "$required" >/dev/null || {
    echo "missing read-only effect-store lock contract: $required" >&2
    exit 1
  }
done
for forbidden in 'O_CREAT' 'O_RDWR' 'LOCK_EX'; do
  if printf '%s\n' "$read_lock_body" | grep -F "$forbidden" >/dev/null 2>&1; then
    echo "read-only effect-store lock acquired mutation authority: $forbidden" >&2
    exit 1
  fi
done

load_body=$(sed -n '/^posix_effect_journal_store::load_latest(/,/^}/p' \
  "$store_source")
printf '%s\n' "$load_body" | grep -F 'lock_store_read_only' >/dev/null || {
  echo 'POSIX effect-store load does not use read-only lock authority' >&2
  exit 1
}
if printf '%s\n' "$load_body" | grep -F 'lock_store(directory_fd_)' \
    >/dev/null 2>&1; then
  echo 'POSIX effect-store load still acquires writer lock' >&2
  exit 1
fi

for required_test in \
  'effect_store_fixture' \
  'effect.disposition=start-application' \
  'effect_before' \
  'effect_after' \
  'repeat_effect_inspection' \
  'absent effect evidence was synthesized' \
  '[ ! -e "$effect_store/.pkgctl-effect.lock" ]' \
  'invalid --attempt' \
  'has no committed store head' \
  'pkgctl: effect journal: store-open-failed:' \
  'pkgctl: effect journal: store-corrupt:'; do
  grep -F "$required_test" "$cli_test" >/dev/null || {
    echo "missing exact effect-inspection command test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'append(' \
  'transaction_run' \
  'run_journal' \
  'assess_effect_restart' \
  'resume_effectful_operation' \
  'reconcile' \
  'driver' \
  'opendir(' \
  'readdir(' \
  'std::thread' \
  'std::async'; do
  if printf '%s\n%s\n' "$parse_body" "$execute_body" | \
      grep -F "$forbidden" >/dev/null 2>&1; then
    echo "forbidden exact effect-inspection command policy: $forbidden" >&2
    exit 1
  fi
done
