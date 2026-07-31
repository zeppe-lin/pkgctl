#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/effect_inspect.h"
journal_header="$srcdir/include/pkgctl/effect_journal.h"
source="$srcdir/src/effect_inspect.cpp"
store_source="$srcdir/src/effect_store.cpp"
report_header="$srcdir/include/pkgctl/report.h"
report_source="$srcdir/src/report.cpp"
test_source="$srcdir/tests/effect_inspect_test.cpp"
store_test="$srcdir/tests/effect_journal_test.cpp"

for file in "$header" "$journal_header" "$source" "$store_source" \
            "$report_header" "$report_source" "$test_source" "$store_test"; do
  [ -s "$file" ] || {
    echo "missing durable effect-attempt inspection source: $file" >&2
    exit 1
  }
done

for required in \
  'effect_attempt_inspection' \
  'inspect_effect_attempt' \
  'const effect_attempt_record& record() const noexcept' \
  'const effect_restart_assessment& assessment() const noexcept' \
  'bool terminal() const noexcept' \
  'bool automatically_continuable() const noexcept' \
  'bool external_resolution_required() const noexcept' \
  'store_contract_violation = 12' \
  'store.load_latest(attempt)' \
  'record->attempt() != attempt' \
  'assess_effect_restart(*record)' \
  'effect store returned foreign inspection authority' \
  'render_report(const effect_attempt_inspection& inspection)' \
  'session.kind=effect-attempt' \
  'effect.attempt=' \
  'effect.record=' \
  'effect.session=' \
  'effect.nonce=' \
  'effect.sequence=' \
  'effect.previous=' \
  'effect.stage=' \
  'effect.disposition=' \
  'effect.terminal=' \
  'effect.automatically-continuable=' \
  'effect.external-resolution-required=' \
  'effect.before-total=' \
  'effect.before-completed=' \
  'effect.after-total=' \
  'effect.after-completed=' \
  'effect.active-index=' \
  'effect.application-receipt=' \
  'effect.application-outcome=' \
  'effect.application-journal=' \
  'effect.application-completed-evidence=' \
  'effect.transaction-evidence=' \
  'effect.publication-request=' \
  'effect.publication-receipt=' \
  'effect.publication-outcome=' \
  'effect.publication-resulting-snapshot=' \
  'effect.terminal-outcome=' \
  'effect.reconciled-state='; do
  grep -F "$required" "$header" "$journal_header" "$source" \
    "$report_header" "$report_source" >/dev/null || {
    echo "missing durable effect-attempt inspection contract: $required" >&2
    exit 1
  }
done

body=$(sed -n '/^effect_attempt_inspection inspect_effect_attempt(/,/^} \/\/ namespace pkgctl$/p' \
  "$source")
previous=0
for token in \
  'store.load_latest' \
  'record->attempt() != attempt' \
  'assess_effect_restart' \
  'effect_attempt_inspection('; do
  line=$(printf '%s\n' "$body" | awk -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid durable effect-attempt inspection order: $token" >&2
    exit 1
  }
  previous=$line
done

read_lock=$(sed -n '/^std::optional<fd_guard> lock_store_read_only(/,/^}$/p' \
  "$store_source")
for required in \
  '.pkgctl-effect.lock' \
  'O_RDONLY | O_CLOEXEC | O_NOFOLLOW' \
  'errno == ENOENT' \
  'LOCK_SH'; do
  printf '%s\n' "$read_lock" | grep -F "$required" >/dev/null || {
    echo "missing read-only effect-store lock contract: $required" >&2
    exit 1
  }
done
for forbidden in 'O_RDWR' 'O_WRONLY' 'O_CREAT' 'LOCK_EX'; do
  if printf '%s\n' "$read_lock" | grep -F "$forbidden" >/dev/null 2>&1; then
    echo "read-only effect-store lock has writer authority: $forbidden" >&2
    exit 1
  fi
done

load_body=$(sed -n '/^posix_effect_journal_store::load_latest(/,/^}$/p' \
  "$store_source")
[ "$(printf '%s\n' "$load_body" | grep -c 'lock_store_read_only')" -ge 3 ] || {
  echo 'effect-store read path lacks lock establishment rechecks' >&2
  exit 1
}
printf '%s\n' "$load_body" | grep -F 'catch (...)' >/dev/null || {
  echo 'effect-store read path does not recheck after failed unlocked observation' >&2
  exit 1
}

writer_lock=$(sed -n '/^fd_guard lock_store(/,/^}$/p' "$store_source")
for required in 'O_RDWR | O_CREAT' 'LOCK_EX'; do
  printf '%s\n' "$writer_lock" | grep -F "$required" >/dev/null || {
    echo "missing effect-store writer lock contract: $required" >&2
    exit 1
  }
done

for required_test in \
  'check_read_only_effect_inspection' \
  'inspection_store' \
  'effect_restart_disposition::start_application' \
  'effect_restart_disposition::external_resolution_required' \
  'effect_restart_disposition::terminal' \
  'effect.disposition=start-application' \
  'effect.disposition=external-resolution-required' \
  'effect.terminal-outcome=outer-lease-lost' \
  'store_contract_violation' \
  'append_calls() == 0U' \
  '!std::filesystem::exists(lock_path)' \
  'std::filesystem::exists(lock_path)'; do
  grep -F "$required_test" "$test_source" "$store_test" >/dev/null || {
    echo "missing durable effect-attempt inspection test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'append(' \
  'resume_effectful_operation(' \
  'effect_restart_checkpoint' \
  'transaction_run_journal_store' \
  'rehydrate_transaction_run(' \
  'transaction_effect_driver' \
  'std::thread' \
  'std::async' \
  'while (' \
  'for (' \
  'opendir(' \
  'readdir(' \
  'posix_effect_journal_store'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden durable effect-attempt inspection policy: $forbidden" >&2
    exit 1
  fi
done

if grep -R -F 'inspect-effect' "$srcdir/cli" >/dev/null 2>&1; then
  echo 'effect-attempt inspection CLI appeared before its release boundary' >&2
  exit 1
fi
