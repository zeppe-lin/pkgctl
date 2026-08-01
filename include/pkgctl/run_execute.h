// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_execute.h
 *  \brief One-dispatch execution behind durable transaction-run barriers.
 */
#pragma once

#include <pkgctl/check.h>
#include <pkgctl/construction.h>
#include <pkgctl/effect.h>
#include <pkgctl/run_commit.h>

namespace pkgctl {

/*! \brief Durable terminal checkpoint for one construction dispatch. */
struct construction_dispatch_execution_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
  construction_result result;
};

/*! \brief Durable terminal checkpoint for one check dispatch. */
struct check_dispatch_execution_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
  transaction_check_result result;
};

/*! \brief Durable checkpoint after one operation execution attempt.
 *
 * The resulting dispatch may remain started when the effect result is an
 * uncertainty observation.  The committed run record retains that observation
 * and the exact effect-attempt admission remains authoritative in its own
 * journal.
 */
struct operation_dispatch_execution_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
  effect_attempt_record admission;
  effectful_operation_result result;
};

/*! \brief Start, execute, and durably retire one construction dispatch.
 *
 * The started run successor is committed before the construction driver is
 * invoked.  If execution or terminal commitment fails, no terminal run record
 * is fabricated; restart therefore observes the exact started dispatch.
 */
[[nodiscard]] construction_dispatch_execution_checkpoint
execute_construction_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_session session,
    construction_driver& driver,
    transaction_run_journal_store& run_store);

/*! \brief Start, execute, and durably retire one check dispatch.
 *
 * The started run successor is committed before the check driver is invoked.
 * Driver or terminal-commit failure leaves the exact started ownership durable
 * for external recovery.
 */
[[nodiscard]] check_dispatch_execution_checkpoint
execute_check_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_session session,
    transaction_check_driver& driver,
    transaction_run_journal_store& run_store);

/*! \brief Start and execute one operation through both durable journals.
 *
 * Effect admission is committed before started-run ownership, and both are
 * committed before the effect driver is invoked.  Definitive results retire
 * the dispatch; lost-lease and indeterminate-publication results remain
 * durably retained observations on the started dispatch.
 */
[[nodiscard]] operation_dispatch_execution_checkpoint
execute_operation_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    effectful_operation_session session,
    effect_attempt_nonce nonce,
    transaction_effect_driver& continuation,
    transaction_effect_state_observer& resulting_state,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store);

} // namespace pkgctl
