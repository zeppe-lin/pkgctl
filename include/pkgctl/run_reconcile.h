// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_reconcile.h
 *  \brief Durable reconciliation of one dispatch selected by restart.
 */
#pragma once

#include <optional>

#include <pkgctl/effect_restart.h>
#include <pkgctl/run_commit.h>
#include <pkgctl/run_restart.h>

namespace pkgctl {

/*! \brief Durable successor after releasing one never-started reservation. */
struct reserved_dispatch_reconciliation_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
};

/*! \brief Durable successor after accepting recovered construction evidence. */
struct construction_dispatch_reconciliation_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
  construction_result result;
};

/*! \brief Durable successor after accepting recovered check evidence. */
struct check_dispatch_reconciliation_checkpoint final {
  transaction_run run;
  transaction_run_journal_record record;
  transaction_check_result result;
};

/*! \brief Effect continuation plus the resulting durable run authority.
 *
 * When run_advanced is false, no transaction-run successor was committed and
 * disposition requires external resolution.  Otherwise the exact returned
 * result was submitted to the dispatch ledger and record is the durable
 * committed successor.
 */
struct operation_dispatch_reconciliation_result final {
  transaction_run run;
  transaction_run_journal_record record;
  effect_restart_disposition disposition;
  effect_attempt_record effect_record;
  std::optional<effectful_operation_result> result;
  bool run_advanced;
};

/*! \brief Durably release one reservation classified as never started. */
[[nodiscard]] reserved_dispatch_reconciliation_checkpoint
reconcile_reserved_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    transaction_run_journal_store& run_store);

/*! \brief Validate recovered construction evidence and durably retire it. */
[[nodiscard]] construction_dispatch_reconciliation_checkpoint
reconcile_construction_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    construction_result result,
    transaction_run_journal_store& run_store);

/*! \brief Validate recovered check evidence and durably retire it. */
[[nodiscard]] check_dispatch_reconciliation_checkpoint
reconcile_check_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    transaction_check_result result,
    transaction_run_journal_store& run_store);

/*! \brief Return whether recovery can continue lifecycle or application work. */
[[nodiscard]] bool operation_reconciliation_requires_continuation_driver(
    const effect_restart_checkpoint& checkpoint);

/*! \brief Return whether recovery needs a resulting canonical-state read. */
[[nodiscard]] bool operation_reconciliation_requires_state_observer(
    const effect_restart_checkpoint& checkpoint);

/*! \brief Return whether recovery must reconcile an exact publication request. */
[[nodiscard]] bool operation_reconciliation_requires_publication_driver(
    const effect_restart_checkpoint& checkpoint);

/*! \brief Continue one exact durable operation attempt and commit its result.
 *
 * The supplied effect checkpoint must belong to the started dispatch and name
 * its exact retained effect-attempt authority.  Automatically continuable
 * effect states are resumed through the existing effect journal.  A terminal
 * or uncertainty-bearing result is then submitted through the ordinary
 * dispatch function and committed as one exact run successor.  A disposition
 * requiring external resolution invokes no effect driver and commits no run
 * successor.
 */
[[nodiscard]] operation_dispatch_reconciliation_result
reconcile_operation_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    effect_restart_checkpoint effect_checkpoint,
    transaction_effect_driver* continuation,
    transaction_effect_state_observer* resulting_state,
    transaction_effect_publication_driver* publication,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store);

} // namespace pkgctl
