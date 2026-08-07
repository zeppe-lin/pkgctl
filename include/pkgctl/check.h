// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file check.h
 *  \brief Exact transaction-check requests, sessions, and results.
 */
#pragma once

#include <vector>

#include <libpkgcheck-exec/libpkgcheck-exec.h>

#include <pkgctl/construction.h>

namespace pkgctl {

struct detail_run_recovery_access;

class transaction_progress;

/*! \brief Pure controller authority for one ready transaction check node. */
class transaction_check_request final {
public:
  [[nodiscard]] static transaction_check_request make(
      const transaction_progress& progression,
      pkgtransaction::transaction_node_identity check_node);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const session_identity& prepared_from_progress() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  check_node() const noexcept;
  [[nodiscard]] const construction_result& construction() const noexcept;
  [[nodiscard]] const pkgcheck::check_request& check() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  transaction_check_request(
      transaction_session transaction,
      session_identity prepared_from_progress,
      pkgtransaction::transaction_node_identity check_node,
      construction_result construction,
      pkgcheck::check_request check,
      session_identity identity);

  transaction_session transaction_;
  session_identity prepared_from_progress_;
  pkgtransaction::transaction_node_identity check_node_;
  construction_result construction_;
  pkgcheck::check_request check_;
  session_identity identity_;
};

/*! \brief Concrete call-scoped resources for one transaction check request. */
struct transaction_check_resources final {
  pkgcheck_exec::source_tree source;
  pkgcheck_exec::checked_package_tree package;
  std::vector<pkgcheck_exec::package_input_resource> inputs;
  pkgcheck_exec::session_paths paths;
  pkgcheck_exec::execution_identity execution_identity;
  pkgexec::resource_limits limits = pkgexec::resource_limits::make();
};

/*! \brief One controller check request bound to exact execution resources. */
class transaction_check_session final {
public:
  [[nodiscard]] static transaction_check_session admit(
      transaction_check_request request,
      transaction_check_resources resources);

  [[nodiscard]] const transaction_check_request& request() const noexcept;
  [[nodiscard]] const pkgcheck_exec::admitted_check_session&
  execution_session() const noexcept;
  [[nodiscard]] const pkgexec::execution_request_identity&
  execution_request() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  transaction_check_session(
      transaction_check_request request,
      pkgcheck_exec::admitted_check_session execution_session,
      pkgexec::execution_request_identity execution_request,
      session_identity identity);

  transaction_check_request request_;
  pkgcheck_exec::admitted_check_session execution_session_;
  pkgexec::execution_request_identity execution_request_;
  session_identity identity_;
};

/*! \brief Execution authority borrowed by the check controller. */
class transaction_check_driver {
public:
  virtual ~transaction_check_driver() = default;

  [[nodiscard]] virtual pkgcheck_exec::check_execution_result execute_check(
      const pkgcheck_exec::admitted_check_session& session) = 0;
};

/*! \brief Native composition of libpkgcheck-exec and libpkgexec.
 *
 * The driver resets the admitted call-scoped temporary host resource and
 * prepares its private home directory before execution. It does not populate
 * or mutate the caller-owned execution root view.
 */
class native_transaction_check_driver final : public transaction_check_driver {
public:
  explicit native_transaction_check_driver(pkgexec::execution_backend& backend);

  [[nodiscard]] pkgcheck_exec::check_execution_result execute_check(
      const pkgcheck_exec::admitted_check_session& session) override;

private:
  pkgexec::execution_backend& backend_;
};

/*! \brief Terminal execution evidence for one controller check session. */
class transaction_check_result final {
public:
  [[nodiscard]] pkgcheck::check_outcome outcome() const noexcept;
  [[nodiscard]] bool succeeded() const noexcept;
  [[nodiscard]] const transaction_check_session& session() const noexcept;
  [[nodiscard]] const pkgcheck_exec::check_execution_result&
  execution() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend struct detail_run_recovery_access;
  friend transaction_check_result execute_transaction_check(
      transaction_check_session, transaction_check_driver&);

  transaction_check_result(
      transaction_check_session session,
      pkgcheck_exec::check_execution_result execution,
      session_identity identity);

  transaction_check_session session_;
  pkgcheck_exec::check_execution_result execution_;
  session_identity identity_;
};

/*! \brief Execute one exact admitted transaction check session. */
[[nodiscard]] transaction_check_result execute_transaction_check(
    transaction_check_session session,
    transaction_check_driver& driver);

} // namespace pkgctl
