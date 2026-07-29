// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/preparation.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

template<typename Left, typename Right>
bool same_string_identity(const Left& lhs, const Right& rhs)
{
  return lhs.string() == rhs.string();
}

pkgtransaction::transaction_action_kind transaction_action(
    pkgplan::operation_kind kind)
{
  switch (kind)
  {
    case pkgplan::operation_kind::install:
      return pkgtransaction::transaction_action_kind::install;
    case pkgplan::operation_kind::upgrade:
      return pkgtransaction::transaction_action_kind::upgrade;
    case pkgplan::operation_kind::remove:
      return pkgtransaction::transaction_action_kind::remove;
  }
  throw error(error_code::invalid_preparation_request,
              "operation preparation has an invalid operation kind");
}

std::vector<std::string> lifecycle_identity_fields(
    const lifecycle_order& lifecycle)
{
  std::vector<std::string> fields;
  fields.push_back(std::to_string(lifecycle.before().size()));
  for (const auto& identity : lifecycle.before())
    fields.push_back(identity.hex());
  fields.push_back(std::to_string(lifecycle.after().size()));
  for (const auto& identity : lifecycle.after())
    fields.push_back(identity.hex());
  return fields;
}

std::string installation_reason_identity(
    const pkgstate::installation_reason& reason)
{
  std::vector<std::string> fields{
      std::to_string(static_cast<unsigned int>(reason.kind()))};
  fields.push_back(reason.issuer_package()
                       ? reason.issuer_package()->name()
                       : std::string());
  fields.push_back(reason.issuer_profile()
                       ? reason.issuer_profile()->name()
                       : std::string());
  fields.push_back(reason.issuer_profile_identity()
                       ? reason.issuer_profile_identity()->string()
                       : std::string());
  fields.push_back(reason.policy().value_or(std::string()));
  return make_session_identity(
      "pkgctl/preparation-installation-reason/1", fields).hex();
}

const pkgtransaction::transaction_node& require_action(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node_identity& action_node,
    pkgplan::operation_kind kind)
{
  const auto* action = transaction.program().find(action_node);
  if (action == nullptr)
    throw error(error_code::invalid_preparation_request,
                "operation action node is absent from the transaction program");
  if (action->environment() != pkgresolve::resolution_environment::target ||
      action->action() != transaction_action(kind))
  {
    throw error(error_code::invalid_preparation_request,
                "operation node is not the requested target action");
  }
  return *action;
}

bool has_build_before_target_edge(
    const pkgtransaction::transaction_program& program,
    const pkgtransaction::transaction_node_identity& build,
    const pkgtransaction::transaction_node_identity& action)
{
  return std::any_of(
      program.edges().begin(), program.edges().end(),
      [&](const pkgtransaction::transaction_edge& edge) {
        return edge.kind() == pkgtransaction::transaction_edge_kind::phase &&
               edge.phase_order() &&
               *edge.phase_order() ==
                   pkgtransaction::phase_order_kind::build_before_target &&
               edge.before() == build && edge.after() == action;
      });
}

void validate_target_authority(
    const transaction_session& transaction,
    const pkgapply::application_target_context& target,
    const pkgplan::target_observation_set& observations)
{
  if (target.target() != observations.target())
    throw error(error_code::invalid_preparation_request,
                "target observations belong to another planner context");

  const auto& state_target = transaction.resolution().installed().target_binding();
  if (!same_string_identity(target.managed_target(),
                            state_target.managed_target()) ||
      !same_string_identity(target.root_view(), state_target.root_view()))
  {
    throw error(error_code::invalid_preparation_request,
                "application target differs from transaction installed state");
  }
}

void validate_construction_authority(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node& action,
    const pkgtransaction::transaction_node_identity& action_node,
    const construction_result& construction)
{
  if (!construction.succeeded())
    throw error(error_code::invalid_preparation_request,
                "incoming operation requires completed construction");
  if (construction.session().request().transaction().identity() !=
      transaction.identity())
  {
    throw error(error_code::invalid_preparation_request,
                "construction belongs to another transaction session");
  }

  const auto& build_node = construction.session().request().build_node();
  const auto* build = transaction.program().find(build_node);
  if (build == nullptr ||
      build->action() != pkgtransaction::transaction_action_kind::build ||
      build->package() != action.package())
  {
    throw error(error_code::invalid_preparation_request,
                "construction does not identify the action package build node");
  }

  const auto* build_selection = build->selection();
  const auto* action_selection = action.selection();
  if (build_selection == nullptr || action_selection == nullptr ||
      build_selection->identity() != action_selection->identity())
  {
    throw error(error_code::invalid_preparation_request,
                "construction and target action use different selections");
  }

  if (!has_build_before_target_edge(
          transaction.program(), build_node, action_node))
  {
    throw error(error_code::invalid_preparation_request,
                "construction build node is not ordered before the action");
  }

  if (!construction.build().artifact_inspection())
    throw error(error_code::invalid_preparation_request,
                "completed construction lacks archive inspection evidence");
}

session_identity request_identity(
    pkgplan::operation_kind kind,
    const transaction_session& transaction,
    const pkgtransaction::transaction_node_identity& action_node,
    const std::optional<construction_result>& construction,
    const pkgapply::application_target_context& target,
    const pkgapply::application_execution_control& control,
    const pkgplan::target_observation_set& observations,
    const std::optional<pkgplan::runtime_dependency_closure_identity>&
        runtime_closure,
    const pkgplan::package_policy_snapshot& policy,
    const lifecycle_order& lifecycle,
    const std::optional<pkgstate::installation_reason>& installation_reason)
{
  std::vector<std::string> fields{
      std::to_string(static_cast<unsigned int>(kind)),
      transaction.identity().hex(), action_node.hex(),
      construction ? construction->identity().hex() : std::string(),
      target.identity().string(), control.identity().string(),
      observations.identity().string(),
      runtime_closure ? runtime_closure->string() : std::string(),
      policy.identity().string()};
  auto lifecycle_fields = lifecycle_identity_fields(lifecycle);
  fields.insert(fields.end(), lifecycle_fields.begin(), lifecycle_fields.end());
  fields.push_back(installation_reason
                       ? installation_reason_identity(*installation_reason)
                       : std::string());
  return make_session_identity(
      "pkgctl/operation-preparation-request/1", fields);
}

const pkgplan::installed_package_fact& require_installed_projection(
    const pkgstate::plan_adapter::installed_state_projection& projection,
    const pkgstate::installed_package& installed)
{
  const auto found = std::find_if(
      projection.packages().begin(), projection.packages().end(),
      [&](const pkgplan::installed_package_fact& package) {
        return same_string_identity(package.identity(), installed.identity());
      });
  if (found == projection.packages().end())
    throw error(error_code::invalid_preparation_request,
                "installed-state projection omitted action authority");
  return *found;
}

void validate_artifact_projection(
    const construction_result& construction,
    const pkgbuild::plan_adapter::artifact_projection& projection)
{
  const auto& expected = construction.build().build();
  if (projection.build().identity() != expected.identity() ||
      projection.build().request().identity() != expected.request().identity() ||
      projection.candidate().source_identity() !=
          expected.request().source().identity())
  {
    throw error(error_code::preparation_driver_contract_violation,
                "preparation driver projected another build authority");
  }

  const auto& construction_receipt =
      *construction.build().artifact_inspection();
  const auto& projected_receipt = projection.image().receipt();
  if (projected_receipt.archive_digest() !=
          construction_receipt.archive_digest() ||
      projected_receipt.image_identity() !=
          construction_receipt.image_identity() ||
      projected_receipt.entry_count() != construction_receipt.entry_count())
  {
    throw error(error_code::preparation_driver_contract_violation,
                "preparation reinspection differs from construction evidence");
  }

  if (projection.artifact().release() !=
          projection.candidate().candidate().release())
  {
    throw error(error_code::preparation_driver_contract_violation,
                "preparation driver returned cross-bound artifact facts");
  }
}

pkgstate::plan_adapter::installed_state_projection project_state(
    const operation_preparation_request& request)
{
  const auto& installed = request.transaction().resolution().installed();
  return pkgstate::plan_adapter::project_installed_state(
      installed,
      pkgstate::plan_adapter::planning_target_context(
          request.target().target(), installed.target_binding()));
}

void append_artifact_projection_fields(
    std::vector<std::string>& fields,
    const std::optional<pkgbuild::plan_adapter::artifact_projection>& artifact,
    const std::optional<pkgapply::incoming_package_authority>& incoming)
{
  fields.push_back(artifact ? artifact->build().identity().hex() : std::string());
  fields.push_back(artifact
                       ? artifact->candidate().candidate().identity().string()
                       : std::string());
  fields.push_back(artifact
                       ? artifact->artifact().artifact().string()
                       : std::string());
  fields.push_back(artifact
                       ? artifact->artifact().manifest().string()
                       : std::string());
  fields.push_back(artifact
                       ? artifact->image().image().identity().string()
                       : std::string());
  fields.push_back(artifact
                       ? artifact->image().receipt().identity().string()
                       : std::string());
  fields.push_back(incoming ? incoming->identity().string() : std::string());
}

session_identity refused_identity(
    const operation_preparation_request& request,
    const pkgstate::plan_adapter::installed_state_projection& installed,
    const std::optional<pkgbuild::plan_adapter::artifact_projection>& artifact,
    const std::optional<pkgapply::incoming_package_authority>& incoming,
    const operation_planning_refusal& refusal)
{
  std::vector<std::string> fields{
      request.identity().hex(), installed.ownership().identity().string()};
  append_artifact_projection_fields(fields, artifact, incoming);
  fields.push_back(refusal.identity().string());
  fields.push_back(std::to_string(static_cast<unsigned int>(
      operation_preparation_outcome::planning_refused)));
  return make_session_identity(
      "pkgctl/operation-preparation-result/1", fields);
}

session_identity prepared_identity(
    const operation_preparation_request& request,
    const pkgstate::plan_adapter::installed_state_projection& installed,
    const std::optional<pkgbuild::plan_adapter::artifact_projection>& artifact,
    const std::optional<pkgapply::incoming_package_authority>& incoming,
    const pkgplan::package_operation_plan& plan,
    const pkgapply::package_application_request& application,
    const effectful_operation_request& effect)
{
  std::vector<std::string> fields{
      request.identity().hex(), installed.ownership().identity().string()};
  append_artifact_projection_fields(fields, artifact, incoming);
  fields.push_back(plan.identity().string());
  fields.push_back(application.identity().string());
  fields.push_back(effect.identity().hex());
  fields.push_back(std::to_string(static_cast<unsigned int>(
      operation_preparation_outcome::prepared)));
  return make_session_identity(
      "pkgctl/operation-preparation-result/1", fields);
}

} // namespace

operation_preparation_request::operation_preparation_request(
    pkgplan::operation_kind kind,
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    std::optional<construction_result> construction,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    std::optional<pkgplan::runtime_dependency_closure_identity> runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle,
    std::optional<pkgstate::installation_reason> installation_reason,
    session_identity identity)
    : kind_(kind), transaction_(std::move(transaction)),
      action_node_(std::move(action_node)),
      construction_(std::move(construction)), target_(std::move(target)),
      control_(std::move(control)), observations_(std::move(observations)),
      runtime_closure_(std::move(runtime_closure)), policy_(std::move(policy)),
      lifecycle_(std::move(lifecycle)),
      installation_reason_(std::move(installation_reason)),
      identity_(std::move(identity))
{
}

operation_preparation_request operation_preparation_request::install(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    construction_result construction,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::runtime_dependency_closure_identity runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle,
    pkgstate::installation_reason installation_reason)
{
  const auto& action = require_action(
      transaction, action_node, pkgplan::operation_kind::install);
  validate_target_authority(transaction, target, observations);
  validate_construction_authority(
      transaction, action, action_node, construction);

  std::optional<construction_result> construction_value(std::move(construction));
  std::optional<pkgplan::runtime_dependency_closure_identity>
      closure_value(std::move(runtime_closure));
  std::optional<pkgstate::installation_reason>
      reason_value(std::move(installation_reason));
  auto identity = request_identity(
      pkgplan::operation_kind::install, transaction, action_node,
      construction_value, target, control, observations, closure_value,
      policy, lifecycle, reason_value);
  return operation_preparation_request(
      pkgplan::operation_kind::install, std::move(transaction),
      std::move(action_node), std::move(construction_value),
      std::move(target), std::move(control), std::move(observations),
      std::move(closure_value), std::move(policy), std::move(lifecycle),
      std::move(reason_value), std::move(identity));
}

operation_preparation_request operation_preparation_request::upgrade(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    construction_result construction,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::runtime_dependency_closure_identity runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle)
{
  const auto& action = require_action(
      transaction, action_node, pkgplan::operation_kind::upgrade);
  validate_target_authority(transaction, target, observations);
  validate_construction_authority(
      transaction, action, action_node, construction);

  std::optional<construction_result> construction_value(std::move(construction));
  std::optional<pkgplan::runtime_dependency_closure_identity>
      closure_value(std::move(runtime_closure));
  auto identity = request_identity(
      pkgplan::operation_kind::upgrade, transaction, action_node,
      construction_value, target, control, observations, closure_value,
      policy, lifecycle, std::nullopt);
  return operation_preparation_request(
      pkgplan::operation_kind::upgrade, std::move(transaction),
      std::move(action_node), std::move(construction_value),
      std::move(target), std::move(control), std::move(observations),
      std::move(closure_value), std::move(policy), std::move(lifecycle),
      std::nullopt, std::move(identity));
}

operation_preparation_request operation_preparation_request::remove(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle)
{
  const auto& action = require_action(
      transaction, action_node, pkgplan::operation_kind::remove);
  if (action.installed() == nullptr)
    throw error(error_code::invalid_preparation_request,
                "removal action lacks installed package authority");
  validate_target_authority(transaction, target, observations);

  auto identity = request_identity(
      pkgplan::operation_kind::remove, transaction, action_node,
      std::nullopt, target, control, observations, std::nullopt,
      policy, lifecycle, std::nullopt);
  return operation_preparation_request(
      pkgplan::operation_kind::remove, std::move(transaction),
      std::move(action_node), std::nullopt, std::move(target),
      std::move(control), std::move(observations), std::nullopt,
      std::move(policy), std::move(lifecycle), std::nullopt,
      std::move(identity));
}

pkgplan::operation_kind operation_preparation_request::kind() const noexcept
{ return kind_; }
const transaction_session&
operation_preparation_request::transaction() const noexcept
{ return transaction_; }
const pkgtransaction::transaction_node_identity&
operation_preparation_request::action_node() const noexcept
{ return action_node_; }
const std::optional<construction_result>&
operation_preparation_request::construction() const noexcept
{ return construction_; }
const pkgapply::application_target_context&
operation_preparation_request::target() const noexcept { return target_; }
const pkgapply::application_execution_control&
operation_preparation_request::control() const noexcept { return control_; }
const pkgplan::target_observation_set&
operation_preparation_request::observations() const noexcept
{ return observations_; }
const std::optional<pkgplan::runtime_dependency_closure_identity>&
operation_preparation_request::runtime_closure() const noexcept
{ return runtime_closure_; }
const pkgplan::package_policy_snapshot&
operation_preparation_request::policy() const noexcept { return policy_; }
const lifecycle_order&
operation_preparation_request::lifecycle() const noexcept { return lifecycle_; }
const std::optional<pkgstate::installation_reason>&
operation_preparation_request::installation_reason() const noexcept
{ return installation_reason_; }
const session_identity&
operation_preparation_request::identity() const noexcept { return identity_; }

native_operation_preparation_driver::native_operation_preparation_driver(
    const pkgimage::archive_backend& archives)
    : archives_(archives)
{
}

pkgbuild::plan_adapter::artifact_projection
native_operation_preparation_driver::project_artifact(
    const construction_result& construction)
{
  return pkgbuild::plan_adapter::project_artifact(
      construction.build().build(),
      construction.session().paths().build.artifact_path,
      archives_);
}

operation_planning_refusal::operation_planning_refusal(
    pkgplan::installation_refusal refusal)
    : body_(std::move(refusal))
{
}
operation_planning_refusal::operation_planning_refusal(
    pkgplan::upgrade_refusal refusal)
    : body_(std::move(refusal))
{
}
operation_planning_refusal::operation_planning_refusal(
    pkgplan::removal_refusal refusal)
    : body_(std::move(refusal))
{
}

pkgplan::operation_kind operation_planning_refusal::kind() const noexcept
{
  return std::visit([](const auto& value) { return value.kind(); }, body_);
}
pkgplan::planning_refusal_code operation_planning_refusal::code() const noexcept
{
  return std::visit([](const auto& value) { return value.code(); }, body_);
}
const pkgplan::planning_refusal_identity&
operation_planning_refusal::identity() const noexcept
{
  return std::visit(
      [](const auto& value) -> const pkgplan::planning_refusal_identity& {
        return value.identity();
      },
      body_);
}
const operation_planning_refusal_body&
operation_planning_refusal::body() const noexcept { return body_; }
const pkgplan::installation_refusal*
operation_planning_refusal::installation() const noexcept
{ return std::get_if<pkgplan::installation_refusal>(&body_); }
const pkgplan::upgrade_refusal*
operation_planning_refusal::upgrade() const noexcept
{ return std::get_if<pkgplan::upgrade_refusal>(&body_); }
const pkgplan::removal_refusal*
operation_planning_refusal::removal() const noexcept
{ return std::get_if<pkgplan::removal_refusal>(&body_); }

operation_preparation_result operation_preparation_result::refused(
    operation_preparation_request request,
    pkgstate::plan_adapter::installed_state_projection installed_state,
    std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
    std::optional<pkgapply::incoming_package_authority> incoming,
    operation_planning_refusal refusal)
{
  auto identity = refused_identity(
      request, installed_state, artifact, incoming, refusal);
  return operation_preparation_result(
      std::move(request), std::move(installed_state), std::move(artifact),
      std::move(incoming), std::move(refusal), std::nullopt, std::nullopt,
      std::nullopt, operation_preparation_outcome::planning_refused,
      std::move(identity));
}

operation_preparation_result operation_preparation_result::success(
    operation_preparation_request request,
    pkgstate::plan_adapter::installed_state_projection installed_state,
    std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
    std::optional<pkgapply::incoming_package_authority> incoming,
    pkgplan::package_operation_plan plan,
    pkgapply::package_application_request application,
    effectful_operation_request effect)
{
  auto identity = prepared_identity(
      request, installed_state, artifact, incoming, plan, application, effect);
  return operation_preparation_result(
      std::move(request), std::move(installed_state), std::move(artifact),
      std::move(incoming), std::nullopt, std::move(plan),
      std::move(application), std::move(effect),
      operation_preparation_outcome::prepared, std::move(identity));
}

operation_preparation_result::operation_preparation_result(
    operation_preparation_request request,
    pkgstate::plan_adapter::installed_state_projection installed_state,
    std::optional<pkgbuild::plan_adapter::artifact_projection> artifact,
    std::optional<pkgapply::incoming_package_authority> incoming,
    std::optional<operation_planning_refusal> refusal,
    std::optional<pkgplan::package_operation_plan> plan,
    std::optional<pkgapply::package_application_request> application,
    std::optional<effectful_operation_request> effect,
    operation_preparation_outcome outcome,
    session_identity identity)
    : request_(std::move(request)),
      installed_state_(std::move(installed_state)),
      artifact_(std::move(artifact)), incoming_(std::move(incoming)),
      refusal_(std::move(refusal)), plan_(std::move(plan)),
      application_(std::move(application)), effect_(std::move(effect)),
      outcome_(outcome), identity_(std::move(identity))
{
}

operation_preparation_outcome
operation_preparation_result::outcome() const noexcept { return outcome_; }
bool operation_preparation_result::prepared() const noexcept
{ return outcome_ == operation_preparation_outcome::prepared; }
const operation_preparation_request&
operation_preparation_result::request() const noexcept { return request_; }
const pkgstate::plan_adapter::installed_state_projection&
operation_preparation_result::installed_state() const noexcept
{ return installed_state_; }
const std::optional<pkgbuild::plan_adapter::artifact_projection>&
operation_preparation_result::artifact() const noexcept { return artifact_; }
const std::optional<pkgapply::incoming_package_authority>&
operation_preparation_result::incoming() const noexcept { return incoming_; }
const std::optional<operation_planning_refusal>&
operation_preparation_result::refusal() const noexcept { return refusal_; }
const std::optional<pkgplan::package_operation_plan>&
operation_preparation_result::plan() const noexcept { return plan_; }
const std::optional<pkgapply::package_application_request>&
operation_preparation_result::application() const noexcept
{ return application_; }
const std::optional<effectful_operation_request>&
operation_preparation_result::effect() const noexcept { return effect_; }
const session_identity&
operation_preparation_result::identity() const noexcept { return identity_; }

operation_preparation_result prepare_operation(
    operation_preparation_request request,
    operation_preparation_driver& driver)
{
  auto installed = project_state(request);
  const auto* action = request.transaction().program().find(request.action_node());
  if (action == nullptr)
    throw error(error_code::invalid_preparation_request,
                "operation action disappeared from its sealed transaction");

  if (request.kind() == pkgplan::operation_kind::remove)
  {
    const auto* native_installed = action->installed();
    if (native_installed == nullptr)
      throw error(error_code::invalid_preparation_request,
                  "removal action lacks installed authority");
    const auto& projected = require_installed_projection(
        installed, *native_installed);
    auto planning = pkgplan::plan_removal(pkgplan::removal_request(
        projected, installed.ownership().snapshot(), installed.ownership(),
        request.target().target(), request.observations(), request.policy()));
    if (const auto* refusal = planning.refusal())
      return operation_preparation_result::refused(
          std::move(request), std::move(installed), std::nullopt,
          std::nullopt, operation_planning_refusal(*refusal));

    pkgplan::package_operation_plan plan(*planning.plan());
    pkgapply::package_application_request application(
        pkgapply::removal_application_request::make(
            *planning.plan(), request.target(), request.control()));
    auto effect = effectful_operation_request::make(
        request.transaction(), request.action_node(), application,
        request.lifecycle());
    return operation_preparation_result::success(
        std::move(request), std::move(installed), std::nullopt, std::nullopt,
        std::move(plan), std::move(application), std::move(effect));
  }

  if (!request.construction() || !request.runtime_closure())
    throw error(error_code::invalid_preparation_request,
                "incoming operation lacks construction or runtime closure");

  auto artifact = driver.project_artifact(*request.construction());
  validate_artifact_projection(*request.construction(), artifact);
  auto incoming = pkgapply::incoming_package_authority::admit(
      artifact.build(), artifact.image());
  if (incoming.candidate() != artifact.candidate().candidate())
    throw error(error_code::preparation_driver_contract_violation,
                "application and planner candidate projections disagree");

  const auto expected_archive = artifact.image().receipt().archive_digest();
  if (request.kind() == pkgplan::operation_kind::install)
  {
    auto planning = pkgplan::plan_install(pkgplan::installation_request(
        incoming.candidate(), artifact.artifact(), expected_archive,
        artifact.image(), installed.ownership().snapshot(),
        installed.ownership(), request.target().target(),
        request.observations(), *request.runtime_closure(), request.policy()));
    if (const auto* refusal = planning.refusal())
      return operation_preparation_result::refused(
          std::move(request), std::move(installed), std::move(artifact),
          std::move(incoming), operation_planning_refusal(*refusal));

    pkgplan::package_operation_plan plan(*planning.plan());
    pkgapply::package_application_request application(
        pkgapply::installation_application_request::make(
            *planning.plan(), incoming, request.target(), request.control()));
    auto effect = effectful_operation_request::make(
        request.transaction(), request.action_node(), application,
        request.lifecycle(), request.installation_reason());
    return operation_preparation_result::success(
        std::move(request), std::move(installed), std::move(artifact),
        std::move(incoming), std::move(plan), std::move(application),
        std::move(effect));
  }

  const auto* native_installed =
      request.transaction().resolution().installed().find_package(
          action->package().name());
  if (native_installed == nullptr)
    throw error(error_code::invalid_preparation_request,
                "upgrade action has no installed package authority");
  const auto& projected = require_installed_projection(
      installed, *native_installed);
  auto planning = pkgplan::plan_upgrade(pkgplan::upgrade_request(
      projected, incoming.candidate(), artifact.artifact(), expected_archive,
      artifact.image(), installed.ownership().snapshot(),
      installed.ownership(), request.target().target(),
      request.observations(), *request.runtime_closure(), request.policy()));
  if (const auto* refusal = planning.refusal())
    return operation_preparation_result::refused(
        std::move(request), std::move(installed), std::move(artifact),
        std::move(incoming), operation_planning_refusal(*refusal));

  pkgplan::package_operation_plan plan(*planning.plan());
  pkgapply::package_application_request application(
      pkgapply::upgrade_application_request::make(
          *planning.plan(), incoming, request.target(), request.control()));
  auto effect = effectful_operation_request::make(
      request.transaction(), request.action_node(), application,
      request.lifecycle());
  return operation_preparation_result::success(
      std::move(request), std::move(installed), std::move(artifact),
      std::move(incoming), std::move(plan), std::move(application),
      std::move(effect));
}

} // namespace pkgctl
