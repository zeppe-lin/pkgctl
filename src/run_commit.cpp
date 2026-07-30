// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_commit.h>

#include <string>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void store_contract_violation(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::store_contract_violation,
      message);
}

void validate_effect_commit(
    const effect_attempt_record& expected,
    const effect_attempt_record& committed)
{
  if (committed.identity() != expected.identity() ||
      committed.attempt() != expected.attempt() ||
      committed.session() != expected.session() ||
      committed.sequence() != expected.sequence())
  {
    store_contract_violation(
        "effect store returned foreign operation-start authority");
  }
}

void validate_run_commit(
    const transaction_run_journal_record& expected,
    const transaction_run_journal_record& committed)
{
  if (committed.identity() != expected.identity() ||
      committed.journal() != expected.journal() ||
      committed.sequence() != expected.sequence())
  {
    store_contract_violation(
        "run store returned foreign operation-start authority");
  }
}

} // namespace

operation_dispatch_start_checkpoint commit_operation_dispatch_start(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session,
    effect_attempt_nonce nonce,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store)
{
  const auto reopened = current_record.reopen(run.progress());
  if (reopened.identity() != run.identity())
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_transition,
        "operation start does not continue the supplied durable run");
  }

  auto started = start_operation_dispatch(
      std::move(run), dispatch, session, std::move(nonce));

  const auto committed_effect = effect_store.append(started.effect_attempt);
  validate_effect_commit(started.effect_attempt, committed_effect);

  auto next_record = current_record.successor(started.run);
  const auto committed_run = run_store.append(next_record);
  validate_run_commit(next_record, committed_run);

  return operation_dispatch_start_checkpoint{
      std::move(started.run), std::move(next_record),
      std::move(started.effect_attempt)};
}

} // namespace pkgctl
