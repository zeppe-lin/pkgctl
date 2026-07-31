// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_admit.h>

#include "run_admit_internal.h"

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

void validate_exact_committed_admission(
    const transaction_run_journal_record& expected,
    const transaction_run_journal_record& committed)
{
  if (committed.identity() != expected.identity() ||
      committed.journal() != expected.journal() ||
      committed.transaction() != expected.transaction() ||
      committed.nonce() != expected.nonce() ||
      committed.sequence() != 0U || committed.previous() ||
      committed.run() != expected.run() ||
      committed.progress() != expected.progress() ||
      committed.current_state() != expected.current_state() ||
      committed.policy().identity() != expected.policy().identity() ||
      !committed.dispatches().empty() ||
      committed.complete() != expected.complete() ||
      committed.failed() != expected.failed() ||
      committed.stopped() != expected.stopped())
  {
    store_contract_violation(
        "run store returned foreign transaction-run admission authority");
  }
}

} // namespace

namespace detail {

prepared_transaction_run_admission prepare_transaction_run_admission(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    transaction_run_nonce_source& nonces)
{
  auto initial = transaction_run::begin(
      std::move(progress), std::move(policy));
  auto nonce = nonces.issue(initial);
  auto expected = transaction_run_journal_record::admit(
      initial, std::move(nonce));
  return prepared_transaction_run_admission{
      std::move(initial), std::move(expected)};
}

transaction_run_admission_checkpoint commit_transaction_run_admission(
    const prepared_transaction_run_admission& prepared,
    transaction_run_journal_store& store)
{
  auto committed = store.append(prepared.record);
  validate_exact_committed_admission(prepared.record, committed);
  auto reopened = committed.reopen(prepared.run.progress());
  return transaction_run_admission_checkpoint{
      std::move(reopened), std::move(committed)};
}

void validate_existing_transaction_run_admission(
    const transaction_run_journal_record& expected,
    const transaction_run_journal_record& committed)
{
  if (committed.journal() != expected.journal() ||
      committed.transaction() != expected.transaction() ||
      committed.nonce() != expected.nonce() ||
      committed.policy().identity() != expected.policy().identity())
  {
    store_contract_violation(
        "run store returned foreign transaction-run launch authority");
  }
  if (committed.sequence() == 0U)
    validate_exact_committed_admission(expected, committed);
}

} // namespace detail

transaction_run_admission_checkpoint admit_transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    transaction_run_nonce_source& nonces,
    transaction_run_journal_store& store)
{
  auto prepared = detail::prepare_transaction_run_admission(
      std::move(progress), std::move(policy), nonces);
  return detail::commit_transaction_run_admission(prepared, store);
}

} // namespace pkgctl
