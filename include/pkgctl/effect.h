// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file effect.h
 *  \brief Exact single-operation transaction effect sessions.
 */
#pragma once

#include <optional>
#include <vector>

#include <libpkgapply-exec/libpkgapply-exec.h>
#include <libpkgapply/apply.h>
#include <libpkgstate-apply/adapter.h>
#include <libpkgstate/canonical_store.h>

#include <pkgctl/identity.h>
#include <pkgctl/session.h>

namespace pkgctl {

/*! \brief Caller-selected lifecycle order around one target action node. */
class lifecycle_order final {
public:
  [[nodiscard]] static lifecycle_order make(
      std::vector<pkgtransaction::transaction_node_identity> before,
      std::vector<pkgtransaction::transaction_node_identity> after);

  [[nodiscard]] const std::vector<pkgtransaction::transaction_node_identity>&
  before() const noexcept;
  [[nodiscard]] const std::vector<pkgtransaction::transaction_node_identity>&
  after() const noexcept;

private:
  lifecycle_order(
      std::vector<pkgtransaction::transaction_node_identity> before,
      std::vector<pkgtransaction::transaction_node_identity> after);

  std::vector<pkgtransaction::transaction_node_identity> before_;
  std::vector<pkgtransaction::transaction_node_identity> after_;
};

/*! \brief Semantic request to realize one exact target operation node. */
class effectful_operation_request final {
public:
  [[nodiscard]] static effectful_operation_request make(
      transaction_session transaction,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::package_application_request application,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason =
          std::nullopt);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  action_node() const noexcept;
  [[nodiscard]] const pkgapply::package_application_request&
  application() const noexcept;
  [[nodiscard]] const lifecycle_order& lifecycle() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::installation_reason>&
  installation_reason() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  effectful_operation_request(
      transaction_session transaction,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::package_application_request application,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason,
      session_identity identity);

  transaction_session transaction_;
  pkgtransaction::transaction_node_identity action_node_;
  pkgapply::package_application_request application_;
  lifecycle_order lifecycle_;
  std::optional<pkgstate::installation_reason> installation_reason_;
  session_identity identity_;
};

/*! \brief One admitted request plus exact executable lifecycle resources. */
class effectful_operation_session final {
public:
  [[nodiscard]] static effectful_operation_session admit(
      effectful_operation_request request,
      std::vector<pkgapply_exec::admitted_lifecycle_session> before,
      std::vector<pkgapply_exec::admitted_lifecycle_session> after);

  [[nodiscard]] const effectful_operation_request& request() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::admitted_lifecycle_session>&
  before() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::admitted_lifecycle_session>&
  after() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  effectful_operation_session(
      effectful_operation_request request,
      std::vector<pkgapply_exec::admitted_lifecycle_session> before,
      std::vector<pkgapply_exec::admitted_lifecycle_session> after,
      session_identity identity);

  effectful_operation_request request_;
  std::vector<pkgapply_exec::admitted_lifecycle_session> before_;
  std::vector<pkgapply_exec::admitted_lifecycle_session> after_;
  session_identity identity_;
};

/*! \brief Physical authority surface borrowed by the controller sequence. */
class transaction_effect_driver {
public:
  virtual ~transaction_effect_driver() = default;

  [[nodiscard]] virtual pkgapply::target_mutation_lease& lease() noexcept = 0;
  [[nodiscard]] virtual const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept = 0;

  [[nodiscard]] virtual pkgapply_exec::lifecycle_execution_result
  execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) = 0;

  [[nodiscard]] virtual pkgapply::application_receipt
  apply_application(const pkgapply::package_application_request& request) = 0;

  [[nodiscard]] virtual pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) = 0;
};

/*! \brief Native driver composing the exact apply, exec, and state APIs. */
class native_transaction_effect_driver final : public transaction_effect_driver {
public:
  native_transaction_effect_driver(
      const pkgapply::lease_bound_state_projection& state,
      pkgapply::target_mutation_lease& lease,
      pkgapply::application_backend& application_backend,
      const pkgimage::package_archive* incoming_archive,
      pkgexec::execution_backend& lifecycle_backend,
      pkgstate::canonical_store& state_store);

  [[nodiscard]] pkgapply::target_mutation_lease& lease() noexcept override;
  [[nodiscard]] const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept override;
  [[nodiscard]] pkgapply_exec::lifecycle_execution_result
  execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override;
  [[nodiscard]] pkgapply::application_receipt
  apply_application(const pkgapply::package_application_request& request)
      override;
  [[nodiscard]] pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) override;

private:
  const pkgapply::lease_bound_state_projection& state_;
  pkgapply::target_mutation_lease& lease_;
  pkgapply::application_backend& application_backend_;
  const pkgimage::package_archive* incoming_archive_;
  pkgexec::execution_backend& lifecycle_backend_;
  pkgstate::canonical_store& state_store_;
};

/*! \brief Terminal controller knowledge for one effectful operation attempt. */
enum class effectful_operation_outcome {
  lifecycle_failed_before_application,
  application_not_completed,
  lifecycle_failed_after_application,
  outer_lease_lost,
  state_publication_not_completed,
  state_publication_indeterminate,
  completed,
};

class effectful_operation_result final {
public:
  [[nodiscard]] effectful_operation_outcome outcome() const noexcept;
  [[nodiscard]] bool succeeded() const noexcept;
  [[nodiscard]] const effectful_operation_session& session() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::lifecycle_execution_result>&
  before() const noexcept;
  [[nodiscard]] const std::optional<pkgapply::application_receipt>&
  application() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::lifecycle_execution_result>&
  after() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::transaction_evidence_identity>&
  transaction_evidence() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::state_publication_request>&
  publication_request() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::state_publication_receipt>&
  publication_receipt() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend effectful_operation_result execute_effectful_operation(
      effectful_operation_session, transaction_effect_driver&);

  [[nodiscard]] static effectful_operation_result seal(
      effectful_operation_session session,
      effectful_operation_outcome outcome,
      std::vector<pkgapply_exec::lifecycle_execution_result> before,
      std::optional<pkgapply::application_receipt> application,
      std::vector<pkgapply_exec::lifecycle_execution_result> after,
      std::optional<pkgstate::transaction_evidence_identity>
          transaction_evidence,
      std::optional<pkgstate::state_publication_request> publication_request,
      std::optional<pkgstate::state_publication_receipt> publication_receipt);

  effectful_operation_result(
      effectful_operation_session session,
      effectful_operation_outcome outcome,
      std::vector<pkgapply_exec::lifecycle_execution_result> before,
      std::optional<pkgapply::application_receipt> application,
      std::vector<pkgapply_exec::lifecycle_execution_result> after,
      std::optional<pkgstate::transaction_evidence_identity>
          transaction_evidence,
      std::optional<pkgstate::state_publication_request> publication_request,
      std::optional<pkgstate::state_publication_receipt> publication_receipt,
      session_identity identity);

  effectful_operation_session session_;
  effectful_operation_outcome outcome_;
  std::vector<pkgapply_exec::lifecycle_execution_result> before_;
  std::optional<pkgapply::application_receipt> application_;
  std::vector<pkgapply_exec::lifecycle_execution_result> after_;
  std::optional<pkgstate::transaction_evidence_identity> transaction_evidence_;
  std::optional<pkgstate::state_publication_request> publication_request_;
  std::optional<pkgstate::state_publication_receipt> publication_receipt_;
  session_identity identity_;
};

/*! \brief Execute one exact operation under one caller-held target lease. */
[[nodiscard]] effectful_operation_result execute_effectful_operation(
    effectful_operation_session session,
    transaction_effect_driver& driver);

} // namespace pkgctl
