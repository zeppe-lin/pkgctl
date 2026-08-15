#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_native.h"
source="$srcdir/src/run_native.cpp"
test_source="$srcdir/tests/unit/effect_test.cpp"
meson="$srcdir/meson.build"
readme="$srcdir/README.md"
design="$srcdir/DESIGN.md"
manual="$srcdir/man/pkgctl_orchestration.7.scd"

for file in "$header" "$source" "$test_source" "$meson" \
            "$readme" "$design" "$manual"; do
  [ -s "$file" ] || {
    echo "missing native effect-runtime source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_effect_archive_source' \
  'acquire_transaction_effect_archive(' \
  'class posix_transaction_effect_driver_source final' \
  'from_lock_directory_fd(' \
  'acquire_execution_drivers(' \
  'acquire_recovery_drivers(' \
  'F_DUPFD_CLOEXEC' \
  'pkgapply::posix::target_mutation_lease::acquire' \
  'transaction_effect_authority_unavailable' \
  'target_mutation_lease_error_code::lock_busy' \
  'pkgstate::apply_adapter::read_application_state' \
  'historical->admitted_state_projection()' \
  'native_transaction_effect_driver' \
  'native_transaction_effect_publication_driver' \
  'archive_image_mismatch' \
  'archive_receipt_mismatch' \
  'operation_reconciliation_requires_continuation_driver' \
  'operation_reconciliation_requires_state_observer' \
  'operation_reconciliation_requires_publication_driver'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "missing native effect-runtime contract: $required" >&2
    exit 1
  }
done

for required in \
  "'libpkgapply-posix'" \
  "version: ['>=3.0.0', '<4.0.0']" \
  "'libpkgstate-apply'" \
  "version: ['>=3.1.1', '<4.0.0']"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "missing native effect-runtime dependency floor: $required" >&2
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
      echo "invalid native effect-runtime order: $token" >&2
      exit 1
    }
    previous=$line
  done
}

continuation=$(sed -n \
  '/acquire_continuation(/,/^  }/p' "$source")
ordered_tokens "$continuation" \
  'acquire_transaction_effect_archive' \
  'acquire_target_lease' \
  'read_application_state' \
  'make_shared<continuation_runtime>'

lease=$(sed -n \
  '/^acquire_target_lease(/,/^}/p' "$source")
ordered_tokens "$lease" \
  'target_mutation_lease::acquire' \
  'target_mutation_lease_error_code::lock_busy' \
  'transaction_effect_authority_unavailable'

execution=$(sed -n \
  '/acquire_execution_drivers(/,/^}/p' "$source")
ordered_tokens "$execution" \
  'handoff.operation()' \
  'acquire_continuation' \
  'owned_native_continuation' \
  'owned_continuation_state_observer'

recovery=$(sed -n \
  '/acquire_recovery_drivers(/,/^}/p' "$source")
ordered_tokens "$recovery" \
  'operation_reconciliation_requires_continuation_driver' \
  'operation_reconciliation_requires_state_observer' \
  'operation_reconciliation_requires_publication_driver'

for required_test in \
  'check_native_effect_archive_source' \
  'archive_missing' \
  'archive_image_mismatch' \
  'archive_receipt_mismatch' \
  'transaction_effect_authority_unavailable' \
  'drivers.continuation.reset()' \
  'drivers.resulting_state.reset()' \
  'check_native_effect_recovery_source' \
  'continuation.publication == nullptr' \
  'terminal.continuation == nullptr' \
  'publication.resulting_state == nullptr' \
  'publication_archives.calls() == 0U'; do
  grep -F -- "$required_test" "$test_source" >/dev/null || {
    echo "missing native effect-runtime test: $required_test" >&2
    exit 1
  }
done

native_source_test=$(sed -n \
  '/void check_native_effect_driver_source()/,/^}/p' "$test_source")
printf '%s\n' "$native_source_test" | \
  grep -F 'catch (const pkgctl::transaction_effect_authority_unavailable&)' \
    >/dev/null || {
  echo 'native effect-runtime unit does not observe translated contention' >&2
  exit 1
}
if printf '%s\n' "$native_source_test" | \
    grep -F 'catch (const pkgapply::posix::target_mutation_lease_error&' \
      >/dev/null 2>&1; then
  echo 'native effect-runtime unit still expects POSIX lock contention to leak' >&2
  exit 1
fi

for required_doc in \
  'Caller-configured POSIX per-dispatch effect runtime' \
  'Release 0.24.0 native effect-runtime boundary' \
  'CALLER-CONFIGURED POSIX EFFECT RUNTIME'; do
  grep -F -- "$required_doc" "$srcdir/HISTORY.md" "$design" "$manual" \
      >/dev/null || {
    echo "missing native effect-runtime documentation: $required_doc" >&2
    exit 1
  }
done

if grep -R -F 'read_historical_application_state' \
    "$srcdir/src" "$srcdir/tests/integration" "$srcdir/tests/unit" \
    >/dev/null 2>&1; then
  echo 'native recovery reconstructs historical application state from current truth' \
    >&2
  exit 1
fi

for forbidden in \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'opendir(' \
  'readdir(' \
  'glob(' \
  'canonical_generation_store::initialize' \
  'transaction_run_journal_store' \
  'effect_journal_store'; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden native effect-runtime shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'posix_transaction_effect_driver_source|acquire_transaction_effect_archive' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'native effect runtime must not acquire a CLI command' >&2
  exit 1
fi
