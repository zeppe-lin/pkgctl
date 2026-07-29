// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file progression.h
 *  \brief Immutable evidence-driven progression of one transaction program.
 */
#pragma once

#include <optional>
#include <vector>

#include <pkgctl/check.h>
#include <pkgctl/construction.h>
#include <pkgctl/effect.h>

namespace pkgctl {

/*! \brief Controller knowledge for one transaction-program node. */
enum class transaction_node_status {
  pending,
  ready,
  satisfied,
  failed,
  blocked,
};

/*! \brief Selectable realization class exposed by transaction progression. */
enum class transaction_unit_kind {
  construction,
  check,
  operation,
};

/*! \brief One graph-ready unit; operation units absorb exact lifecycle nodes. */
class ready_transaction_unit final {
public:
  [[nodiscard]] transaction_unit_kind kind() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  primary_node() const noexcept;
  [[nodiscard]] const std::vector<pkgtransaction::transaction_node_identity>&
  members() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend class transaction_progress;
  ready_transaction_unit(
      transaction_unit_kind kind,
      pkgtransaction::transaction_node_identity primary_node,
      std::vector<pkgtransaction::transaction_node_identity> members,
      session_identity identity);

  transaction_unit_kind kind_;
  pkgtransaction::transaction_node_identity primary_node_;
  std::vector<pkgtransaction::transaction_node_identity> members_;
  session_identity identity_;
};

/*! \brief Immutable current epoch and accepted terminal evidence for one graph. */
class transaction_progress final {
public:
  /*! \brief Begin at the transaction's exact resolution-state epoch. */
  [[nodiscard]] static transaction_progress begin(
      transaction_session transaction);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgstate::snapshot& current_state() const noexcept;
  [[nodiscard]] const std::vector<construction_result>&
  constructions() const noexcept;
  [[nodiscard]] const std::vector<transaction_check_result>&
  checks() const noexcept;
  [[nodiscard]] const std::vector<effectful_operation_result>&
  effects() const noexcept;

  [[nodiscard]] transaction_node_status status(
      const pkgtransaction::transaction_node_identity& node) const;
  [[nodiscard]] std::vector<pkgtransaction::transaction_node_identity>
  nodes(transaction_node_status status) const;
  [[nodiscard]] const std::vector<ready_transaction_unit>&
  ready_units() const noexcept;

  [[nodiscard]] const construction_result* construction(
      const pkgtransaction::transaction_node_identity& build_node) const noexcept;
  [[nodiscard]] const transaction_check_result* check(
      const pkgtransaction::transaction_node_identity& check_node) const noexcept;
  [[nodiscard]] const effectful_operation_result* effect(
      const pkgtransaction::transaction_node_identity& action_node) const noexcept;

  [[nodiscard]] bool complete() const noexcept;
  [[nodiscard]] bool failed() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend transaction_progress advance_construction(
      transaction_progress, construction_result);
  friend transaction_progress advance_check(
      transaction_progress, transaction_check_result);
  friend transaction_progress advance_effect(
      transaction_progress, effectful_operation_result,
      std::optional<pkgstate::snapshot>);

  struct node_record final {
    pkgtransaction::transaction_node_identity node;
    transaction_node_status status;
  };

  [[nodiscard]] static transaction_progress rebuild(
      transaction_session transaction,
      pkgstate::snapshot current_state,
      std::vector<construction_result> constructions,
      std::vector<transaction_check_result> checks,
      std::vector<effectful_operation_result> effects);

  transaction_progress(
      transaction_session transaction,
      pkgstate::snapshot current_state,
      std::vector<construction_result> constructions,
      std::vector<transaction_check_result> checks,
      std::vector<effectful_operation_result> effects,
      std::vector<node_record> nodes,
      std::vector<ready_transaction_unit> ready_units,
      session_identity identity);

  transaction_session transaction_;
  pkgstate::snapshot current_state_;
  std::vector<construction_result> constructions_;
  std::vector<transaction_check_result> checks_;
  std::vector<effectful_operation_result> effects_;
  std::vector<node_record> nodes_;
  std::vector<ready_transaction_unit> ready_units_;
  session_identity identity_;
};

/*! \brief Accept one ready build node's terminal construction evidence. */
[[nodiscard]] transaction_progress advance_construction(
    transaction_progress progress,
    construction_result construction);

/*! \brief Accept one ready check node's terminal execution evidence. */
[[nodiscard]] transaction_progress advance_check(
    transaction_progress progress,
    transaction_check_result check);

/*! \brief Accept one ready target unit's terminal effect evidence.
 *
 * A completed effect requires the exact resulting canonical snapshot. A
 * definitive failed effect requires no replacement snapshot. Indeterminate or
 * lease-lost effects are not terminal progression evidence.
 */
[[nodiscard]] transaction_progress advance_effect(
    transaction_progress progress,
    effectful_operation_result effect,
    std::optional<pkgstate::snapshot> resulting_state = std::nullopt);

} // namespace pkgctl
