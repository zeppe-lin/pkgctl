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
#include <libpkgapply/journal.h>
#include <libpkgapply/restart.h>
#include <libpkgstate-apply/adapter.h>
#include <libpkgstate/canonical_store.h>

#include <pkgctl/identity.h>
#include <pkgctl/session.h>

namespace pkgctl {

class effect_attempt_nonce;
class effect_journal_store;
class effect_restart_checkpoint;
class effect_restart_result;

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
      pkgstate::snapshot expected_state,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::package_application_request application,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason =
          std::nullopt);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgstate::snapshot& expected_state() const noexcept;
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
      pkgstate::snapshot expected_state,
      pkgtransaction::transaction_node_identity action_node,
      pkgapply::package_application_request application,
      lifecycle_order lifecycle,
      std::optional<pkgstate::installation_reason> installation_reason,
      session_identity identity);

  transaction_session transaction_;
  pkgstate::snapshot expected_state_;
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

/*! \brief Durable owner for subordinate effect bodies named by the journal.
 *
 * The controller journal retains only exact identities and terminal facts. A
 * command that promises automatic restart supplies one sink that durably
 * retains each owner-encoded body before the journal is allowed to reference
 * it. The sink does not execute effects or choose restart policy.
 */
class transaction_effect_body_sink {
public:
  virtual ~transaction_effect_body_sink() = default;

  virtual void retain_lifecycle(
      const pkgapply_exec::lifecycle_execution_result& result) = 0;
  virtual void retain_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_receipt& receipt) = 0;
  virtual void retain_publication_request(
      const pkgstate::state_publication_request& request) = 0;
  virtual void retain_publication_receipt(
      const pkgstate::state_publication_request& request,
      const pkgstate::state_publication_receipt& receipt) = 0;
};

class transaction_effect_driver {
public:
  virtual ~transaction_effect_driver() = default;

  [[nodiscard]] virtual pkgapply::target_mutation_lease& lease() noexcept = 0;
  [[nodiscard]] virtual const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept = 0;

  /*! \brief Return the projection binding completed evidence to publication. */
  [[nodiscard]] virtual const pkgapply::lease_bound_state_projection&
  publication_state_projection() const noexcept;

  [[nodiscard]] virtual pkgapply_exec::lifecycle_execution_result
  execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) = 0;

  [[nodiscard]] virtual pkgapply::application_receipt
  apply_application(const pkgapply::package_application_request& request) = 0;

  [[nodiscard]] virtual pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) = 0;

  /*! \brief Resume one exact durable application handoff under the new lease. */
  [[nodiscard]] virtual pkgapply::application_receipt
  resume_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_journal_declaration_identity& declaration);
};

/*! \brief Read-only canonical-state authority under one target-scoped lease. */
class transaction_effect_state_observer {
public:
  virtual ~transaction_effect_state_observer() = default;

  [[nodiscard]] virtual pkgapply::target_mutation_lease& lease() noexcept = 0;
  [[nodiscard]] virtual pkgstate::snapshot read_state() const = 0;
};

/*! \brief Publication recovery authority extending exact state observation. */
class transaction_effect_publication_driver
    : public transaction_effect_state_observer {
public:
  ~transaction_effect_publication_driver() override = default;

  [[nodiscard]] virtual pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) = 0;
};

/*! \brief Native continuation driver composing apply, exec, and publication. */
class native_transaction_effect_driver final : public transaction_effect_driver {
public:
  native_transaction_effect_driver(
      const pkgapply::lease_bound_state_projection& state,
      pkgapply::target_mutation_lease& lease,
      pkgapply::application_backend& application_backend,
      pkgapply::application_journal_store& application_journal_store,
      const pkgimage::package_archive* incoming_archive,
      pkgexec::execution_backend& lifecycle_backend,
      pkgstate::canonical_store& state_store,
      const pkgapply::lease_bound_state_projection* publication_state =
          nullptr);

  [[nodiscard]] pkgapply::target_mutation_lease& lease() noexcept override;
  [[nodiscard]] const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept override;
  [[nodiscard]] const pkgapply::lease_bound_state_projection&
  publication_state_projection() const noexcept override;
  [[nodiscard]] pkgapply_exec::lifecycle_execution_result
  execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override;
  [[nodiscard]] pkgapply::application_receipt
  apply_application(const pkgapply::package_application_request& request)
      override;
  [[nodiscard]] pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) override;
  [[nodiscard]] pkgapply::application_receipt
  resume_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_journal_declaration_identity& declaration) override;

private:
  const pkgapply::lease_bound_state_projection& state_;
  pkgapply::target_mutation_lease& lease_;
  pkgapply::application_backend& application_backend_;
  pkgapply::application_journal_store& application_journal_store_;
  const pkgimage::package_archive* incoming_archive_;
  pkgexec::execution_backend& lifecycle_backend_;
  pkgstate::canonical_store& state_store_;
  const pkgapply::lease_bound_state_projection* publication_state_;
};

/*! \brief Native target-scoped canonical-state observation and publication. */
class native_transaction_effect_publication_driver final
    : public transaction_effect_publication_driver {
public:
  native_transaction_effect_publication_driver(
      pkgapply::target_mutation_lease& lease,
      pkgstate::canonical_store& state_store);

  [[nodiscard]] pkgapply::target_mutation_lease& lease() noexcept override;
  [[nodiscard]] pkgstate::snapshot read_state() const override;
  [[nodiscard]] pkgstate::state_publication_receipt
  publish_state(const pkgstate::state_publication_request& request) override;

private:
  pkgapply::target_mutation_lease& lease_;
  pkgstate::canonical_store& state_store_;
};

/*! \brief Controller knowledge for one effectful operation attempt. */
enum class effectful_operation_outcome {
  lifecycle_failed_before_application,
  application_not_completed,
  lifecycle_failed_after_application,
  outer_lease_lost,
  state_publication_not_completed,
  state_publication_indeterminate,
  completed,
  application_resolution_required,
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
  [[nodiscard]] const std::optional<pkgstate::installed_state_snapshot_identity>&
  reconciled_state() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend effectful_operation_result execute_effectful_operation(
      effectful_operation_session, transaction_effect_driver&);
  friend effectful_operation_result execute_effectful_operation_durable(
      effectful_operation_session, const effect_attempt_nonce&,
      transaction_effect_driver&, effect_journal_store&,
      transaction_effect_body_sink*);
  friend struct detail_effect_rehydration_access;
  friend effect_restart_result resume_effectful_operation(
      effect_restart_checkpoint, transaction_effect_driver*,
      transaction_effect_publication_driver*, effect_journal_store&,
      transaction_effect_body_sink*);

  [[nodiscard]] static effectful_operation_result seal(
      effectful_operation_session session,
      effectful_operation_outcome outcome,
      std::vector<pkgapply_exec::lifecycle_execution_result> before,
      std::optional<pkgapply::application_receipt> application,
      std::vector<pkgapply_exec::lifecycle_execution_result> after,
      std::optional<pkgstate::transaction_evidence_identity>
          transaction_evidence,
      std::optional<pkgstate::state_publication_request> publication_request,
      std::optional<pkgstate::state_publication_receipt> publication_receipt,
      std::optional<pkgstate::installed_state_snapshot_identity>
          reconciled_state = std::nullopt);

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
      std::optional<pkgstate::installed_state_snapshot_identity> reconciled_state,
      session_identity identity);

  effectful_operation_session session_;
  effectful_operation_outcome outcome_;
  std::vector<pkgapply_exec::lifecycle_execution_result> before_;
  std::optional<pkgapply::application_receipt> application_;
  std::vector<pkgapply_exec::lifecycle_execution_result> after_;
  std::optional<pkgstate::transaction_evidence_identity> transaction_evidence_;
  std::optional<pkgstate::state_publication_request> publication_request_;
  std::optional<pkgstate::state_publication_receipt> publication_receipt_;
  std::optional<pkgstate::installed_state_snapshot_identity> reconciled_state_;
  session_identity identity_;
};

/*! \brief Execute one exact operation under one caller-held target lease. */
[[nodiscard]] effectful_operation_result execute_effectful_operation(
    effectful_operation_session session,
    transaction_effect_driver& driver);

/*! \brief Execute one exact operation with durable intent/terminal snapshots. */
[[nodiscard]] effectful_operation_result execute_effectful_operation_durable(
    effectful_operation_session session,
    const effect_attempt_nonce& nonce,
    transaction_effect_driver& driver,
    effect_journal_store& journal_store,
    transaction_effect_body_sink* bodies = nullptr);

} // namespace pkgctl
