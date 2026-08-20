#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
header=$root/include/pkgctl/run_operation.h
source=$root/src/run_operation.cpp
paths=$root/src/run_lifecycle_session.h
test_source=$root/tests/unit/effect_test.cpp
manual=$root/man/pkgctl_orchestration.7.scd
codec_header=$root/include/pkgctl/operation_codec.h
codec_source=$root/src/operation_session_codec.cpp

for file in "$header" "$source" "$paths" "$codec_header" "$codec_source" "$test_source" "$manual"; do
  [ -s "$file" ] || {
    echo "missing native operation authority file: $file" >&2
    exit 1
  }
done

for required in \
  'class native_transaction_operation_configuration final' \
  'class transaction_operation_specification_source' \
  'class transaction_operation_session_store' \
  'class transaction_effect_restart_body_source' \
  'class native_transaction_operation_authority_source final' \
  'public transaction_operation_execution_authority_source' \
  'public transaction_operation_recovery_authority_source' \
  'class explicit_transaction_effect_archive_source final' \
  'const transaction_progress& progress' \
  'specifications_.operation' \
  'encode_operation_session' \
  'decode_operation_session' \
  'sessions_->load' \
  'specification.lifecycle()' \
  'if (order.empty())' \
  'native_operation_preparation_driver' \
  'effect_restart_checkpoint::make' \
  'effects_.load_latest' \
  'native_transaction_operation_authority_source::rehydrate' \
  'evidence.stage() != effect_attempt_stage::terminal' \
  'record.journal().hex()' \
  'dispatch.identity().hex()' \
  'incoming.image().receipt().archive_digest()'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "missing native operation authority contract: $required" >&2
    exit 1
  }
done

for required_path in \
  'pkgctl/native-lifecycle-session-root/1' \
  'record.journal().hex()' \
  'dispatch.identity().hex()' \
  'lifecycle_node.hex()'; do
  grep -F -- "$required_path" "$paths" >/dev/null || {
    echo "missing lifecycle-session path authority contract: $required_path" >&2
    exit 1
  }
done

for required_test in \
  'check_native_operation_authority_source' \
  'check_native_incoming_operation_authority_source' \
  'application_target_without_lifecycle_executor' \
  '!no_lifecycle_target.lifecycle_executor()' \
  'explicit_lifecycle_order' \
  'incomplete_lifecycle_specifications' \
  'fresh.session.identity() == repeated.session.identity()' \
  'recording_operation_session_store' \
  'sessions.retain_calls() == 2U' \
  'sessions.load_calls() == 1U' \
  'drift_specifications.calls() == 0U' \
  'corrupt_sessions.corrupt' \
  'corrupt_specifications.calls() == 0U' \
  'lifecycle_session_parent' \
  'admitted_lifecycle.paths().session_root.parent_path()' \
  'transaction_node.hex()' \
  '!std::filesystem::exists(authority_root)' \
  'effect_attempt_missing' \
  'planning_refused' \
  'check_explicit_transaction_effect_archive_source' \
  'expected_archive_digest' \
  'duplicate_refused'; do
  grep -F -- "$required_test" "$test_source" >/dev/null || {
    echo "missing native operation authority test: $required_test" >&2
    exit 1
  }
done

# The orchestrator may report that planning refused, but it must not invent or
# expose a second textual/numeric vocabulary for the planner-owned refusal code.
grep -F -- '"native operation planning refused"' "$source" >/dev/null || {
  echo 'native operation refusal lacks generic controller diagnostic' >&2
  exit 1
}
for forbidden in \
  'prepared.refusal()->code()' \
  'planning_refusal_name' \
  'static_cast<unsigned>(prepared.refusal'; do
  if grep -F -- "$forbidden" "$source" >/dev/null 2>&1; then
    echo "native operation authority renders foreign refusal vocabulary: $forbidden" >&2
    exit 1
  fi
done

# Session/recovery authority may project and validate, but may not observe or
# mutate the target, execute effects, append journals, discover archives, or
# acquire runtime backends. Archive opening is confined to the explicit map.
for forbidden in \
  'derive_lifecycle_order' \
  'create_directories(' \
  'remove_all(' \
  'directory_iterator' \
  'recursive_directory_iterator' \
  'canonical(' \
  'weakly_canonical(' \
  'exists(' \
  'status(' \
  'symlink_status(' \
  'effect_store.append' \
  'effects_.append' \
  'execute_effectful_operation' \
  'resume_effectful_operation' \
  'target_mutation_lease::acquire' \
  'read_application_state' \
  'canonical_store' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'opendir(' \
  'readdir(' \
  'glob('; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden native operation authority shortcut: $forbidden" >&2
    exit 1
  fi
done

# Retained operation-session decoding is pure historical evidence recovery.
for forbidden in \
  'std::filesystem::exists' \
  'directory_iterator' \
  'canonical(' \
  'weakly_canonical(' \
  'open(' \
  'openat(' \
  'stat(' \
  'lstat(' \
  'readlink(' \
  'pkgfetch::' \
  'canonical_store'; do
  if grep -F -- "$forbidden" "$codec_source" >/dev/null 2>&1; then
    echo "forbidden operation-session codec observation: $forbidden" >&2
    exit 1
  fi
done

# Frontend code must not bypass the reviewed runtime by directly wiring this
# operation authority or ad-hoc effect-implying commands into cli/main.cpp.
! grep -R -q 'run_operation' "$root/cli"
! grep -E -q '(^|[[:space:]])(run|apply|install|upgrade|remove)([[:space:]]|$)' \
  "$root/cli/main.cpp"

grep -F 'Release 0.33.0 native operation authority boundary' \
  "$root/DESIGN.md" >/dev/null
grep -F 'NATIVE OPERATION AUTHORITY' "$manual" >/dev/null
