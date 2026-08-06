#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
model="$srcdir/include/pkgctl/run_evidence.h"
codec="$srcdir/include/pkgctl/run_evidence_codec.h"
store="$srcdir/include/pkgctl/run_evidence_store.h"
model_source="$srcdir/src/run_evidence.cpp"
codec_source="$srcdir/src/run_evidence_codec.cpp"
store_source="$srcdir/src/run_evidence_store.cpp"
construction_test="$srcdir/tests/construction_test.cpp"
check_test="$srcdir/tests/check_test.cpp"

for file in "$model" "$codec" "$store" "$model_source" "$codec_source" \
            "$store_source" "$construction_test" "$check_test"; do
  [ -s "$file" ] || {
    echo "missing transaction-run evidence source: $file" >&2
    exit 1
  }
done

for required in \
  'transaction_run_evidence_schema_version = 1' \
  'construction_dispatch_evidence_record' \
  'check_dispatch_evidence_record' \
  'controller_request() const noexcept' \
  'execution_request() const noexcept' \
  'backend() const noexcept' \
  'encoding() const noexcept' \
  'encode_construction_dispatch_evidence' \
  'decode_construction_dispatch_evidence' \
  'encode_check_dispatch_evidence' \
  'decode_check_dispatch_evidence' \
  'transaction_run_evidence_store' \
  'posix_transaction_run_evidence_store' \
  'publish(const construction_dispatch_evidence_record& record)' \
  'publish(const check_dispatch_evidence_record& record)' \
  'load_construction(' \
  'load_check(' \
  'O_NOFOLLOW' \
  'F_DUPFD_CLOEXEC' \
  'flock(' \
  'LOCK_SH' \
  'lock_store_read_only' \
  'fchmod(' \
  'fsync(' \
  'linkat(' \
  'unlinkat(' \
  'store_conflict' \
  'store_corrupt'; do
  grep -F "$required" "$model" "$codec" "$store" "$model_source" \
      "$codec_source" "$store_source" >/dev/null || {
    echo "missing transaction-run evidence contract: $required" >&2
    exit 1
  }
done

publish_body=$(sed -n \
  '/posix_transaction_run_evidence_store::publish(/,/^}/p' \
  "$store_source")
for token in \
  'persist_immutable(' \
  'object_name(' \
  'encode_index(' \
  'persist_immutable('; do
  line=$(printf '%s\n' "$publish_body" | grep -n -F "$token" | head -n 1 | cut -d: -f1)
  [ -n "$line" ] || {
    echo "missing transaction-run evidence publication step: $token" >&2
    exit 1
  }
done

for forbidden in \
  'decode_build_execution_result(' \
  'decode_check_execution_result(' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'opendir(' \
  'readdir(' \
  'glob('; do
  if grep -F "$forbidden" "$model" "$codec" "$store" "$model_source" \
      "$codec_source" "$store_source" >/dev/null 2>&1; then
    echo "forbidden transaction-run evidence authority: $forbidden" >&2
    exit 1
  fi
done

for required_test in \
  'check_transaction_run_evidence_storage' \
  'decode_construction_dispatch_evidence' \
  'evidence-original' \
  'evidence-selected' \
  '.pkgctl-run-evidence.lock' \
  'owner_write' \
  'evidence-absent-object' \
  'evidence-corrupt-index' \
  'store_conflict' \
  'corrupt_encoding' \
  'encode_check_dispatch_evidence' \
  'posix_transaction_run_evidence_store::open' \
  'injected construction-evidence failure' \
  'injected check-evidence failure'; do
  grep -F "$required_test" "$construction_test" "$check_test" \
      "$srcdir/tests/run_execute_support.h" >/dev/null || {
    echo "missing transaction-run evidence test: $required_test" >&2
    exit 1
  }
done

if grep -R -n -E 'run_evidence|transaction_run_evidence' "$srcdir/cli" \
    >/dev/null 2>&1; then
  echo 'transaction-run evidence storage must not acquire a command frontend' >&2
  exit 1
fi
