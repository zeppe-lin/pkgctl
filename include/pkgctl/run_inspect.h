// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_inspect.h
 *  \brief Read-only inspection of one exact durable transaction-run head.
 */
#pragma once

#include <cstdint>

#include <pkgctl/run_restart.h>
#include <pkgctl/run_store.h>

namespace pkgctl {

/*! \brief Storage-derived high-level state of one durable run head. */
enum class transaction_run_inspection_disposition : std::uint8_t {
  completed = 1,
  stopped_after_failure = 2,
  active = 3,
  quiescent_incomplete = 4,
};

/*! \brief Exact controller-owned evidence loaded from one durable journal. */
class transaction_run_inspection final {
public:
  [[nodiscard]] transaction_run_inspection_disposition
  disposition() const noexcept;
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;
  [[nodiscard]] const transaction_run_restart_assessment&
  assessment() const noexcept;
  [[nodiscard]] bool terminal() const noexcept;
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] bool external_evidence_required() const noexcept;

private:
  friend transaction_run_inspection inspect_transaction_run(
      session_identity,
      const transaction_run_journal_store&);

  transaction_run_inspection(
      transaction_run_inspection_disposition disposition,
      transaction_run_journal_record record,
      transaction_run_restart_assessment assessment);

  transaction_run_inspection_disposition disposition_;
  transaction_run_journal_record record_;
  transaction_run_restart_assessment assessment_;
};

/*! \brief Load and classify one caller-selected durable run head.
 *
 * The supplied identity selects exactly one journal. The controller loads its
 * committed head, validates that storage returned authority for that same
 * journal, and classifies only controller-owned record and dispatch evidence.
 * It does not rehydrate package semantics, discover journals, append records,
 * reserve or execute work, inspect subordinate effect journals, or invoke a
 * command action.
 */
[[nodiscard]] transaction_run_inspection inspect_transaction_run(
    session_identity journal,
    const transaction_run_journal_store& store);

} // namespace pkgctl
