#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/run_runtime.h"
source="$srcdir/src/run_runtime.cpp"
test_source="$srcdir/tests/unit/construction_test.cpp"
meson="$srcdir/src/meson.build"
readme="$srcdir/README.md"
design="$srcdir/DESIGN.md"
testing="$srcdir/TESTING.md"
manual="$srcdir/man/pkgctl_orchestration.7.scd"

for file in "$header" "$source" "$test_source" "$meson" \
            "$readme" "$design" "$testing" "$manual"; do
  [ -s "$file" ] || {
    echo "missing transaction-run runtime source: $file" >&2
    exit 1
  }
done

for required in \
  'struct transaction_run_runtime_authorities final' \
  'struct transaction_run_runtime_backends final' \
  'class posix_transaction_run_runtime final' \
  'class transaction_run_runtime_engine final' \
  'explicit_run_nonce_source' \
  'canonical_transaction_dispatch_nonce_source dispatch_nonces_' \
  'posix_transaction_run_journal_store::from_directory_fd' \
  'posix_transaction_run_evidence_store::from_directory_fd' \
  'posix_effect_journal_store::from_directory_fd' \
  'posix_transaction_effect_driver_source::from_lock_directory_fd' \
  'launch_transaction_run(' \
  'drive_transaction_run(' \
  'struct native_transaction_run_runtime_paths final' \
  'std::optional<std::filesystem::path> target_lock_store;' \
  'enum class native_transaction_run_runtime_error_code' \
  'native_transaction_requires_target_operation_authority(' \
  'struct native_transaction_execution_scope final' \
  'native_transaction_execution_scopes(' \
  'native_transaction_resume_execution_scopes(' \
  'evidence_store_directory_fd' \
  'effect_store_directory_fd' \
  'class native_transaction_run_runtime_configuration final' \
  'const native_transaction_operation_configuration*' \
  'struct native_transaction_run_runtime_authorities final' \
  'transaction_operation_specification_source* operation_specifications;' \
  'transaction_effect_restart_body_source* effect_restart_bodies;' \
  'struct native_transaction_run_runtime_backends final' \
  'pkgexec::execution_backend* construction;' \
  'pkgexec::execution_backend* check;' \
  'pkgobject::store* package_objects;' \
  'pkgapply::application_backend* application;' \
  'pkgexec::execution_backend* lifecycle;' \
  'pkgstate::canonical_store* state;' \
  'current execution backend is unavailable for durable recovery' \
  'class native_posix_transaction_run_runtime final' \
  'native_posix_transaction_run_runtime::open(' \
  'native_posix_transaction_run_runtime::from_directory_fds(' \
  'validate_runtime_paths(paths)' \
  'O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW' \
  'validate_distinct_runtime_directories(' \
  'validate_native_configuration(' \
  'validate_native_runtime_composition(' \
  'target-operation transaction requires explicit operation authority' \
  'construction/check-only transaction refuses target-operation ' \
  'construction/check-only transaction refuses target-operation ' \
  'one execution-root path names contradictory root-view identities' \
  'native construction/check storage overlaps lifecycle execution' \
  'native_transaction_resource_session_source sessions_' \
  'std::unique_ptr<native_transaction_operation_authority_source> operations_' \
  'std::unique_ptr<explicit_transaction_effect_archive_source> owned_archives_' \
  'transaction_effect_archive_source* archives_' \
  'native_transaction_progress_rehydration_context_source progress_context_' \
  'detail::native_construction_recovery_context(' \
  'detail::native_check_recovery_context(' \
  'progress_context_(operations_.get())' \
  'operations_->rehydrate(' \
  'stored_transaction_progress_rehydration_source progress_' \
  'transaction_progress::begin(configuration_.transaction())' \
  'transaction_run_nonce run_nonce'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "missing transaction-run runtime contract: $required" >&2
    exit 1
  }
done

native_body=$(sed -n \
  '/class native_posix_transaction_run_runtime::implementation final/,/^};/p' \
  "$source")
previous=0
for token in \
  'runs_(posix_transaction_run_journal_store::from_directory_fd' \
  'evidence_(posix_transaction_run_evidence_store::from_directory_fd' \
  'effects_(posix_effect_journal_store::from_directory_fd' \
  'sessions_(configuration_.sessions(), backends.package_objects)' \
  'operations_(' \
  'owned_archives_(' \
  'archives_(' \
  'progress_context_(' \
  'progress_(' \
  'unavailable_execution_backend_()' \
  'engine_('; do
  line=$(printf '%s\n' "$native_body" | awk \
    -v token="$token" -v previous="$previous" \
    'NR > previous && index($0, token) { print NR; exit }')
  [ -n "$line" ] && [ "$line" -gt "$previous" ] || {
    echo "invalid native runtime assembly order: $token" >&2
    exit 1
  }
  previous=$line
done

for required_test in \
  'check_posix_transaction_run_runtime' \
  'posix_transaction_run_runtime::from_directory_fds' \
  'journal_nonce(211U)' \
  'transaction_run_advance_disposition::executed_construction' \
  'transaction_run_advance_disposition::quiescent' \
  'check_native_posix_transaction_run_runtime' \
  'native_transaction_run_runtime_configuration::make' \
  'lifecycle-execution-root' \
  'surplus_operation_authority_refused' \
  'contradictory_root_refused' \
  'mutable_overlap_refused' \
  'descriptor_alias_refused' \
  'surplus_target_lock_refused' \
  'construction_lock_path' \
  'native_posix_transaction_run_runtime::open' \
  'native_transaction_run_runtime_error_code::directory_overlap' \
  'journal_nonce(212U)' \
  'package_objects = pkgobject::store::open_or_create' \
  'operation_specifications.calls() == 0U' \
  'effect_restart_bodies.calls() == 0U' \
  'forbidden_recovery_execution_backend' \
  'recovery_backend.capability_calls() == 0U' \
  'recovery_backend.execution_calls() == 0U' \
  'artifacts == 1U' \
  'missing_package_object_authority_refused' \
  'terminal_without_package_objects' \
  '{nullptr, nullptr, nullptr, archive_backend}'; do
  grep -F -- "$required_test" "$test_source" >/dev/null || {
    echo "missing transaction-run runtime test: $required_test" >&2
    exit 1
  }
done

for required_doc in \
  'Release 0.36.0 construction-only runtime authority' \
  'Release 0.34.0 native target/runtime composition boundary' \
  'NATIVE TARGET AND RUNTIME COMPOSITION' \
  'explicit durable run-intent nonce' \
  'live per-dispatch operation specifications'; do
  grep -F -- "$required_doc" "$srcdir/HISTORY.md" "$readme" "$design" \
      "$testing" "$manual" >/dev/null || {
    echo "missing native runtime documentation: $required_doc" >&2
    exit 1
  }
done

for forbidden_authority in \
  'transaction_run_nonce_source& run_nonces' \
  'transaction_dispatch_nonce_source& dispatch_nonces'; do
  if grep -F -- "$forbidden_authority" "$header" >/dev/null 2>&1; then
    echo "runtime must not borrow nonce service: $forbidden_authority" >&2
    exit 1
  fi
done


progress_recovery_body=$(sed -n \
  '/class native_transaction_progress_rehydration_context_source final/,/^};/p' \
  "$source")
if printf '%s\n' "$progress_recovery_body" | grep -F 'transaction_dispatch_session_source' \
    >/dev/null 2>&1; then
  echo 'progress recovery must not borrow the fresh construction/check session source' >&2
  exit 1
fi
if printf '%s\n' "$progress_recovery_body" | grep -F 'sessions_' \
    >/dev/null 2>&1; then
  echo 'progress recovery retains a fresh session-source dependency' >&2
  exit 1
fi


for forbidden_recovery_profile in \
  'native_recovery_profile(' \
  'construction_recovery_backend' \
  'check_recovery_backend' \
  'backends.construction.capabilities()' \
  'backends.check.capabilities()'; do
  if grep -F -- "$forbidden_recovery_profile" "$header" "$source" >/dev/null 2>&1; then
    echo "runtime must not reconstruct historical backend profile: $forbidden_recovery_profile" >&2
    exit 1
  fi
done

if grep -F 'unavailable_operation_authority_source' "$source" >/dev/null 2>&1; then
  echo 'construction-only runtime fabricates an unavailable operation authority' >&2
  exit 1
fi

for forbidden in \
  'create_director' \
  'remove_all(' \
  'directory_iterator' \
  'recursive_directory_iterator' \
  'canonical(' \
  'weakly_canonical(' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'usleep(' \
  'opendir(' \
  'readdir(' \
  'glob(' \
  'mkdtemp(' \
  'getenv(' \
  'canonical_generation_store::initialize' \
  'list_' \
  'latest_'; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden native runtime policy: $forbidden" >&2
    exit 1
  fi
done

grep -F 'native_posix_transaction_run_runtime::from_directory_fds(' \
  "$srcdir/cli/run_command.cpp" >/dev/null || {
  echo 'bounded run command does not enter through the reviewed native runtime' >&2
  exit 1
}
if grep -R -n -E \
    'launch_transaction_run[[:space:]]*\(|drive_transaction_run[[:space:]]*\(' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'command frontend bypasses the native runtime root' >&2
  exit 1
fi
if grep -F 'posix_transaction_run_evidence_store' "$srcdir/cli/run_command.cpp" \
    >/dev/null 2>&1; then
  echo 'command frontend opens transaction-run evidence storage directly' >&2
  exit 1
fi
