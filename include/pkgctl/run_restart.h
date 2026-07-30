// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_restart.h
 *  \brief Conservative restart classification for durable transaction runs.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <pkgctl/run_journal.h>

namespace pkgctl {

/*! \brief Evidence required to resolve one active dispatch after restart. */
enum class transaction_dispatch_restart_disposition : std::uint8_t {
  release_reserved = 1,
  recover_construction = 2,
  recover_check = 3,
  inspect_effect_journal = 4,
};

class transaction_dispatch_restart_assessment final {
public:
  transaction_dispatch_restart_assessment(
      session_identity dispatch,
      transaction_unit_kind kind,
      transaction_dispatch_state state,
      transaction_dispatch_restart_disposition disposition,
      std::optional<session_identity> attempt_session,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations);

  [[nodiscard]] const session_identity& dispatch() const noexcept;
  [[nodiscard]] transaction_unit_kind kind() const noexcept;
  [[nodiscard]] transaction_dispatch_state state() const noexcept;
  [[nodiscard]] transaction_dispatch_restart_disposition
  disposition() const noexcept;
  [[nodiscard]] const std::optional<session_identity>&
  attempt_session() const noexcept;
  [[nodiscard]] const std::optional<session_identity>&
  effect_attempt() const noexcept;
  [[nodiscard]] const std::vector<session_identity>&
  observations() const noexcept;
  [[nodiscard]] bool external_evidence_required() const noexcept;

private:
  session_identity dispatch_;
  transaction_unit_kind kind_;
  transaction_dispatch_state state_;
  transaction_dispatch_restart_disposition disposition_;
  std::optional<session_identity> attempt_session_;
  std::optional<session_identity> effect_attempt_;
  std::vector<session_identity> observations_;
};

class transaction_run_restart_assessment final {
public:
  transaction_run_restart_assessment(
      session_identity journal,
      session_identity record,
      std::uint64_t sequence,
      std::vector<transaction_dispatch_restart_assessment> active);

  [[nodiscard]] const session_identity& journal() const noexcept;
  [[nodiscard]] const session_identity& record() const noexcept;
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  [[nodiscard]] const std::vector<transaction_dispatch_restart_assessment>&
  active() const noexcept;
  [[nodiscard]] bool quiescent() const noexcept;
  [[nodiscard]] bool external_evidence_required() const noexcept;

private:
  session_identity journal_;
  session_identity record_;
  std::uint64_t sequence_;
  std::vector<transaction_dispatch_restart_assessment> active_;
};

/*! \brief Exact rehydrated progression plus reopened durable dispatch ledger. */
class transaction_run_restart_checkpoint final {
public:
  [[nodiscard]] static transaction_run_restart_checkpoint make(
      transaction_progress progress,
      transaction_run_journal_record record);

  [[nodiscard]] const transaction_run& run() const noexcept;
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;
  [[nodiscard]] const transaction_run_restart_assessment&
  assessment() const noexcept;

private:
  transaction_run_restart_checkpoint(
      transaction_run run,
      transaction_run_journal_record record,
      transaction_run_restart_assessment assessment);

  transaction_run run_;
  transaction_run_journal_record record_;
  transaction_run_restart_assessment assessment_;
};

} // namespace pkgctl
