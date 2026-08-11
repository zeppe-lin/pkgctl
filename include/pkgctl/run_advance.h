// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_advance.h
 *  \brief One deterministic durable transaction-run advancement step.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include <pkgctl/run_authority.h>

namespace pkgctl {

struct detail_transaction_run_advance_access;

/*! \brief Caller-owned replay-safe dispatch nonce authority. */
class transaction_dispatch_nonce_source {
public:
  virtual ~transaction_dispatch_nonce_source() = default;

  /*! \brief Issue the nonce for fresh work at one committed run head.
   *
   * The source is called only after the storage-derived run is proven to have
   * no retained active ownership and at least one canonical ready unit. Exact
   * retries against the same record must return the same nonce. A successor
   * record may yield a different nonce.
   */
  [[nodiscard]] virtual transaction_dispatch_nonce issue(
      const transaction_run_journal_record& record,
      const transaction_run& run) = 0;
};

/*! \brief Caller-owned semantic authorities used by one advancement step. */
struct transaction_run_advance_authorities final {
  transaction_progress_rehydration_source& progress;
  transaction_dispatch_execution_authority_source& execution;
  transaction_dispatch_recovery_authority_source& recovery;
};

/*! \brief Call-scoped authorities for one freshly admitted operation. */
struct transaction_effect_execution_drivers final {
  std::unique_ptr<transaction_effect_driver> continuation;
  std::unique_ptr<transaction_effect_state_observer> resulting_state;
  transaction_effect_body_sink* bodies = nullptr;
};

/*! \brief Call-scoped authorities selected for one retained operation. */
struct transaction_effect_recovery_drivers final {
  std::unique_ptr<transaction_effect_driver> continuation;
  std::unique_ptr<transaction_effect_state_observer> resulting_state;
  std::unique_ptr<transaction_effect_publication_driver> publication;
  transaction_effect_body_sink* bodies = nullptr;
};

/*! \brief Caller-owned source of exact per-dispatch physical authority.
 *
 * A transaction may contain several operation dispatches. Each dispatch can
 * require another target lease, installed-state projection, archive, and
 * recovery binding. The source is consulted only after an exact semantic
 * execution or recovery handoff has been validated. Continuation authority is
 * separated from resulting-state observation and publication reconciliation;
 * returned objects are owned for one advancement call and are never durable.
 */
class transaction_effect_driver_source {
public:
  virtual ~transaction_effect_driver_source() = default;

  [[nodiscard]] virtual transaction_effect_execution_drivers
  acquire_execution_drivers(
      const transaction_dispatch_execution_handoff& handoff) = 0;

  [[nodiscard]] virtual transaction_effect_recovery_drivers
  acquire_recovery_drivers(
      const transaction_dispatch_recovery_handoff& handoff) = 0;
};

/*! \brief Caller-owned effectors used by one advancement step. */
struct transaction_run_advance_drivers final {
  construction_driver* construction;
  transaction_check_driver* check;
  transaction_effect_driver_source* operation;
};

/*! \brief Caller-owned durable stores used by one advancement step. */
struct transaction_run_advance_stores final {
  transaction_run_journal_store& runs;
  transaction_run_evidence_store& evidence;
  effect_journal_store* effects;
};

/*! \brief Observable outcome class of one bounded advancement call. */
enum class transaction_run_advance_disposition : std::uint8_t {
  quiescent = 1,
  released_reserved = 2,
  reconciled_construction = 3,
  reconciled_check = 4,
  reconciled_operation = 5,
  external_resolution_required = 6,
  executed_construction = 7,
  executed_check = 8,
  executed_operation = 9,
};

/*! \brief Operation evidence returned by fresh execution or restart repair.
 *
 * Fresh execution retains the exact effect-attempt admission used by the
 * write-ahead handoff; the effect journal may already have advanced beyond
 * that record while the physical operation ran. Reconciliation instead
 * returns the exact effect record selected or produced by restart. Callers
 * that need the current durable effect head after fresh execution must load it
 * from the effect journal by `record.attempt()`.
 */
struct transaction_run_operation_advance_evidence final {
  effect_attempt_record record;
  std::optional<effectful_operation_result> result;
  std::optional<effect_restart_disposition> restart_disposition;
};

/*! \brief Exact semantic evidence produced or accepted by one step. */
using transaction_run_advance_evidence = std::variant<
    std::monostate,
    construction_result,
    transaction_check_result,
    transaction_run_operation_advance_evidence>;

/*! \brief Storage-derived durable authority returned by one bounded step. */
class transaction_run_advance_result final {
public:
  [[nodiscard]] const transaction_run& run() const noexcept;
  [[nodiscard]] const transaction_run_journal_record& record() const noexcept;
  [[nodiscard]] transaction_run_advance_disposition disposition() const noexcept;
  [[nodiscard]] const std::optional<transaction_dispatch>&
  dispatch() const noexcept;
  [[nodiscard]] const transaction_run_advance_evidence& evidence() const noexcept;
  [[nodiscard]] const construction_result* construction() const noexcept;
  [[nodiscard]] const transaction_check_result* check() const noexcept;
  [[nodiscard]] const transaction_run_operation_advance_evidence*
  operation() const noexcept;
  [[nodiscard]] bool durable_transition_committed() const noexcept;
  [[nodiscard]] bool external_resolution_required() const noexcept;

private:
  friend struct detail_transaction_run_advance_access;

  friend transaction_run_advance_result advance_transaction_run_once(
      session_identity,
      transaction_dispatch_nonce,
      transaction_run_advance_authorities,
      transaction_run_advance_drivers,
      transaction_run_advance_stores);
  friend transaction_run_advance_result advance_transaction_run_once(
      session_identity,
      transaction_dispatch_nonce_source&,
      transaction_run_advance_authorities,
      transaction_run_advance_drivers,
      transaction_run_advance_stores);

  transaction_run_advance_result(
      transaction_run run,
      transaction_run_journal_record record,
      transaction_run_advance_disposition disposition,
      std::optional<transaction_dispatch> dispatch,
      transaction_run_advance_evidence evidence);

  transaction_run run_;
  transaction_run_journal_record record_;
  transaction_run_advance_disposition disposition_;
  std::optional<transaction_dispatch> dispatch_;
  transaction_run_advance_evidence evidence_;
};

/*! \brief Reconcile retained ownership or execute one newly reserved dispatch.
 *
 * The supplied journal identity selects one committed run-store head.  The
 * step loads that storage-derived record, rehydrates its semantic progression,
 * reconciles the first retained active
 * dispatch in durable ledger order, and returns without reserving new work.
 * Only a quiescent reopened run may reserve the first canonical ready unit.
 * That reservation is committed before execution authority is requested and
 * execution remains behind the existing durable start and terminal barriers.
 * No loop, retry policy, resource discovery, evidence discovery, or worker
 * scheduling is performed.
 */
[[nodiscard]] transaction_run_advance_result advance_transaction_run_once(
    session_identity journal,
    transaction_dispatch_nonce nonce,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores);

/*! \brief Advance once with a head-derived nonce requested only for fresh work. */
[[nodiscard]] transaction_run_advance_result advance_transaction_run_once(
    session_identity journal,
    transaction_dispatch_nonce_source& nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores);

} // namespace pkgctl
