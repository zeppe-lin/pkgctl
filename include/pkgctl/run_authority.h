// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_authority.h
 *  \brief Exact caller-supplied authorities for durable transaction runs.
 */
#pragma once

#include <variant>

#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_execute.h>

namespace pkgctl {

/*! \brief Exact operation execution resources plus caller-issued attempt nonce. */
struct operation_dispatch_execution_authority final {
  effectful_operation_session session;
  effect_attempt_nonce nonce;
};

/*! \brief Closed executable authority for one reserved dispatch. */
using transaction_dispatch_execution_authority_body = std::variant<
    construction_session,
    transaction_check_session,
    operation_dispatch_execution_authority>;

/*! \brief Recoverable construction authority: replay or retained result. */
using construction_dispatch_recovery_authority = std::variant<
    construction_session,
    construction_result>;

/*! \brief Recoverable check authority: replay or retained result. */
using check_dispatch_recovery_authority = std::variant<
    transaction_check_session,
    transaction_check_result>;

/*! \brief Closed recovery authority for one restart-classified dispatch. */
using transaction_dispatch_recovery_authority_body = std::variant<
    std::monostate,
    construction_session,
    construction_result,
    transaction_check_session,
    transaction_check_result,
    effect_restart_checkpoint>;

/*! \brief Caller-owned source of exact semantic progression after restart.
 *
 * Implementations may load, reconstruct, or otherwise obtain authority from
 * their owning components.  pkgctl receives only the resulting semantic value
 * and validates it against the durable run record.
 */
class transaction_progress_rehydration_source {
public:
  virtual ~transaction_progress_rehydration_source() = default;

  [[nodiscard]] virtual transaction_progress rehydrate_progress(
      const transaction_run_journal_record& record) = 0;
};

/*! \brief Fresh construction/check sessions for one exact dispatch.
 *
 * Implementations select explicit paths and call-scoped resources but do not
 * execute the dispatch. Fresh construction admits the resulting session into
 * durable controller evidence before terminal retirement; construction restart
 * consumes that retained session authority instead of consulting this source
 * again. Check restart may still require this source until its own admitted
 * session is retained. User run intent is deliberately outside
 * construction/check resource authority.
 */
class transaction_dispatch_session_source {
public:
  virtual ~transaction_dispatch_session_source() = default;

  [[nodiscard]] virtual construction_session construction(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) = 0;

  [[nodiscard]] virtual transaction_check_session check(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Caller-owned operation session and attempt nonce for fresh work. */
class transaction_operation_execution_authority_source {
public:
  virtual ~transaction_operation_execution_authority_source() = default;

  [[nodiscard]] virtual operation_dispatch_execution_authority operation(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Caller-owned source of concrete resources for one fresh dispatch. */
class transaction_dispatch_execution_authority_source {
public:
  virtual ~transaction_dispatch_execution_authority_source() = default;

  [[nodiscard]] virtual construction_session construction(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) = 0;

  [[nodiscard]] virtual transaction_check_session check(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) = 0;

  [[nodiscard]] virtual operation_dispatch_execution_authority operation(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Compose construction/check authority with optional operation input. */
class composed_transaction_dispatch_execution_authority_source final
    : public transaction_dispatch_execution_authority_source {
public:
  explicit composed_transaction_dispatch_execution_authority_source(
      transaction_dispatch_session_source& sessions);

  composed_transaction_dispatch_execution_authority_source(
      transaction_dispatch_session_source& sessions,
      transaction_operation_execution_authority_source& operations);

  [[nodiscard]] construction_session construction(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] transaction_check_session check(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] operation_dispatch_execution_authority operation(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) override;

private:
  transaction_dispatch_session_source& sessions_;
  transaction_operation_execution_authority_source* operations_;
};

/*! \brief Caller-owned source of exact evidence for one active restart item. */
class transaction_dispatch_recovery_authority_source {
public:
  virtual ~transaction_dispatch_recovery_authority_source() = default;

  [[nodiscard]] virtual construction_dispatch_recovery_authority construction(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) = 0;

  [[nodiscard]] virtual check_dispatch_recovery_authority check(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) = 0;

  [[nodiscard]] virtual effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Exact durable record, reopened run, dispatch, and executable input. */
class transaction_dispatch_execution_handoff final {
public:
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;
  [[nodiscard]] const transaction_run& run() const noexcept;
  [[nodiscard]] const transaction_dispatch& dispatch() const noexcept;
  [[nodiscard]] transaction_unit_kind kind() const noexcept;
  [[nodiscard]] const transaction_dispatch_execution_authority_body&
  authority() const noexcept;
  [[nodiscard]] const construction_session* construction() const noexcept;
  [[nodiscard]] const transaction_check_session* check() const noexcept;
  [[nodiscard]] const operation_dispatch_execution_authority*
  operation() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend transaction_dispatch_execution_handoff
  acquire_transaction_dispatch_execution_authority(
      transaction_run_journal_record,
      transaction_run,
      transaction_dispatch,
      transaction_dispatch_execution_authority_source&);

  transaction_dispatch_execution_handoff(
      transaction_run_journal_record record,
      transaction_run run,
      transaction_dispatch dispatch,
      transaction_dispatch_execution_authority_body authority,
      session_identity identity);

  transaction_run_journal_record record_;
  transaction_run run_;
  transaction_dispatch dispatch_;
  transaction_dispatch_execution_authority_body authority_;
  session_identity identity_;
};

/*! \brief Exact restart checkpoint, dispatch, and selected recovery authority. */
class transaction_dispatch_recovery_handoff final {
public:
  [[nodiscard]] const transaction_run_restart_checkpoint&
  checkpoint() const noexcept;
  [[nodiscard]] const transaction_dispatch_restart_assessment&
  assessment() const noexcept;
  [[nodiscard]] const transaction_dispatch& dispatch() const noexcept;
  [[nodiscard]] transaction_dispatch_restart_disposition
  disposition() const noexcept;
  [[nodiscard]] const transaction_dispatch_recovery_authority_body&
  authority() const noexcept;
  [[nodiscard]] bool releases_reserved() const noexcept;
  [[nodiscard]] const construction_session* construction_retry() const noexcept;
  [[nodiscard]] const construction_result* construction() const noexcept;
  [[nodiscard]] const transaction_check_session* check_retry() const noexcept;
  [[nodiscard]] const transaction_check_result* check() const noexcept;
  [[nodiscard]] const effect_restart_checkpoint* operation() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend transaction_dispatch_recovery_handoff
  acquire_transaction_dispatch_recovery_authority(
      transaction_run_restart_checkpoint,
      transaction_dispatch,
      transaction_dispatch_recovery_authority_source&);

  transaction_dispatch_recovery_handoff(
      transaction_run_restart_checkpoint checkpoint,
      transaction_dispatch_restart_assessment assessment,
      transaction_dispatch dispatch,
      transaction_dispatch_recovery_authority_body authority,
      session_identity identity);

  transaction_run_restart_checkpoint checkpoint_;
  transaction_dispatch_restart_assessment assessment_;
  transaction_dispatch dispatch_;
  transaction_dispatch_recovery_authority_body authority_;
  session_identity identity_;
};

/*! \brief Obtain semantic progression and reopen one exact durable run. */
[[nodiscard]] transaction_run_restart_checkpoint rehydrate_transaction_run(
    transaction_run_journal_record record,
    transaction_progress_rehydration_source& source);

/*! \brief Obtain and validate concrete execution authority for one reservation. */
[[nodiscard]] transaction_dispatch_execution_handoff
acquire_transaction_dispatch_execution_authority(
    transaction_run_journal_record record,
    transaction_run run,
    transaction_dispatch dispatch,
    transaction_dispatch_execution_authority_source& source);

/*! \brief Obtain and validate recovery authority for one active restart item. */
[[nodiscard]] transaction_dispatch_recovery_handoff
acquire_transaction_dispatch_recovery_authority(
    transaction_run_restart_checkpoint checkpoint,
    transaction_dispatch dispatch,
    transaction_dispatch_recovery_authority_source& source);

} // namespace pkgctl
