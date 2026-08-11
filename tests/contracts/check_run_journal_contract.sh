#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
model="$srcdir/include/pkgctl/run_journal.h"
codec_header="$srcdir/include/pkgctl/run_journal_codec.h"
store_header="$srcdir/include/pkgctl/run_store.h"
commit_header="$srcdir/include/pkgctl/run_commit.h"
restart_header="$srcdir/include/pkgctl/run_restart.h"
model_source="$srcdir/src/run_journal.cpp"
codec="$srcdir/src/run_journal_codec.cpp"
store="$srcdir/src/run_store.cpp"
commit="$srcdir/src/run_commit.cpp"
restart="$srcdir/src/run_restart.cpp"
dispatch="$srcdir/src/dispatch.cpp"
progression="$srcdir/src/progression.cpp"

for file in "$model" "$codec_header" "$store_header" "$commit_header" \
            "$restart_header" "$model_source" "$codec" "$store" \
            "$commit" "$restart" \
            "$dispatch" "$progression"; do
  [ -s "$file" ] || {
    echo "missing transaction-run journal authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_run_nonce final' \
  'class transaction_run_journal_record final' \
  'transaction_run_journal_record::admit' \
  'transaction_run_journal_record::successor' \
  'transaction_run_journal_record::validate_successor_of' \
  'transaction_run_journal_record::reopen' \
  'validate_record_transition' \
  'validate_transition_snapshots' \
  'validate_durable_successor' \
  'transaction-run journal admission contains dispatch ownership' \
  'positive transaction-run journal sequence has no dispatch history' \
  'transaction-run journal has more reservations than transitions' \
  'transaction-run journal sequence disagrees with retained history' \
  'new dispatch reservation is detached from durable progression' \
  'validate_rehydrated_run_history' \
  'encode_transaction_run_record' \
  'decode_transaction_run_record' \
  'class posix_transaction_run_journal_store final' \
  'transaction_run_commit_checkpoint' \
  'commit_transaction_run_successor' \
  'operation_dispatch_start_checkpoint' \
  'commit_operation_dispatch_start' \
  'store_contract_violation' \
  'posix_transaction_run_journal_store::from_directory_fd' \
  'transaction_run_restart_checkpoint::make' \
  'transaction_dispatch_restart_disposition::release_reserved' \
  'transaction_dispatch_restart_disposition::recover_construction' \
  'transaction_dispatch_restart_disposition::recover_check' \
  'transaction_dispatch_restart_disposition::inspect_effect_journal' \
  'progress.contains_unit(dispatch.unit())' \
  'completed dispatch contradicts progression evidence' \
  'active operation dispatch retains a stale state epoch' \
  'pkgctl/transaction-run-journal-head/1' \
  'head_magic' \
  'read_head' \
  'publish_head' \
  'verify_existing_snapshot' \
  'has_unexpected_journal_record_entries' \
  'latest->identity() == record.identity()' \
  'transaction-run journal head names a missing snapshot' \
  '::flock' \
  '::linkat' \
  '::renameat' \
  '::fsync' \
  'O_NOFOLLOW'; do
  grep -F -- "$required" \
      "$model" "$codec_header" "$store_header" "$commit_header" \
      "$restart_header" "$model_source" "$codec" "$store" "$commit" \
      "$restart" \
      "$dispatch" "$progression" >/dev/null || {
    echo "missing transaction-run journal contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  'execute_construction(' \
  'execute_transaction_check(' \
  'execute_effectful_operation(' \
  'pkgexec::execute' \
  'pkgapply::apply' \
  'compare_and_publish' \
  'canonical_generation_store' \
  'libpkgexec-linux' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'pkgmk' \
  'pkgman'; do
  if grep -F -- "$forbidden" \
      "$model" "$codec_header" "$store_header" "$commit_header" \
      "$restart_header" "$model_source" "$codec" "$store" "$commit" \
      "$restart" \
      >/dev/null 2>&1; then
    echo "forbidden transaction-run journal shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'append\(|commit_transaction_run_successor|commit_operation_dispatch_start|transaction_run_restart_checkpoint::make' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'transaction-run journal mutation must not acquire a command frontend' >&2
  exit 1
fi

effect_line=$(awk '
  /commit_operation_dispatch_start\(/ { active = 1 }
  active && /effect_store\.append/ { print NR; exit }
' "$commit")
run_commit_line=$(awk '
  /commit_operation_dispatch_start\(/ { active = 1 }
  active && /commit_transaction_run_successor/ { print NR; exit }
' "$commit")
run_append_line=$(awk '
  /commit_transaction_run_successor\(/ { active = 1 }
  active && /run_store\.append/ { print NR; exit }
' "$commit")
[ -n "$effect_line" ] && [ -n "$run_commit_line" ] && \
    [ "$effect_line" -lt "$run_commit_line" ] && \
    [ -n "$run_append_line" ] || {
  echo 'operation start does not commit effect admission before run ownership' >&2
  exit 1
}
