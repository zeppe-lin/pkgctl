// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_commit.h
 *  \brief Ordered durable commitment of operation dispatch ownership.
 */
#pragma once

#include <pkgctl/effect_store.h>
#include <pkgctl/run_store.h>

namespace pkgctl {

/*! \brief Exact durable authorities after committing an operation start. */
struct operation_dispatch_start_checkpoint final {
  transaction_run run;
  transaction_run_journal_record run_record;
  effect_attempt_record effect_attempt;
};

/*! \brief Commit effect admission before durable started-run ownership.
 *
 * This is the only safe cross-journal order.  Both appends are exact and
 * idempotent, so a caller may retry this function with the same arguments after
 * an interrupted call.  No effect driver is invoked here.
 */
[[nodiscard]] operation_dispatch_start_checkpoint
commit_operation_dispatch_start(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session,
    effect_attempt_nonce nonce,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store);

} // namespace pkgctl
