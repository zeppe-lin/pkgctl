// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_launch.h
 *  \brief Restart-safe admission and bounded driving of one transaction run.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <pkgctl/run_admit.h>
#include <pkgctl/run_drive.h>

namespace pkgctl {

struct detail_transaction_run_launch_access;

/*! \brief How the durable journal existed when one launch call began. */
enum class transaction_run_launch_origin : std::uint8_t {
  admitted = 1,
  resumed = 2,
};

/*! \brief Durable admission and bounded-drive authority from one launch call. */
class transaction_run_launch_result final {
public:
  [[nodiscard]] transaction_run_launch_origin origin() const noexcept;
  [[nodiscard]] bool admission_committed() const noexcept;
  [[nodiscard]] const transaction_run_journal_record&
  starting_record() const noexcept;
  [[nodiscard]] const transaction_run_drive_result& drive() const noexcept;
  [[nodiscard]] const transaction_run& run() const noexcept;
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;

private:
  friend struct detail_transaction_run_launch_access;

  transaction_run_launch_result(
      transaction_run_launch_origin origin,
      transaction_run_journal_record starting_record,
      transaction_run_drive_result drive);

  transaction_run_launch_origin origin_;
  transaction_run_journal_record starting_record_;
  transaction_run_drive_result drive_;
};

/*! \brief Converge on one exact journal and drive it under an explicit bound.
 *
 * The controller constructs the immutable initial run and obtains its
 * replay-safe run nonce. If that exact journal has no committed head, sequence
 * zero is appended before any drive action. If the journal already has a head,
 * its transaction, nonce, and dispatch policy must match the expected admission
 * universe and no admission append occurs. The bounded drive then reloads and
 * advances that same journal. A failure before admission commits performs no
 * drive; a failure after admission leaves a durable journal that an exact retry
 * resumes. No journal discovery, unbounded execution, worker, concurrency,
 * retry timing, cleanup, rollback, compaction, or command action occurs here.
 */
[[nodiscard]] transaction_run_launch_result launch_transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy dispatch_policy,
    transaction_run_drive_policy drive_policy,
    transaction_run_nonce_source& run_nonces,
    transaction_dispatch_nonce_source& dispatch_nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores);

} // namespace pkgctl
