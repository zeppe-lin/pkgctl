// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_admit.h>

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

void validate_committed_admission(
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

transaction_run_admission_checkpoint admit_transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    transaction_run_nonce_source& nonces,
    transaction_run_journal_store& store)
{
  auto initial = transaction_run::begin(progress, std::move(policy));
  auto nonce = nonces.issue(initial);
  auto expected = transaction_run_journal_record::admit(initial, std::move(nonce));
  auto committed = store.append(expected);
  validate_committed_admission(expected, committed);
  auto reopened = committed.reopen(std::move(progress));
  return transaction_run_admission_checkpoint{
      std::move(reopened), std::move(committed)};
}

} // namespace pkgctl
