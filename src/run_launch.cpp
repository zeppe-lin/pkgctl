// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_launch.h>

#include "run_admit_internal.h"

#include <utility>

namespace pkgctl {

struct detail_transaction_run_launch_access final {
  static transaction_run_launch_result make(
      transaction_run_launch_origin origin,
      transaction_run_journal_record starting_record,
      transaction_run_drive_result drive)
  {
    return transaction_run_launch_result(
        origin, std::move(starting_record), std::move(drive));
  }
};

namespace {

[[noreturn]] void invalid_launch(const char* message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition,
      message);
}

void validate_launch_result(
    transaction_run_launch_origin origin,
    const transaction_run_journal_record& starting,
    const transaction_run_drive_result& drive)
{
  if (starting.journal() != drive.record().journal() ||
      starting.transaction() != drive.record().transaction() ||
      starting.nonce() != drive.record().nonce() ||
      starting.policy().identity() != drive.record().policy().identity())
    invalid_launch("transaction launch crossed durable admission authority");
  if (drive.record().sequence() < starting.sequence())
    invalid_launch("transaction launch moved behind its starting record");
  if (origin == transaction_run_launch_origin::admitted &&
      (starting.sequence() != 0U || starting.previous()))
    invalid_launch("new transaction launch did not begin at sequence zero");
}

} // namespace

transaction_run_launch_result::transaction_run_launch_result(
    transaction_run_launch_origin origin,
    transaction_run_journal_record starting_record,
    transaction_run_drive_result drive)
    : origin_(origin), starting_record_(std::move(starting_record)),
      drive_(std::move(drive))
{
  validate_launch_result(origin_, starting_record_, drive_);
}

transaction_run_launch_origin
transaction_run_launch_result::origin() const noexcept
{
  return origin_;
}

bool transaction_run_launch_result::admission_committed() const noexcept
{
  return origin_ == transaction_run_launch_origin::admitted;
}

const transaction_run_journal_record&
transaction_run_launch_result::starting_record() const noexcept
{
  return starting_record_;
}

const transaction_run_drive_result&
transaction_run_launch_result::drive() const noexcept
{
  return drive_;
}

const transaction_run& transaction_run_launch_result::run() const noexcept
{
  return drive_.run();
}

const transaction_run_journal_record&
transaction_run_launch_result::record() const noexcept
{
  return drive_.record();
}

transaction_run_launch_result launch_transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy dispatch_policy,
    transaction_run_drive_policy drive_policy,
    transaction_run_nonce_source& run_nonces,
    transaction_dispatch_nonce_source& dispatch_nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  auto prepared = detail::prepare_transaction_run_admission(
      std::move(progress), std::move(dispatch_policy), run_nonces);

  auto existing = stores.runs.load_latest(prepared.record.journal());
  transaction_run_launch_origin origin =
      transaction_run_launch_origin::resumed;
  transaction_run_journal_record starting = prepared.record;
  if (existing)
  {
    detail::validate_existing_transaction_run_admission(
        prepared.record, *existing);
    starting = std::move(*existing);
  }
  else
  {
    auto admitted = detail::commit_transaction_run_admission(
        prepared, stores.runs);
    starting = std::move(admitted.record);
    origin = transaction_run_launch_origin::admitted;
  }

  auto drive = drive_transaction_run(
      starting.journal(), std::move(drive_policy), dispatch_nonces,
      authorities, drivers, stores);
  return detail_transaction_run_launch_access::make(
      origin, std::move(starting), std::move(drive));
}

} // namespace pkgctl
