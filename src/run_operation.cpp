// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_operation.h>
#include <pkgctl/preparation.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include <libpkgapply-exec/derive.h>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_configuration(const std::string& message)
{
  throw native_operation_authority_error(
      native_operation_authority_error_code::invalid_configuration, message);
}

[[nodiscard]] bool absolute_normal(const std::filesystem::path& path)
{
  return path.is_absolute() && !path.empty() && path == path.lexically_normal();
}

[[nodiscard]] bool contains(const std::filesystem::path& parent,
                            const std::filesystem::path& child)
{
  auto parent_part = parent.begin();
  auto child_part = child.begin();
  for (; parent_part != parent.end() && child_part != child.end();
       ++parent_part, ++child_part)
    if (*parent_part != *child_part)
      return false;
  return parent_part == parent.end();
}

[[nodiscard]] std::optional<pkgplan::operation_kind> operation_kind(
    pkgtransaction::transaction_action_kind action)
{
  switch (action)
  {
    case pkgtransaction::transaction_action_kind::install:
      return pkgplan::operation_kind::install;
    case pkgtransaction::transaction_action_kind::upgrade:
      return pkgplan::operation_kind::upgrade;
    case pkgtransaction::transaction_action_kind::remove:
      return pkgplan::operation_kind::remove;
    case pkgtransaction::transaction_action_kind::build:
    case pkgtransaction::transaction_action_kind::check:
    case pkgtransaction::transaction_action_kind::retain:
    case pkgtransaction::transaction_action_kind::lifecycle:
      return std::nullopt;
  }
  return std::nullopt;
}

[[nodiscard]] pkgapply_exec::lifecycle_subject lifecycle_subject(
    pkgsource::lifecycle_action action)
{
  switch (action)
  {
    case pkgsource::lifecycle_action::pre_install:
    case pkgsource::lifecycle_action::post_install:
      return pkgapply_exec::lifecycle_subject::incoming;
    case pkgsource::lifecycle_action::pre_remove:
    case pkgsource::lifecycle_action::post_remove:
      return pkgapply_exec::lifecycle_subject::installed;
  }
  throw native_operation_authority_error(
      native_operation_authority_error_code::lifecycle_node_missing,
      "unsupported lifecycle action in transaction graph");
}

void validate_specification(
    const transaction_session& transaction,
    const native_transaction_operation_specification& specification,
    const pkgtransaction::transaction_node_identity& action)
{
  if (specification.action_node() != action)
    throw native_operation_authority_error(
        native_operation_authority_error_code::specification_missing,
        "operation specification belongs to another action node");
  const auto* node = transaction.program().find(action);
  const auto expected = node ? operation_kind(node->action()) : std::nullopt;
  if (!expected || *expected != specification.kind())
    throw native_operation_authority_error(
        native_operation_authority_error_code::specification_missing,
        "operation specification kind differs from transaction graph");
  if (specification.target().target() != specification.observations().target())
    throw native_operation_authority_error(
        native_operation_authority_error_code::specification_missing,
        "operation target and observations identify different targets");
}

[[nodiscard]] const construction_result& require_construction(
    const transaction_progress& progress,
    const pkgtransaction::transaction_node_identity& action)
{
  const construction_result* found = nullptr;
  for (const auto& edge : progress.transaction().program().edges())
  {
    if (!edge.phase_order() || edge.after() != action ||
        *edge.phase_order() !=
            pkgtransaction::phase_order_kind::build_before_target)
      continue;
    const auto* candidate = progress.construction(edge.before());
    if (candidate == nullptr || !candidate->succeeded())
      continue;
    if (found != nullptr && found->identity() != candidate->identity())
      throw native_operation_authority_error(
          native_operation_authority_error_code::construction_ambiguous,
          "operation action has more than one completed construction authority");
    found = candidate;
  }
  if (found == nullptr)
    throw native_operation_authority_error(
        native_operation_authority_error_code::construction_missing,
        "operation action lacks completed construction authority");
  return *found;
}

[[nodiscard]] operation_preparation_request preparation_request(
    const transaction_progress& progress,
    const native_transaction_operation_specification& specification,
    lifecycle_order lifecycle)
{
  switch (specification.kind())
  {
    case pkgplan::operation_kind::install:
      return operation_preparation_request::install(
          progress, specification.action_node(),
          require_construction(progress, specification.action_node()),
          specification.target(), specification.control(),
          specification.observations(), *specification.runtime_closure(),
          specification.policy(), std::move(lifecycle),
          *specification.installation_reason());
    case pkgplan::operation_kind::upgrade:
      return operation_preparation_request::upgrade(
          progress, specification.action_node(),
          require_construction(progress, specification.action_node()),
          specification.target(), specification.control(),
          specification.observations(), *specification.runtime_closure(),
          specification.policy(), std::move(lifecycle));
    case pkgplan::operation_kind::remove:
      return operation_preparation_request::remove(
          progress, specification.action_node(), specification.target(),
          specification.control(), specification.observations(),
          specification.policy(), std::move(lifecycle));
  }
  throw native_operation_authority_error(
      native_operation_authority_error_code::session_invalid,
      "unsupported operation kind in native specification");
}

[[nodiscard]] std::vector<pkgapply_exec::admitted_lifecycle_session>
admit_lifecycle_sessions(
    const pkgapply::package_application_request& application,
    const transaction_session& transaction,
    const pkgtransaction::transaction_node_identity& action_identity,
    const std::vector<pkgtransaction::transaction_node_identity>& order,
    const native_transaction_lifecycle_configuration& configuration,
    const std::filesystem::path& root)
{
  if (order.empty())
    return {};

  const auto nodes = pkgapply_exec::derive(application);
  const auto* action = transaction.program().find(action_identity);
  if (action == nullptr || !operation_kind(action->action()))
    throw native_operation_authority_error(
        native_operation_authority_error_code::lifecycle_node_missing,
        "operation action node is absent from transaction graph");

  std::vector<pkgapply_exec::admitted_lifecycle_session> result;
  result.reserve(order.size());
  for (std::size_t index = 0U; index < order.size(); ++index)
  {
    const auto* transaction_node = transaction.program().find(order[index]);
    if (transaction_node == nullptr || !transaction_node->lifecycle() ||
        transaction_node->package() != action->package())
      throw native_operation_authority_error(
          native_operation_authority_error_code::lifecycle_node_missing,
          "ordered lifecycle node does not belong to the operation package");
    const auto* node = nodes.find(
        lifecycle_subject(*transaction_node->lifecycle()),
        *transaction_node->lifecycle());
    if (node == nullptr)
      throw native_operation_authority_error(
          native_operation_authority_error_code::lifecycle_node_missing,
          "application projection lacks an ordered lifecycle node");
    result.push_back(pkgapply_exec::admitted_lifecycle_session::admit(
        application, *node,
        {configuration.execution_root,
         configuration.execution_root_path,
         application.target().root_view(),
         configuration.target_root_path,
         root / std::to_string(index)},
        configuration.execution_identity));
  }
  return result;
}

[[nodiscard]] effect_attempt_nonce operation_nonce(
    const transaction_run_journal_record& record,
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session)
{
  return effect_attempt_nonce::from_hex(make_session_identity(
      "pkgctl/native-operation-attempt-nonce/1",
      {record.journal().hex(), record.identity().hex(), run.identity().hex(),
       dispatch.identity().hex(), session.identity().hex()}).hex());
}

} // namespace

native_operation_authority_error::native_operation_authority_error(
    native_operation_authority_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

native_operation_authority_error_code
native_operation_authority_error::code() const noexcept
{
  return code_;
}

native_transaction_operation_specification::
native_transaction_operation_specification(
    pkgplan::operation_kind kind,
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    std::optional<pkgplan::runtime_dependency_closure_identity>
        runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle,
    std::optional<pkgstate::installation_reason> installation_reason)
    : kind_(kind), action_node_(std::move(action_node)),
      target_(std::move(target)), control_(std::move(control)),
      observations_(std::move(observations)),
      runtime_closure_(std::move(runtime_closure)), policy_(std::move(policy)),
      lifecycle_(std::move(lifecycle)),
      installation_reason_(std::move(installation_reason))
{
}

native_transaction_operation_specification
native_transaction_operation_specification::install(
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::runtime_dependency_closure_identity runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle,
    pkgstate::installation_reason installation_reason)
{
  return native_transaction_operation_specification(
      pkgplan::operation_kind::install, std::move(action_node),
      std::move(target), std::move(control), std::move(observations),
      std::move(runtime_closure), std::move(policy), std::move(lifecycle),
      std::move(installation_reason));
}

native_transaction_operation_specification
native_transaction_operation_specification::upgrade(
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::runtime_dependency_closure_identity runtime_closure,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle)
{
  return native_transaction_operation_specification(
      pkgplan::operation_kind::upgrade, std::move(action_node),
      std::move(target), std::move(control), std::move(observations),
      std::move(runtime_closure), std::move(policy), std::move(lifecycle),
      std::nullopt);
}

native_transaction_operation_specification
native_transaction_operation_specification::remove(
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::application_target_context target,
    pkgapply::application_execution_control control,
    pkgplan::target_observation_set observations,
    pkgplan::package_policy_snapshot policy,
    lifecycle_order lifecycle)
{
  return native_transaction_operation_specification(
      pkgplan::operation_kind::remove, std::move(action_node),
      std::move(target), std::move(control), std::move(observations),
      std::nullopt, std::move(policy), std::move(lifecycle), std::nullopt);
}

pkgplan::operation_kind
native_transaction_operation_specification::kind() const noexcept
{
  return kind_;
}

const pkgtransaction::transaction_node_identity&
native_transaction_operation_specification::action_node() const noexcept
{
  return action_node_;
}

const pkgapply::application_target_context&
native_transaction_operation_specification::target() const noexcept
{
  return target_;
}

const pkgapply::application_execution_control&
native_transaction_operation_specification::control() const noexcept
{
  return control_;
}

const pkgplan::target_observation_set&
native_transaction_operation_specification::observations() const noexcept
{
  return observations_;
}

const std::optional<pkgplan::runtime_dependency_closure_identity>&
native_transaction_operation_specification::runtime_closure() const noexcept
{
  return runtime_closure_;
}

const pkgplan::package_policy_snapshot&
native_transaction_operation_specification::policy() const noexcept
{
  return policy_;
}

const lifecycle_order&
native_transaction_operation_specification::lifecycle() const noexcept
{
  return lifecycle_;
}

const std::optional<pkgstate::installation_reason>&
native_transaction_operation_specification::installation_reason() const noexcept
{
  return installation_reason_;
}

native_transaction_operation_configuration::
native_transaction_operation_configuration(
    transaction_session transaction,
    native_transaction_lifecycle_configuration lifecycle)
    : transaction_(std::move(transaction)), lifecycle_(std::move(lifecycle))
{
}

native_transaction_operation_configuration
native_transaction_operation_configuration::make(
    transaction_session transaction,
    native_transaction_lifecycle_configuration lifecycle)
{
  if (!absolute_normal(lifecycle.execution_root_path) ||
      !absolute_normal(lifecycle.target_root_path) ||
      !absolute_normal(lifecycle.session_root))
    invalid_configuration(
        "native lifecycle paths must be absolute and normalized");
  if (contains(lifecycle.execution_root_path, lifecycle.target_root_path) ||
      contains(lifecycle.target_root_path, lifecycle.execution_root_path) ||
      contains(lifecycle.execution_root_path, lifecycle.session_root) ||
      contains(lifecycle.session_root, lifecycle.execution_root_path) ||
      contains(lifecycle.target_root_path, lifecycle.session_root) ||
      contains(lifecycle.session_root, lifecycle.target_root_path))
    invalid_configuration(
        "native lifecycle execution, target, and session roots must be disjoint");

  return native_transaction_operation_configuration(
      std::move(transaction), std::move(lifecycle));
}

const transaction_session&
native_transaction_operation_configuration::transaction() const noexcept
{
  return transaction_;
}

const native_transaction_lifecycle_configuration&
native_transaction_operation_configuration::lifecycle() const noexcept
{
  return lifecycle_;
}

native_transaction_operation_authority_source::
native_transaction_operation_authority_source(
    native_transaction_operation_configuration configuration,
    transaction_operation_specification_source& specifications,
    effect_journal_store& effects,
    transaction_effect_restart_body_source& bodies,
    transaction_operation_session_sink* sessions)
    : configuration_(std::move(configuration)),
      specifications_(specifications), effects_(effects), bodies_(bodies),
      sessions_(sessions)
{
}

effectful_operation_session
native_transaction_operation_authority_source::session(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    bool retain) const
{
  if (record.transaction() != configuration_.transaction().identity() ||
      progress.transaction().identity() !=
          configuration_.transaction().identity())
    throw native_operation_authority_error(
        native_operation_authority_error_code::transaction_mismatch,
        "native operation authority was supplied for another transaction");
  if (dispatch.unit().kind() != transaction_unit_kind::operation)
    throw native_operation_authority_error(
        native_operation_authority_error_code::operation_dispatch_required,
        "native operation authority requires an operation dispatch");

  auto specification = specifications_.operation(record, progress, dispatch);
  validate_specification(
      configuration_.transaction(), specification,
      dispatch.unit().primary_node());

  try
  {
    native_operation_preparation_driver driver;
    auto prepared = prepare_operation(
        preparation_request(
            progress, specification, specification.lifecycle()),
        driver);
    if (!prepared.prepared() || !prepared.effect() || !prepared.application())
    {
      std::ostringstream message;
      message << "native operation planning refused";
      if (prepared.refusal())
        message << " with code "
                << static_cast<unsigned>(prepared.refusal()->code());
      throw native_operation_authority_error(
          native_operation_authority_error_code::planning_refused,
          message.str());
    }

    const auto base = configuration_.lifecycle().session_root /
        record.journal().hex() / dispatch.identity().hex();
    auto before = admit_lifecycle_sessions(
        *prepared.application(), configuration_.transaction(),
        dispatch.unit().primary_node(), prepared.effect()->lifecycle().before(),
        configuration_.lifecycle(),
        base / "before");
    auto after = admit_lifecycle_sessions(
        *prepared.application(), configuration_.transaction(),
        dispatch.unit().primary_node(), prepared.effect()->lifecycle().after(),
        configuration_.lifecycle(),
        base / "after");
    auto admitted = effectful_operation_session::admit(
        *prepared.effect(), std::move(before), std::move(after));
    if (retain && sessions_ != nullptr)
      sessions_->retain(record, progress, dispatch, specification, admitted);
    return admitted;
  }
  catch (const native_operation_authority_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    throw native_operation_authority_error(
        native_operation_authority_error_code::session_invalid,
        std::string("native operation session is invalid: ") + problem.what());
  }
}

operation_dispatch_execution_authority
native_transaction_operation_authority_source::operation(
    const transaction_run_journal_record& record,
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  if (record.run() != run.identity())
    throw native_operation_authority_error(
        native_operation_authority_error_code::transaction_mismatch,
        "native operation authority was supplied for another durable run");
  auto value = session(record, run.progress(), dispatch, true);
  auto nonce = operation_nonce(record, run, dispatch, value);
  return {std::move(value), std::move(nonce)};
}

effect_restart_checkpoint
native_transaction_operation_authority_source::operation(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch)
{
  if (assessment.kind() != transaction_unit_kind::operation ||
      assessment.dispatch() != dispatch.identity() ||
      !assessment.effect_attempt())
    throw native_operation_authority_error(
        native_operation_authority_error_code::effect_attempt_mismatch,
        "restart assessment does not identify one operation attempt");

  auto value = session(
      checkpoint.record(), checkpoint.run().progress(), dispatch, false);
  const auto record = effects_.load_latest(*assessment.effect_attempt());
  if (!record)
    throw native_operation_authority_error(
        native_operation_authority_error_code::effect_attempt_missing,
        "effect journal lacks the active operation attempt");
  if (record->attempt() != *assessment.effect_attempt() ||
      record->session() != value.identity())
    throw native_operation_authority_error(
        native_operation_authority_error_code::effect_attempt_mismatch,
        "effect journal attempt differs from reconstructed operation session");

  return this->checkpoint(std::move(value), *record);
}

effect_restart_checkpoint
native_transaction_operation_authority_source::rehydrate(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const effect_attempt_record& evidence)
{
  auto value = session(record, progress, dispatch, false);
  if (evidence.session() != value.identity() ||
      evidence.stage() != effect_attempt_stage::terminal)
    throw native_operation_authority_error(
        native_operation_authority_error_code::effect_attempt_mismatch,
        "terminal effect evidence differs from reconstructed operation session");
  return checkpoint(std::move(value), evidence);
}

effect_restart_checkpoint
native_transaction_operation_authority_source::checkpoint(
    effectful_operation_session session,
    const effect_attempt_record& record)
{
  try
  {
    auto bodies = bodies_.load(session, record);
    return effect_restart_checkpoint::make(
        std::move(session), record, std::move(bodies.before),
        std::move(bodies.application), std::move(bodies.after),
        std::move(bodies.publication_request),
        std::move(bodies.publication_receipt),
        std::move(bodies.application_journal));
  }
  catch (const native_operation_authority_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    throw native_operation_authority_error(
        native_operation_authority_error_code::restart_body_invalid,
        std::string("operation restart body is invalid: ") + problem.what());
  }
}

explicit_transaction_effect_archive_source::
explicit_transaction_effect_archive_source(
    pkgimage::archive_backend& backend,
    std::vector<retained_transaction_effect_archive> archives)
    : backend_(backend), archives_(std::move(archives))
{
}

explicit_transaction_effect_archive_source
explicit_transaction_effect_archive_source::make(
    pkgimage::archive_backend& backend,
    std::vector<retained_transaction_effect_archive> archives)
{
  for (std::size_t index = 0U; index < archives.size(); ++index)
  {
    if (!absolute_normal(archives[index].path))
      invalid_configuration(
          "retained operation archive path must be absolute and normalized");
    for (std::size_t other = 0U; other < index; ++other)
      if (archives[other].incoming == archives[index].incoming)
        invalid_configuration(
            "retained operation archive map contains duplicate authority");
  }
  return explicit_transaction_effect_archive_source(
      backend, std::move(archives));
}

std::unique_ptr<pkgimage::package_archive>
explicit_transaction_effect_archive_source::open_archive(
    const pkgapply::incoming_package_authority& incoming)
{
  const auto found = std::find_if(
      archives_.begin(), archives_.end(),
      [&](const retained_transaction_effect_archive& value) {
        return value.incoming == incoming.identity();
      });
  if (found == archives_.end())
    return nullptr;
  return backend_.open(
      {found->path, incoming.image().receipt().archive_digest()});
}

} // namespace pkgctl
