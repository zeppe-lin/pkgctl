#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_recovery.h"
source="$srcdir/src/run_recovery.cpp"
construction="$srcdir/tests/unit/construction_test.cpp"
check="$srcdir/tests/unit/check_test.cpp"
meson="$srcdir/src/meson.build"
tests_meson="$srcdir/tests/meson.build"

for file in "$header" "$source" "$construction" "$check" \
            "$meson" "$tests_meson"; do
  [ -s "$file" ] || {
    echo "missing evidence recovery source: $file" >&2
    exit 1
  }
done

for required in \
  'struct construction_dispatch_recovery_context final' \
  'struct check_dispatch_recovery_context final' \
  'class transaction_dispatch_recovery_context_source' \
  'class transaction_operation_recovery_authority_source' \
  'class native_transaction_dispatch_recovery_context_source final' \
  'pkgexec::backend_capability_profile construction_backend_' \
  'pkgexec::backend_capability_profile check_backend_' \
  'transaction_operation_recovery_authority_source& operations_' \
  'class stored_transaction_dispatch_recovery_authority_source final' \
  'transaction_run_evidence_store& evidence_' \
  'transaction_dispatch_recovery_context_source& context_' \
  'load_construction(' \
  'load_check(' \
  'decode_build_execution_result(' \
  'decode_check_execution_result(' \
  'evidence_missing' \
  'recovery_context_mismatch' \
  'recovery_decode_failed' \
  'detail_run_recovery_access' \
  'decode_construction_session(' \
  'decode_check_session(' \
  'pkgfetch::decode_source_materialization(' \
  'pkgbuild_exec::seal_execution_request(' \
  'pkgcheck_exec::seal_execution_request(' \
  'detail::native_construction_recovery_context(' \
  'detail::native_check_recovery_context(' \
  'transaction_check_request::make(' \
  'checkpoint.run().progress()'; do
  grep -F -- "$required" "$header" "$source" \
      "$srcdir/include/pkgctl/run_evidence.h" >/dev/null || {
    echo "missing evidence recovery contract: $required" >&2
    exit 1
  }
done

for required_test in \
  'check_stored_construction_recovery' \
  'construction_dispatch_evidence_record::admit' \
  'stored_transaction_dispatch_recovery_authority_source recovery_source' \
  'transaction_run_evidence_error_code::evidence_missing' \
  'recovery_context_mismatch' \
  'check_posix_transaction_run_runtime_recovery' \
  'CHECK(execution.calls() == 0U)'; do
  grep -F -- "$required_test" "$construction" >/dev/null || {
    echo "missing construction evidence recovery test: $required_test" >&2
    exit 1
  }
done

for required_test in \
  'check_stored_check_recovery' \
  'check_dispatch_evidence_record::admit' \
  'stored_transaction_dispatch_recovery_authority_source recovery_source' \
  'transaction_run_evidence_error_code::evidence_missing' \
  'recovery_context_mismatch' \
  'native_transaction_dispatch_recovery_context_source' \
  'check_durable_session_codec' \
  'encode_check_session(native_recovery.check()->session())'; do
  grep -F -- "$required_test" "$check" >/dev/null || {
    echo "missing check evidence recovery test: $required_test" >&2
    exit 1
  }
done

for forbidden in \
  'sessions.construction(' \
  'sessions.check(' \
  'pkgfetch::materialize(' \
  'pkgexec::execution_backend& construction_backend_' \
  'pkgexec::execution_backend& check_backend_' \
  'selected_backend.capabilities()' \
  'from_sha256(evidence' \
  'materialization_identity::from_sha256' \
  'build_request_identity::from_sha256' \
  'execution_request_identity::from_sha256' \
  'backend_capability_profile_identity::from_sha256' \
  'construction_result(' \
  'transaction_check_result('; do
  case "$forbidden" in
    'construction_result('| 'transaction_check_result(')
      # Private reconstruction is restricted to the reviewed access struct.
      count=$(grep -F -- "$forbidden" "$source" | wc -l | tr -d ' ')
      [ "$count" -le 1 ] || {
        echo "recovery source duplicates controller result construction: $forbidden" >&2
        exit 1
      }
      ;;
    *)
      if grep -F -- "$forbidden" "$source" >/dev/null 2>&1; then
        echo "recovery source fabricates missing authority: $forbidden" >&2
        exit 1
      fi
      ;;
  esac
done

if ! grep -F "'run_recovery.cpp'" "$meson" >/dev/null; then
  echo 'evidence recovery implementation is not built' >&2
  exit 1
fi
if ! grep -F "'run_recovery.h'" "$tests_meson" >/dev/null; then
  echo 'evidence recovery public header is not qualified' >&2
  exit 1
fi
