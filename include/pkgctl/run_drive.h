// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_drive.h
 *  \brief Bounded serial driving of one durable transaction run.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <pkgctl/run_advance.h>

namespace pkgctl {

struct detail_transaction_run_drive_access;

/*! \brief Explicit upper bound for one serial drive call. */
class transaction_run_drive_policy final {
public:
  [[nodiscard]] static transaction_run_drive_policy make(
      std::size_t maximum_steps);
  [[nodiscard]] std::size_t maximum_steps() const noexcept;

private:
  explicit transaction_run_drive_policy(std::size_t maximum_steps) noexcept;
  std::size_t maximum_steps_;
};

/*! \brief Why one bounded serial drive call stopped. */
enum class transaction_run_drive_disposition : std::uint8_t {
  completed = 1,
  stopped_after_failure = 2,
  external_resolution_required = 3,
  quiescent_incomplete = 4,
  step_limit_reached = 5,
};

/*! \brief Exact ordered outcomes observed by one bounded drive call. */
class transaction_run_drive_result final {
public:
  [[nodiscard]] transaction_run_drive_disposition disposition() const noexcept;
  [[nodiscard]] const std::vector<transaction_run_advance_result>&
  steps() const noexcept;
  [[nodiscard]] const transaction_run_advance_result& last() const noexcept;
  [[nodiscard]] const transaction_run& run() const noexcept;
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;
  [[nodiscard]] std::size_t durable_step_count() const noexcept;
  [[nodiscard]] bool terminal() const noexcept;
  [[nodiscard]] bool external_resolution_required() const noexcept;

private:
  friend struct detail_transaction_run_drive_access;

  transaction_run_drive_result(
      transaction_run_drive_policy policy,
      transaction_run_drive_disposition disposition,
      std::vector<transaction_run_advance_result> steps);

  transaction_run_drive_policy policy_;
  transaction_run_drive_disposition disposition_;
  std::vector<transaction_run_advance_result> steps_;
};

/*! \brief Drive one journal serially for at most the explicit step bound.
 *
 * Every iteration reloads the committed run head through
 * advance_transaction_run_once(). Retained ownership is reconciled before new
 * work. The nonce source is consulted only when that committed head can reserve
 * fresh work. The call stops on completion, terminal failure containment,
 * external-resolution authority, incomplete quiescence, or the step bound. A
 * durable operation result that remains a started dispatch is itself an
 * external-resolution stop; the drive does not consume another no-op iteration
 * merely to rediscover that retained observation. It creates no worker,
 * concurrency, adaptive priority, retry timing, sleep,
 * discovery, rollback, cleanup, compaction, or command action.
 */
[[nodiscard]] transaction_run_drive_result drive_transaction_run(
    session_identity journal,
    transaction_run_drive_policy policy,
    transaction_dispatch_nonce_source& nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores);

} // namespace pkgctl
