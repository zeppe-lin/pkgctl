#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_nonce.h"
source="$srcdir/src/run_nonce.cpp"
test_source="$srcdir/tests/construction_test.cpp"
meson="$srcdir/src/meson.build"
design="$srcdir/DESIGN.md"
manual="$srcdir/man/pkgctl_orchestration.7.scd"

for file in "$header" "$source" "$test_source" "$meson" "$design" "$manual"; do
  [ -s "$file" ] || {
    echo "missing canonical transaction nonce source: $file" >&2
    exit 1
  }
done

for required in \
  'canonical_transaction_dispatch_nonce(' \
  'class canonical_transaction_dispatch_nonce_source final' \
  'transaction_dispatch_nonce_source' \
  'record.reopen(run.progress())' \
  'reopened.identity() != run.identity()' \
  'pkgctl/transaction-dispatch-nonce/1' \
  'record.journal().hex()' \
  'record.identity().hex()' \
  'run.identity().hex()' \
  'transaction_dispatch_nonce::from_hex(identity.hex())'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing canonical transaction nonce contract: $required" >&2
    exit 1
  }
done

for required_test in \
  'check_canonical_transaction_dispatch_nonce_authority' \
  'canonical_transaction_dispatch_nonce_source source' \
  'CHECK(first == repeated)' \
  'canonical_transaction_dispatch_nonce(record, run)' \
  'CHECK(next != first)' \
  'transaction_run_journal_error_code::invalid_transition'; do
  grep -F "$required_test" "$test_source" >/dev/null || {
    echo "missing canonical transaction nonce test: $required_test" >&2
    exit 1
  }
done

for required_doc in \
  'Release 0.26.0 explicit run intent and canonical dispatch nonce boundary' \
  'EXPLICIT RUN INTENT AND CANONICAL DISPATCH NONCES'; do
  grep -F "$required_doc" "$design" "$manual" >/dev/null || {
    echo "missing canonical transaction nonce documentation: $required_doc" >&2
    exit 1
  }
done

for forbidden in \
  'getrandom' \
  '/dev/urandom' \
  'random_device' \
  'O_CREAT' \
  '::open(' \
  'openat(' \
  'fopen(' \
  'transaction_run_journal_store' \
  'effect_journal_store' \
  'std::thread' \
  'sleep('; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden canonical transaction nonce authority: $forbidden" >&2
    exit 1
  fi
done
