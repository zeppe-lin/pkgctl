// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file dispatch.h
 *  \brief Immutable transaction dispatch reservations and completion ledger.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <pkgctl/effect_journal.h>
#include <pkgctl/progression.h>

namespace pkgctl {

class transaction_run_journal_record;

inline constexpr std::size_t transaction_dispatch_nonce_size = 32U;

/*! \brief Caller-issued, transaction-local dispatch attempt nonce. */
class transaction_dispatch_nonce final {
public:
  using byte_array =
      std::array<std::uint8_t, transaction_dispatch_nonce_size>;

  [[nodiscard]] static transaction_dispatch_nonce from_bytes(byte_array bytes);
  [[nodiscard]] static transaction_dispatch_nonce from_hex(std::string value);

  [[nodiscard]] const byte_array& bytes() const noexcept;
  [[nodiscard]] std::string hex() const;

  friend bool operator==(const transaction_dispatch_nonce&,
                         const transaction_dispatch_nonce&) noexcept;
  friend bool operator!=(const transaction_dispatch_nonce&,
                         const transaction_dispatch_nonce&) noexcept;
  friend bool operator<(const transaction_dispatch_nonce&,
                        const transaction_dispatch_nonce&) noexcept;

private:
  explicit transaction_dispatch_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Conservative transaction-wide failure-containment policy. */
enum class transaction_failure_containment {
  stop_after_terminal_failure,
};

/*! \brief Explicit bounded parallelism for immutable dispatch selection. */
class transaction_dispatch_policy final {
public:
  [[nodiscard]] static transaction_dispatch_policy make(
      std::size_t construction_capacity,
      std::size_t check_capacity,
      transaction_failure_containment failure_containment =
          transaction_failure_containment::stop_after_terminal_failure);

  [[nodiscard]] std::size_t construction_capacity() const noexcept;
  [[nodiscard]] std::size_t check_capacity() const noexcept;
  [[nodiscard]] std::size_t operation_capacity() const noexcept;
  [[nodiscard]] transaction_failure_containment
  failure_containment() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend class transaction_run_journal_record;

  [[nodiscard]] static transaction_dispatch_policy restore(
      std::size_t construction_capacity,
      std::size_t check_capacity,
      transaction_failure_containment failure_containment,
      const session_identity& expected_identity);

  transaction_dispatch_policy(
      std::size_t construction_capacity,
      std::size_t check_capacity,
      transaction_failure_containment failure_containment,
      session_identity identity);

  std::size_t construction_capacity_;
  std::size_t check_capacity_;
  transaction_failure_containment failure_containment_;
  session_identity identity_;
};

/*! \brief Exact terminal predecessor evidence retained at reservation time. */
class transaction_dispatch_dependency final {
public:
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  node() const noexcept;
  [[nodiscard]] const session_identity& evidence() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

  friend bool operator==(const transaction_dispatch_dependency&,
                         const transaction_dispatch_dependency&) noexcept;
  friend bool operator!=(const transaction_dispatch_dependency&,
                         const transaction_dispatch_dependency&) noexcept;

private:
  friend struct detail_dispatch_access;
  friend class transaction_run_journal_record;

  [[nodiscard]] static transaction_dispatch_dependency restore(
      pkgtransaction::transaction_node_identity node,
      session_identity evidence,
      const session_identity& expected_identity);

  transaction_dispatch_dependency(
      pkgtransaction::transaction_node_identity node,
      session_identity evidence,
      session_identity identity);

  pkgtransaction::transaction_node_identity node_;
  session_identity evidence_;
  session_identity identity_;
};

/*! \brief One exact ready unit reserved to one caller-issued attempt. */
class transaction_dispatch final {
public:
  [[nodiscard]] const ready_transaction_unit& unit() const noexcept;
  [[nodiscard]] const transaction_dispatch_nonce& nonce() const noexcept;
  [[nodiscard]] const session_identity& reserved_from_progress() const noexcept;
  [[nodiscard]] const pkgstate::installed_state_snapshot_identity&
  reserved_state() const noexcept;
  [[nodiscard]] const std::vector<transaction_dispatch_dependency>&
  dependencies() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend struct detail_dispatch_access;
  friend class transaction_run_journal_record;

  [[nodiscard]] static transaction_dispatch restore(
      ready_transaction_unit unit,
      transaction_dispatch_nonce nonce,
      session_identity reserved_from_progress,
      pkgstate::installed_state_snapshot_identity reserved_state,
      std::vector<transaction_dispatch_dependency> dependencies,
      const session_identity& expected_identity);

  transaction_dispatch(
      ready_transaction_unit unit,
      transaction_dispatch_nonce nonce,
      session_identity reserved_from_progress,
      pkgstate::installed_state_snapshot_identity reserved_state,
      std::vector<transaction_dispatch_dependency> dependencies,
      session_identity identity);

  ready_transaction_unit unit_;
  transaction_dispatch_nonce nonce_;
  session_identity reserved_from_progress_;
  pkgstate::installed_state_snapshot_identity reserved_state_;
  std::vector<transaction_dispatch_dependency> dependencies_;
  session_identity identity_;
};

/*! \brief Lifecycle of one immutable dispatch-ledger record. */
enum class transaction_dispatch_state {
  reserved,
  started,
  completed,
  released_unstarted,
};

/*! \brief Reservation, attempt, uncertainty, and terminal evidence ledger. */
class transaction_dispatch_record final {
public:
  [[nodiscard]] const transaction_dispatch& dispatch() const noexcept;
  [[nodiscard]] transaction_dispatch_state state() const noexcept;
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] const std::optional<session_identity>&
  attempt_session() const noexcept;
  /*! \brief Exact durable effect-attempt history for an operation. */
  [[nodiscard]] const std::optional<session_identity>&
  effect_attempt() const noexcept;
  [[nodiscard]] const std::vector<session_identity>&
  observations() const noexcept;
  [[nodiscard]] const std::optional<session_identity>&
  terminal_evidence() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend struct detail_dispatch_access;
  friend class transaction_run_journal_record;

  [[nodiscard]] static transaction_dispatch_record restore(
      transaction_dispatch dispatch,
      transaction_dispatch_state state,
      std::optional<session_identity> attempt_session,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations,
      std::optional<session_identity> terminal_evidence,
      const session_identity& expected_identity);

  transaction_dispatch_record(
      transaction_dispatch dispatch,
      transaction_dispatch_state state,
      std::optional<session_identity> attempt_session,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations,
      std::optional<session_identity> terminal_evidence,
      session_identity identity);

  transaction_dispatch dispatch_;
  transaction_dispatch_state state_;
  std::optional<session_identity> attempt_session_;
  std::optional<session_identity> effect_attempt_;
  std::vector<session_identity> observations_;
  std::optional<session_identity> terminal_evidence_;
  session_identity identity_;
};

/*! \brief Immutable progression plus all dispatch ownership records. */
class transaction_run final {
public:
  [[nodiscard]] static transaction_run begin(
      transaction_progress progress,
      transaction_dispatch_policy policy);

  [[nodiscard]] const transaction_progress& progress() const noexcept;
  [[nodiscard]] const transaction_dispatch_policy& policy() const noexcept;
  [[nodiscard]] const std::vector<transaction_dispatch_record>&
  records() const noexcept;
  [[nodiscard]] const transaction_dispatch_record* record(
      const session_identity& dispatch) const noexcept;
  [[nodiscard]] std::size_t active_count(transaction_unit_kind kind) const noexcept;
  [[nodiscard]] bool stopped() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend struct detail_dispatch_access;
  friend class transaction_run_journal_record;

  [[nodiscard]] static transaction_run restore(
      transaction_progress progress,
      transaction_dispatch_policy policy,
      std::vector<transaction_dispatch_record> records,
      const session_identity& expected_identity);

  transaction_run(
      transaction_progress progress,
      transaction_dispatch_policy policy,
      std::vector<transaction_dispatch_record> records,
      session_identity identity);

  transaction_progress progress_;
  transaction_dispatch_policy policy_;
  std::vector<transaction_dispatch_record> records_;
  session_identity identity_;
};

/*! \brief Result of one deterministic reservation attempt. */
struct transaction_dispatch_result final {
  transaction_run run;
  std::optional<transaction_dispatch> dispatch;
};

/*! \brief Pure durable-operation start authority before either append. */
struct operation_dispatch_start_result final {
  transaction_run run;
  effect_attempt_record effect_attempt;
};

/*! \brief Reserve the first canonical ready unit allowed by policy. */
[[nodiscard]] transaction_dispatch_result reserve_next(
    transaction_run run,
    transaction_dispatch_nonce nonce);

/*! \brief Bind one reserved construction dispatch to its admitted session. */
[[nodiscard]] transaction_run start_construction_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const construction_session& session);

/*! \brief Bind one reserved check dispatch to its admitted session. */
[[nodiscard]] transaction_run start_check_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const transaction_check_session& session);

/*! \brief Bind an operation and derive its exact durable effect attempt.
 *
 * The returned effect-attempt admission must be committed first, followed by
 * the successor run snapshot, before any effect driver is invoked.
 */
[[nodiscard]] operation_dispatch_start_result start_operation_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session,
    effect_attempt_nonce nonce);

/*! \brief Release a reservation that has not acquired execution authority. */
[[nodiscard]] transaction_run release_unstarted_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch);

/*! \brief Retire one started construction dispatch with terminal evidence. */
[[nodiscard]] transaction_run complete_construction_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_result construction);

/*! \brief Retire one started check dispatch with terminal evidence. */
[[nodiscard]] transaction_run complete_check_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_result check);

/*! \brief Record one operation result and retire it only when authoritative.
 *
 * Lost-lease and indeterminate-publication results remain active observations.
 * A later authoritative result for the same exact operation session may retire
 * the dispatch through this function.
 */
[[nodiscard]] transaction_run submit_operation_dispatch_result(
    transaction_run run,
    const transaction_dispatch& dispatch,
    effectful_operation_result effect,
    std::optional<pkgstate::snapshot> resulting_state = std::nullopt);

} // namespace pkgctl
