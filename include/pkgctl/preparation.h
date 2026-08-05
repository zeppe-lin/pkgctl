// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file preparation.h
 *  \brief One exact transaction action prepared for the effect kernel.
 */
#pragma once

#include <optional>
#include <variant>

#include <libpkgapply/request.h>
#include <libpkgbuild-plan/adapter.h>
#include <libpkgplan/libpkgplan.h>
#include <libpkgstate-plan/adapter.h>

#include <pkgctl/progression.h>

namespace pkgctl {

/*! \brief Complete caller authority for one package-local operation attempt. */
class operation_preparation_request final {
public:
  [[nodiscard]] static operation_preparation_request install(
      transaction_progress progression,
      pkgtransaction::transaction_node_identity action_node,
      construction_result construction,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      pkgplan::runtime_dependency_closure_identity runtime_closure,
      pkgplan::package_policy_snapshot policy,
      lifecycle_order lifecycle,
      pkgstate::installation_reason installation_reason);

  [[nodiscard]] static operation_preparation_request upgrade(
      transaction_progress progression,
      pkgtransaction::transaction_node_identity action_node,
      construction_result construction,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      pkgplan::runtime_dependency_closure_identity runtime_closure,
      pkgplan::package_policy_snapshot policy,
      lifecycle_order lifecycle);

  [[nodiscard]] static operation_preparation_request remove(
      transaction_progress progression,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      pkgplan::package_policy_snapshot policy,
      lifecycle_order lifecycle);

  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const transaction_progress& progression() const noexcept;
  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgstate::snapshot& current_state() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  action_node() const noexcept;
  [[nodiscard]] const std::optional<construction_result>&
  construction() const noexcept;
  [[nodiscard]] const pkgapply::application_target_context&
  target() const noexcept;
  [[nodiscard]] const pkgapply::application_execution_control&
  control() const noexcept;
  [[nodiscard]] const pkgplan::target_observation_set&
  observations() const noexcept;
  [[nodiscard]] const std::optional<
      pkgplan::runtime_dependency_closure_identity>&
  runtime_closure() const noexcept;
  [[nodiscard]] const pkgplan::package_policy_snapshot& policy() const noexcept;
  [[nodiscard]] const lifecycle_order& lifecycle() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::installation_reason>&
  installation_reason() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  operation_preparation_request(
      pkgplan::operation_kind kind,
      transaction_progress progression,
      pkgtransaction::transaction_node_identity action_node,
      std::optional<construction_result> construction,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      std::optional<pkgplan::runtime_dependency_closure_identity>
          runtime_closure,
      pkgplan::package_policy_snapshot policy,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason,
      session_identity identity);

  pkgplan::operation_kind kind_;
  transaction_progress progression_;
  pkgtransaction::transaction_node_identity action_node_;
  std::optional<construction_result> construction_;
  pkgapply::application_target_context target_;
  pkgapply::application_execution_control control_;
  pkgplan::target_observation_set observations_;
  std::optional<pkgplan::runtime_dependency_closure_identity> runtime_closure_;
  pkgplan::package_policy_snapshot policy_;
  lifecycle_order lifecycle_;
  std::optional<pkgstate::installation_reason> installation_reason_;
  session_identity identity_;
};

/*! \brief Read-only artifact projection authority used during preparation. */
class operation_preparation_driver {
public:
  virtual ~operation_preparation_driver() = default;

  [[nodiscard]] virtual pkgbuild::plan_adapter::artifact_projection
  project_artifact(const construction_result& construction) = 0;
};

/*! \brief Native pure projection through libpkgbuild-plan. */
class native_operation_preparation_driver final
    : public operation_preparation_driver {
public:
  native_operation_preparation_driver() = default;

  [[nodiscard]] pkgbuild::plan_adapter::artifact_projection
  project_artifact(const construction_result& construction) override;

};

/*! \brief Closed operation-specific planning refusal. */
using operation_planning_refusal_body = std::variant<
    pkgplan::installation_refusal,
    pkgplan::upgrade_refusal,
    pkgplan::removal_refusal>;

/*! \brief Common immutable envelope for an official libpkgplan refusal. */
class operation_planning_refusal final {
public:
  explicit operation_planning_refusal(pkgplan::installation_refusal refusal);
  explicit operation_planning_refusal(pkgplan::upgrade_refusal refusal);
  explicit operation_planning_refusal(pkgplan::removal_refusal refusal);

  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] pkgplan::planning_refusal_code code() const noexcept;
  [[nodiscard]] const pkgplan::planning_refusal_identity&
  identity() const noexcept;
  [[nodiscard]] const operation_planning_refusal_body& body() const noexcept;
  [[nodiscard]] const pkgplan::installation_refusal*
  installation() const noexcept;
  [[nodiscard]] const pkgplan::upgrade_refusal* upgrade() const noexcept;
  [[nodiscard]] const pkgplan::removal_refusal* removal() const noexcept;

private:
  operation_planning_refusal_body body_;
};

enum class operation_preparation_outcome {
  planning_refused,
  prepared,
};

/*! \brief Complete projections and terminal outcome of one preparation call. */
class operation_preparation_result final {
public:
  [[nodiscard]] operation_preparation_outcome outcome() const noexcept;
  [[nodiscard]] bool prepared() const noexcept;
  [[nodiscard]] const operation_preparation_request& request() const noexcept;
  [[nodiscard]] const pkgstate::plan_adapter::installed_state_projection&
  installed_state() const noexcept;
  [[nodiscard]] const std::optional<
      pkgbuild::plan_adapter::artifact_projection>&
  artifact() const noexcept;
  [[nodiscard]] const std::optional<pkgapply::incoming_package_authority>&
  incoming() const noexcept;
  [[nodiscard]] const std::optional<operation_planning_refusal>&
  refusal() const noexcept;
  [[nodiscard]] const std::optional<pkgplan::package_operation_plan>&
  plan() const noexcept;
  [[nodiscard]] const std::optional<pkgapply::package_application_request>&
  application() const noexcept;
  [[nodiscard]] const std::optional<effectful_operation_request>&
  effect() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend operation_preparation_result prepare_operation(
      operation_preparation_request, operation_preparation_driver&);

  [[nodiscard]] static operation_preparation_result refused(
      operation_preparation_request request,
      pkgstate::plan_adapter::installed_state_projection installed_state,
      std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
      std::optional<pkgapply::incoming_package_authority> incoming,
      operation_planning_refusal refusal);

  [[nodiscard]] static operation_preparation_result success(
      operation_preparation_request request,
      pkgstate::plan_adapter::installed_state_projection installed_state,
      std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
      std::optional<pkgapply::incoming_package_authority> incoming,
      pkgplan::package_operation_plan plan,
      pkgapply::package_application_request application,
      effectful_operation_request effect);

  operation_preparation_result(
      operation_preparation_request request,
      pkgstate::plan_adapter::installed_state_projection installed_state,
      std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
      std::optional<pkgapply::incoming_package_authority> incoming,
      std::optional<operation_planning_refusal> refusal,
      std::optional<pkgplan::package_operation_plan> plan,
      std::optional<pkgapply::package_application_request> application,
      std::optional<effectful_operation_request> effect,
      operation_preparation_outcome outcome,
      session_identity identity);

  operation_preparation_request request_;
  pkgstate::plan_adapter::installed_state_projection installed_state_;
  std::optional<pkgbuild::plan_adapter::artifact_projection> artifact_;
  std::optional<pkgapply::incoming_package_authority> incoming_;
  std::optional<operation_planning_refusal> refusal_;
  std::optional<pkgplan::package_operation_plan> plan_;
  std::optional<pkgapply::package_application_request> application_;
  std::optional<effectful_operation_request> effect_;
  operation_preparation_outcome outcome_;
  session_identity identity_;
};

/*! \brief Project, plan, and seal one exact effectful operation request. */
[[nodiscard]] operation_preparation_result prepare_operation(
    operation_preparation_request request,
    operation_preparation_driver& driver);

} // namespace pkgctl
