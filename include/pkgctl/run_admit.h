// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_admit.h
 *  \brief Durable admission of one initial transaction run.
 */
#pragma once

#include <pkgctl/run_store.h>

namespace pkgctl {

/*! \brief Caller-owned replay-safe authority for one run-history nonce. */
class transaction_run_nonce_source {
public:
  virtual ~transaction_run_nonce_source() = default;

  /*! \brief Issue the nonce for one exact initial transaction run.
   *
   * Exact retries for the same run identity must return the same nonce. A
   * distinct initial run must not intentionally reuse a nonce in the same
   * transaction and dispatch-policy domain.
   */
  [[nodiscard]] virtual transaction_run_nonce issue(
      const transaction_run& run) = 0;
};

/*! \brief Storage-derived authority after one initial run is committed. */
struct transaction_run_admission_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
};

/*! \brief Durably admit sequence zero of one transaction-run history.
 *
 * The controller first constructs the exact immutable initial run, then asks
 * the caller-owned source for replay-safe nonce authority, derives sequence
 * zero, and appends that record. Exact retries are idempotent when the source
 * returns the same nonce. The returned run is reopened from the record returned
 * by storage. No reservation, execution authority, driver, effect store,
 * scheduler, loop, retry timing, discovery, rollback, cleanup, or command
 * action occurs here.
 */
[[nodiscard]] transaction_run_admission_checkpoint admit_transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    transaction_run_nonce_source& nonces,
    transaction_run_journal_store& store);

} // namespace pkgctl
