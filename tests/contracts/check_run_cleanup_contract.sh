#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
header="$root/include/pkgctl/run_cleanup.h"
source="$root/src/run_cleanup.cpp"
command="$root/cli/run_command.cpp"
unit="$root/tests/unit/run_cleanup_test.cpp"
cli="$root/tests/integration/cli_build_cleanup_test.sh"
process="$root/tests/integration/cli_build_process_death_test.sh"
refusal="$root/tests/integration/cli_build_check_authority_refusal_test.sh"
inspector="$root/tests/fixtures/run_evidence_inspect_fixture.cpp"
design="$root/DESIGN.md"
readme="$root/README.md"
history="$root/HISTORY.md"
manual="$root/man/pkgctl.1.scd"

fail()
{
  echo "run-cleanup-contract: $*" >&2
  exit 1
}

require()
{
  file=$1
  text=$2
  grep -F -- "$text" "$file" >/dev/null || \
    fail "${file#$root/} omits: $text"
}

forbid()
{
  file=$1
  text=$2
  if grep -F -- "$text" "$file" >/dev/null; then
    fail "${file#$root/} retains forbidden text: $text"
  fi
}

for file in \
  "$header" "$source" "$command" "$unit" "$cli" "$process" "$refusal" \
  "$inspector" "$design" "$readme" "$history" "$manual"; do
  [ -s "$file" ] || fail "missing cleanup qualification source: ${file#$root/}"
done

# Cleanup authority is a projection from a terminal durable run head, not a
# filesystem scan and not another durable truth subsystem.
for text in \
  'enum class transaction_run_private_realization_kind' \
  'enum class transaction_run_cleanup_disposition' \
  'class transaction_run_cleanup_plan final' \
  'Only a successfully completed run owns cleanup authority.' \
  'class transaction_run_private_realization_cleaner' \
  'cleanup_transaction_run_private_realizations('; do
  require "$header" "$text"
done
require "$source" 'record.complete() && !record.failed() && !record.stopped()'
require "$source" 'for (const auto& dispatch_record : record.dispatches())'
require "$source" 'dispatch_record.state() != transaction_dispatch_state::completed'
require "$source" 'roots.construction_session_root'
require "$source" 'roots.package_output_root'
require "$source" 'roots.check_resource_root'
require "$source" 'roots.check_temporary_root'

# The mechanism is descriptor anchored and must refuse path substitution rather
# than following attacker-selected links out of private authority.
require "$source" 'O_NOFOLLOW'
require "$source" 'AT_SYMLINK_NOFOLLOW'
require "$source" '::openat('
require "$source" '::unlinkat('
require "$source" '::fchmod('
require "$source" 'S_IRWXU'
if grep -F 'std::filesystem::remove_all' "$source" >/dev/null || \
   grep -F 'system(' "$source" >/dev/null; then
  fail 'production cleanup fell back to pathname-recursive deletion'
fi

# Frontend cleanup is strictly post-result and operational. A sweep refusal is
# reported but cannot rewrite a completed transaction result or exit status.
require "$command" 'cleanup_terminal_private_realizations('
require "$command" 'pkgctl: private realization cleanup incomplete:'
require "$command" 'pkgctl: private realization cleanup unavailable:'
launch_render=$(grep -n 'render_terminal_failure(result.drive())' "$command" | head -n 1 | cut -d: -f1)
launch_cleanup=$(grep -n 'cleanup_terminal_private_realizations(' "$command" | tail -n 2 | head -n 1 | cut -d: -f1)
resume_render=$(grep -n 'render_terminal_failure(result);' "$command" | head -n 1 | cut -d: -f1)
resume_cleanup=$(grep -n 'cleanup_terminal_private_realizations(result.record()' "$command" | head -n 1 | cut -d: -f1)
[ "$launch_render" -lt "$launch_cleanup" ] || fail 'fresh-run cleanup precedes terminal reporting'
[ "$resume_render" -lt "$resume_cleanup" ] || fail 'resume cleanup precedes terminal reporting'

# API qualification attacks both build and check stage eligibility, partial
# mechanism failure, repeated execution, exact-target symlink substitution, and
# accidental discovery of unplanned siblings.
for text in \
  'check_released_reservation_is_not_cleanup_authority()' \
  'check_build_and_check_stage_authority()' \
  'check_posix_cleanup_never_discovers_foreign_siblings()' \
  'check_posix_cleanup_refuses_ancestor_substitution()' \
  'check_posix_cleanup_removes_sealed_owner_directories()' \
  'check_posix_cleanup_is_idempotent_and_nofollow()' \
  'value.completed.dispatches().front().dispatch().identity()' \
  'injected cleanup refusal' \
  'unknown cleanup failure' \
  'fs::create_directory_symlink(external, hostile_target.path())' \
  'fs::perms::owner_read | fs::perms::owner_exec' \
  'not-authorized'; do
  require "$unit" "$text"
done

# The CLI campaign crosses two resumable build stages, attacks one exact leaf,
# completes check from durable authority, then retries a partial sweep with no
# live catalog and zero durable transaction work.
for text in \
  'build archive-probe --check' \
  'private realization was cleaned while transaction remained resumable' \
  'ln -s "$external" "$hostile_target"' \
  'private realization cleanup incomplete:' \
  'constructions 2' \
  'checks 1' \
  'construction-evidence 2' \
  'check-evidence 1' \
  'rm -rf "$collection"' \
  "'durable-steps 0'" \
  'cleanup retry changed durable transaction/evidence projection'; do
  require "$cli" "$text"
done

# A process can die after terminal truth is durable but before cleanup. Resume
# must derive the same cleanup plan without replaying durable package work.
require "$process" 'transaction-completed'
require "$process" '"$runtime/run" 9 --'
require "$process" 'terminal crash did not retain expected pre-cleanup residue'
require "$process" "'durable-steps 0'"

# A failed check remains forensic/restart authority. Successful predecessor
# builds do not authorize a partial terminal sweep.
require "$refusal" 'must-survive-failed-check'
require "$refusal" 'failed check cleaned construction-session residue'
require "$refusal" 'failed check cleaned package-output residue'
require "$refusal" 'failed transaction attempted terminal cleanup'

# The inspector validates durable construction/check evidence independently of
# disposable runtime trees.
require "$inspector" 'completed construction lacks retained evidence'
require "$inspector" 'completed check lacks retained evidence'
require "$inspector" 'evidence.journal() != record.journal()'
require "$inspector" 'evidence.transaction() != record.transaction()'
require "$inspector" 'evidence.dispatch() != dispatch.identity()'
require "$inspector" 'evidence.node() != dispatch.unit().primary_node()'
require "$inspector" 'evidence.attempt_session() != *retained.attempt_session()'
require "$inspector" 'evidence.result() != *retained.terminal_evidence()'
require "$inspector" ' evidence binding contradicts durable run head'
forbid "$inspector" 'evidence->identity() != *retained.terminal_evidence()'

# User-facing documentation keeps the same authority split: completed dispatches
# own disposable realization; released/incomplete/failed history does not, and
# cleanup never becomes durable transaction truth.
require "$design" 'Released-unstarted reservations also project nothing'
require "$design" 'Cleanup adds no durable schema or historical bit'
require "$readme" 'Released reservations own no disposable'
require "$history" 'released-unstarted reservations authorize no deletion'
require "$manual" '*check-resources*'
require "$manual" 'failure is an operational warning rather than rewritten transaction truth.'
