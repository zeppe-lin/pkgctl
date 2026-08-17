// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_operation.h
 *  \brief Exact native operation sessions and restart authority.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgimage/archive_backend.h>

#include <pkgctl/run_native.h>
#include <pkgctl/run_recovery.h>

namespace pkgctl {

/*! \brief Structured failure owned by native operation authority. */
enum class native_operation_authority_error_code : std::uint8_t {
  invalid_configuration = 1,
  transaction_mismatch = 2,
  operation_dispatch_required = 3,
  specification_missing = 4,
  construction_missing = 5,
  construction_ambiguous = 6,
  planning_refused = 7,
  lifecycle_node_missing = 8,
  session_invalid = 9,
  effect_attempt_missing = 10,
  effect_attempt_mismatch = 11,
  restart_body_invalid = 12,
};

/*! \brief Invalid descriptor or returned authority in operation assembly. */
class native_operation_authority_error final : public std::runtime_error {
public:
  native_operation_authority_error(
      native_operation_authority_error_code code,
      std::string message);

  [[nodiscard]] native_operation_authority_error_code code() const noexcept;

private:
  native_operation_authority_error_code code_;
};

/*! \brief Exact per-dispatch operation-local authority for one action node. */
class native_transaction_operation_specification final {
public:
  [[nodiscard]] static native_transaction_operation_specification install(
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      pkgplan::runtime_dependency_closure_identity runtime_closure,
      lifecycle_order lifecycle,
      pkgstate::installation_reason installation_reason);

  [[nodiscard]] static native_transaction_operation_specification upgrade(
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      pkgplan::runtime_dependency_closure_identity runtime_closure,
      lifecycle_order lifecycle);

  [[nodiscard]] static native_transaction_operation_specification remove(
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      lifecycle_order lifecycle);

  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  action_node() const noexcept;
  [[nodiscard]] const pkgapply::application_target_context&
  target() const noexcept;
  [[nodiscard]] const pkgapply::application_execution_control&
  control() const noexcept;
  [[nodiscard]] const pkgplan::target_observation_set&
  observations() const noexcept;
  [[nodiscard]] const std::optional<
      pkgplan::runtime_dependency_closure_identity>&
  runtime_closure() const noexcept;
  [[nodiscard]] const lifecycle_order& lifecycle() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::installation_reason>&
  installation_reason() const noexcept;

private:
  native_transaction_operation_specification(
      pkgplan::operation_kind kind,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::application_target_context target,
      pkgapply::application_execution_control control,
      pkgplan::target_observation_set observations,
      std::optional<pkgplan::runtime_dependency_closure_identity>
          runtime_closure,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason);

  pkgplan::operation_kind kind_;
  pkgtransaction::transaction_node_identity action_node_;
  pkgapply::application_target_context target_;
  pkgapply::application_execution_control control_;
  pkgplan::target_observation_set observations_;
  std::optional<pkgplan::runtime_dependency_closure_identity> runtime_closure_;
  lifecycle_order lifecycle_;
  std::optional<pkgstate::installation_reason> installation_reason_;
};

/*! \brief Fixed native lifecycle coordinates and credentials. */
struct native_transaction_lifecycle_configuration final {
  pkgexec::root_view_identity execution_root;
  std::filesystem::path execution_root_path;
  std::filesystem::path target_root_path;
  std::filesystem::path session_root;
  pkgapply_exec::lifecycle_execution_identity execution_identity;
};

/*! \brief Fixed transaction, package-policy, and lifecycle authority. */
class native_transaction_operation_configuration final {
public:
  [[nodiscard]] static native_transaction_operation_configuration make(
      transaction_session transaction,
      pkgplan::package_policy_snapshot policy,
      native_transaction_lifecycle_configuration lifecycle);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgplan::package_policy_snapshot& policy() const noexcept;
  [[nodiscard]] const native_transaction_lifecycle_configuration&
  lifecycle() const noexcept;

private:
  native_transaction_operation_configuration(
      transaction_session transaction,
      pkgplan::package_policy_snapshot policy,
      native_transaction_lifecycle_configuration lifecycle);

  transaction_session transaction_;
  pkgplan::package_policy_snapshot policy_;
  native_transaction_lifecycle_configuration lifecycle_;
};

/*! \brief Fresh-dispatch planning/application authority for one operation. */
class transaction_operation_specification_source {
public:
  virtual ~transaction_operation_specification_source() = default;

  [[nodiscard]] virtual native_transaction_operation_specification operation(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) = 0;
};

using operation_session_encoding = std::vector<std::uint8_t>;

/*! \brief Durable opaque store for one controller-owned operation session.
 *
 * The native operation authority encodes the exact admitted specification and
 * lifecycle context before an effect attempt may name the session. Restart
 * retrieves those owner bytes by session identity; the store never rebuilds
 * semantic authority from the current target or command configuration.
 */
class transaction_operation_session_store {
public:
  virtual ~transaction_operation_session_store() = default;

  virtual void retain(
      const effectful_operation_session& session,
      operation_session_encoding encoding) = 0;

  [[nodiscard]] virtual std::optional<operation_session_encoding> load(
      const session_identity& session) = 0;
};

/*! \brief Exact subordinate values retained outside the effect journal. */
struct transaction_effect_restart_bodies final {
  std::vector<pkgapply_exec::lifecycle_execution_result> before;
  std::optional<pkgapply::application_receipt> application;
  std::vector<pkgapply_exec::lifecycle_execution_result> after;
  std::optional<pkgstate::state_publication_request> publication_request;
  std::optional<pkgstate::state_publication_receipt> publication_receipt;
  /*! \brief Restart authority only for an unresolved application intent. */
  std::optional<pkgapply::application_journal_record> application_journal;
};

/*! \brief Owner source for exact subordinate restart bodies. */
class transaction_effect_restart_body_source {
public:
  virtual ~transaction_effect_restart_body_source() = default;

  [[nodiscard]] virtual transaction_effect_restart_bodies load(
      const effectful_operation_session& session,
      const effect_attempt_record& record) = 0;
};

/*! \brief Native fresh and restart operation authority for one transaction. */
class native_transaction_operation_authority_source final
    : public transaction_operation_execution_authority_source,
      public transaction_operation_recovery_authority_source {
public:
  native_transaction_operation_authority_source(
      native_transaction_operation_configuration configuration,
      transaction_operation_specification_source& specifications,
      effect_journal_store& effects,
      transaction_effect_restart_body_source& bodies,
      transaction_operation_session_store* sessions);

  [[nodiscard]] operation_dispatch_execution_authority operation(
      const transaction_run_journal_record& record,
      const transaction_run& run,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) override;

  /*! \brief Rehydrate one exact terminal operation from retained authority. */
  [[nodiscard]] effect_restart_checkpoint rehydrate(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch,
      const effect_attempt_record& evidence);

private:
  [[nodiscard]] effectful_operation_session fresh_session(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) const;

  [[nodiscard]] effect_restart_checkpoint checkpoint(
      effectful_operation_session session,
      const effect_attempt_record& record);

  native_transaction_operation_configuration configuration_;
  transaction_operation_specification_source& specifications_;
  effect_journal_store& effects_;
  transaction_effect_restart_body_source& bodies_;
  transaction_operation_session_store* sessions_;
};

/*! \brief Caller-retained pathname for one exact incoming authority. */
struct retained_transaction_effect_archive final {
  pkgapply::incoming_package_authority_identity incoming;
  std::filesystem::path path;
};

/*! \brief Explicit archive map over one caller-selected image backend. */
class explicit_transaction_effect_archive_source final
    : public transaction_effect_archive_source {
public:
  [[nodiscard]] static explicit_transaction_effect_archive_source make(
      pkgimage::archive_backend& backend,
      std::vector<retained_transaction_effect_archive> archives);

  [[nodiscard]] std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority& incoming) override;

private:
  explicit_transaction_effect_archive_source(
      pkgimage::archive_backend& backend,
      std::vector<retained_transaction_effect_archive> archives);

  pkgimage::archive_backend& backend_;
  std::vector<retained_transaction_effect_archive> archives_;
};

} // namespace pkgctl
