#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
header=$root/include/pkgctl/run_progress.h
source=$root/src/run_progress.cpp
fail(){ echo "run-progress-contract: $*" >&2; exit 1; }

for text in \
  'class transaction_progress_rehydration_context_source' \
  'class stored_transaction_progress_rehydration_source final' \
  'const transaction_progress& partial_progress' \
  'construction_dispatch_recovery_context construction(' \
  'check_dispatch_recovery_context check(' \
  'effect_restart_checkpoint operation('
do
  grep -F -- "$text" "$header" >/dev/null || fail "missing contract: $text"
done

for text in \
  'load_construction(' \
  'load_check(' \
  'load_latest(*completed.effect_attempt())' \
  'detail::rehydrate_construction_dispatch_evidence(' \
  'detail::rehydrate_check_dispatch_evidence(' \
  'detail::rehydrate_terminal_effectful_operation(' \
  'pkgstate::project_publication_request(' \
  'transaction_dispatch_state::completed' \
  'transaction_node_status::ready' \
  'progress.identity() != record.progress()'
do
  grep -F -- "$text" "$source" >/dev/null || fail "missing implementation: $text"
done

for forbidden in \
  'append(' \
  'publish(' \
  'execute_' \
  'resume_effectful_operation(' \
  'create_director' \
  'remove_all(' \
  'canonical_store' \
  'read_state(' \
  'main('
do
  ! grep -F -- "$forbidden" "$source" >/dev/null || \
    fail "rehydrator owns forbidden behavior: $forbidden"
done

grep -F '#include <libpkgstate/publication_projection.h>' "$source" \
  >/dev/null || fail 'rehydrator bypasses the state-owned projection API'
