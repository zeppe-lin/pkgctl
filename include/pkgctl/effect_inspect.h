// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file effect_inspect.h
 *  \brief Read-only inspection of one exact durable effect-attempt head.
 */
#pragma once

#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>

namespace pkgctl {

/*! \brief Exact controller-owned evidence loaded from one effect journal. */
class effect_attempt_inspection final {
public:
  [[nodiscard]] const effect_attempt_record& record() const noexcept;
  [[nodiscard]] const effect_restart_assessment& assessment() const noexcept;
  [[nodiscard]] bool terminal() const noexcept;
  [[nodiscard]] bool automatically_continuable() const noexcept;
  [[nodiscard]] bool external_resolution_required() const noexcept;

private:
  friend effect_attempt_inspection inspect_effect_attempt(
      session_identity,
      const effect_journal_store&);

  effect_attempt_inspection(
      effect_attempt_record record,
      effect_restart_assessment assessment);

  effect_attempt_record record_;
  effect_restart_assessment assessment_;
};

/*! \brief Load and classify one caller-selected durable effect-attempt head.
 *
 * The supplied identity selects exactly one effect journal. The controller
 * loads its committed head, validates that storage returned authority for that
 * same attempt, and classifies only the controller-owned record through the
 * existing pure restart assessment. It does not rehydrate lifecycle results,
 * application receipts or journals, transaction evidence, publication
 * requests or receipts, or installed-state snapshots. It does not discover
 * attempts, traverse run journals, append, reconcile, repair, or invoke a
 * driver.
 */
[[nodiscard]] effect_attempt_inspection inspect_effect_attempt(
    session_identity attempt,
    const effect_journal_store& store);

} // namespace pkgctl
