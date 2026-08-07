// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_progress.h
 *  \brief Exact semantic progress reconstruction from durable authorities.
 */
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <pkgctl/effect_store.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_evidence_store.h>
#include <pkgctl/run_recovery.h>

namespace pkgctl {

/*! \brief Stable failure classes for exact progress reconstruction. */
enum class transaction_progress_rehydration_error_code : std::uint8_t {
  invalid_record = 1,
  evidence_missing = 2,
  evidence_mismatch = 3,
  unresolved_history = 4,
  progress_mismatch = 5,
};

/*! \brief Durable history cannot reproduce one exact semantic progression. */
class transaction_progress_rehydration_error final
    : public std::runtime_error {
public:
  transaction_progress_rehydration_error(
      transaction_progress_rehydration_error_code code,
      std::string message);

  [[nodiscard]] transaction_progress_rehydration_error_code
  code() const noexcept;

private:
  transaction_progress_rehydration_error_code code_;
};

/*! \brief Caller-owned complete semantic bodies required by retained evidence.
 *
 * Durable stores retain exact indexes, identities, and owner encodings. They do
 * not promote identities into source materializations, execution requests,
 * backend profiles, lifecycle results, application receipts, or publication
 * bodies. The owner of those bodies supplies them here and the rehydrator
 * validates every body against the exact durable record before accepting it.
 */
class transaction_progress_rehydration_context_source {
public:
  virtual ~transaction_progress_rehydration_context_source() = default;

  [[nodiscard]] virtual construction_dispatch_recovery_context construction(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const construction_dispatch_evidence_record& evidence) = 0;

  [[nodiscard]] virtual check_dispatch_recovery_context check(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const check_dispatch_evidence_record& evidence) = 0;

  [[nodiscard]] virtual effect_restart_checkpoint operation(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const effect_attempt_record& evidence) = 0;
};

/*! \brief Reconstruct exact progress from typed evidence and effect journals.
 *
 * Completed dispatches are replayed only when their graph unit becomes ready.
 * Construction and check bodies are decoded through their owning codecs.
 * Terminal operations are reconstructed from the exact latest effect record;
 * successful publication advances state through libpkgstate's pure projection.
 * Reserved, started, and released dispatches never become semantic progress.
 */
class stored_transaction_progress_rehydration_source final
    : public transaction_progress_rehydration_source {
public:
  stored_transaction_progress_rehydration_source(
      transaction_session transaction,
      transaction_run_evidence_store& evidence,
      effect_journal_store& effects,
      transaction_progress_rehydration_context_source& context);

  [[nodiscard]] transaction_progress rehydrate_progress(
      const transaction_run_journal_record& record) override;

private:
  transaction_session transaction_;
  transaction_run_evidence_store& evidence_;
  effect_journal_store& effects_;
  transaction_progress_rehydration_context_source& context_;
};

} // namespace pkgctl
