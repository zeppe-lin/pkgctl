// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_nonce.h>

#include <string>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_nonce_authority(const char* message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition, message);
}

void validate_nonce_authority(
    const transaction_run_journal_record& record,
    const transaction_run& run)
{
  auto reopened = record.reopen(run.progress());
  if (reopened.identity() != run.identity())
    invalid_nonce_authority(
        "dispatch nonce authority does not match the committed run head");
}

} // namespace

transaction_dispatch_nonce canonical_transaction_dispatch_nonce(
    const transaction_run_journal_record& record,
    const transaction_run& run)
{
  validate_nonce_authority(record, run);
  const auto identity = make_session_identity(
      "pkgctl/transaction-dispatch-nonce/1",
      {record.journal().hex(), record.identity().hex(), run.identity().hex()});
  return transaction_dispatch_nonce::from_hex(identity.hex());
}

transaction_dispatch_nonce
canonical_transaction_dispatch_nonce_source::issue(
    const transaction_run_journal_record& record,
    const transaction_run& run)
{
  return canonical_transaction_dispatch_nonce(record, run);
}

} // namespace pkgctl
