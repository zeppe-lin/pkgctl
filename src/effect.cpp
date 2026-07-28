// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect.h>
#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace pkgctl {
namespace {

template<typename Left, typename Right>
bool same_string_identity(const Left& lhs, const Right& rhs)
{
  return lhs.string() == rhs.string();
}

const pkgplan::operation_preconditions& preconditions(
    const pkgapply::package_application_request& request)
{
  if (const auto* value = request.installation())
    return value->plan().preconditions();
  if (const auto* value = request.upgrade())
    return value->plan().preconditions();
  if (const auto* value = request.removal())
    return value->plan().preconditions();
  throw error(error_code::invalid_effect_request,
              "effect application request has no operation body");
}

const pkgplan::package_release& application_release(
    const pkgapply::package_application_request& request)
{
  if (const auto* value = request.installation())
    return value->plan().release();
  if (const auto* value = request.upgrade())
    return value->plan().release();
  if (const auto* value = request.removal())
    return value->plan().release();
  throw error(error_code::invalid_effect_request,
              "effect application request has no operation body");
}

pkgtransaction::transaction_action_kind action_kind(pkgplan::operation_kind value)
{
  switch (value)
  {
    case pkgplan::operation_kind::install:
      return pkgtransaction::transaction_action_kind::install;
    case pkgplan::operation_kind::upgrade:
      return pkgtransaction::transaction_action_kind::upgrade;
    case pkgplan::operation_kind::remove:
      return pkgtransaction::transaction_action_kind::remove;
  }
  throw error(error_code::invalid_effect_request,
              "effect application request has an invalid operation kind");
}

std::vector<std::string> identity_text(
    const std::vector<pkgtransaction::transaction_node_identity>& values)
{
  std::vector<std::string> result;
  result.reserve(values.size());
  for (const auto& value : values)
    result.push_back(value.hex());
  return result;
}

void require_unique(
    const std::vector<pkgtransaction::transaction_node_identity>& values,
    const char* label)
{
  std::set<std::string> seen;
  for (const auto& value : values)
  {
    if (!seen.insert(value.hex()).second)
      throw error(error_code::invalid_effect_request,
                  std::string("duplicate lifecycle node in ") + label);
  }
}

std::string reason_kind(const pkgstate::installation_reason& reason)
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
  return make_session_identity("pkgctl/installation-reason/1", fields).hex();
}


void validate_action_authority(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node& action,
    const pkgapply::package_application_request& application)
{
  if (action.environment() != pkgresolve::resolution_environment::target ||
      action.action() != action_kind(application.kind()))
  {
    throw error(error_code::invalid_effect_request,
                "selected transaction node is not the requested target action");
  }

  if (action.package().name() != application_release(application).name())
    throw error(error_code::invalid_effect_request,
                "transaction action package differs from application plan");

  const auto& required = preconditions(application);
  if (!same_string_identity(required.installed_snapshot(),
                            transaction.resolution().installed().identity()) ||
      !same_string_identity(required.ownership_inventory(),
                            transaction.resolution().installed().ownership_identity()))
  {
    throw error(error_code::invalid_effect_request,
                "application plan is not based on the transaction state snapshot");
  }

  const auto& state_target =
      transaction.resolution().installed().target_binding();
  if (!same_string_identity(application.target().managed_target(),
                            state_target.managed_target()) ||
      !same_string_identity(application.target().root_view(),
                            state_target.root_view()))
  {
    throw error(error_code::invalid_effect_request,
                "application target differs from the transaction state target");
  }

  if (application.kind() == pkgplan::operation_kind::remove)
  {
    const auto* installed = action.installed();
    const auto* removal = application.removal();
    if (installed == nullptr || removal == nullptr ||
        !same_string_identity(removal->plan().inputs().package(),
                              installed->identity()) ||
        !same_string_identity(removal->plan().inputs().control(),
                              installed->control().identity()))
    {
      throw error(error_code::invalid_effect_request,
                  "removal action authority differs from the accepted plan");
    }
    return;
  }

  const auto* selection = action.selection();
  const auto* incoming = application.incoming();
  if (selection == nullptr || incoming == nullptr ||
      selection->source_snapshot().hex() !=
          incoming->build().request().source().identity().hex() ||
      selection->release().identity().hex() !=
          incoming->build().request().release().identity().hex())
  {
    throw error(error_code::invalid_effect_request,
                "incoming action authority differs from the selected source");
  }

  if (application.kind() == pkgplan::operation_kind::upgrade)
  {
    const auto* upgrade = application.upgrade();
    const auto* installed = transaction.resolution().installed().find_package(
        application_release(application).name());
    if (upgrade == nullptr || installed == nullptr ||
        !same_string_identity(upgrade->plan().inputs().old_package(),
                              installed->identity()) ||
        !same_string_identity(upgrade->plan().inputs().old_control(),
                              installed->control().identity()))
    {
      throw error(error_code::invalid_effect_request,
                  "upgrade old-package authority differs from transaction state");
    }
  }
}

void validate_single_package_program(
    const pkgtransaction::transaction_program& program,
    const pkgtransaction::transaction_node& action)
{
  std::size_t target_mutations = 0;
  for (const auto& node : program.nodes())
  {
    if (node.package() != action.package())
      throw error(error_code::invalid_effect_request,
                  "effectful session v1 requires a single-package transaction program");
    if (node.environment() == pkgresolve::resolution_environment::target &&
        (node.action() == pkgtransaction::transaction_action_kind::install ||
         node.action() == pkgtransaction::transaction_action_kind::upgrade ||
         node.action() == pkgtransaction::transaction_action_kind::remove))
    {
      ++target_mutations;
      if (node.identity() != action.identity())
        throw error(error_code::invalid_effect_request,
                    "transaction program contains another target mutation");
    }
  }
  if (target_mutations != 1U || !program.runtime_cohorts().empty())
    throw error(error_code::invalid_effect_request,
                "effectful session v1 requires one acyclic target mutation");
}

std::vector<pkgtransaction::transaction_node_identity> required_lifecycle(
    const pkgtransaction::transaction_program& program,
    const pkgtransaction::transaction_node_identity& action,
    bool before)
{
  std::vector<pkgtransaction::transaction_node_identity> result;
  for (const auto& edge : program.edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::phase ||
        !edge.phase_order())
      continue;
    if (before &&
        *edge.phase_order() ==
            pkgtransaction::phase_order_kind::pre_lifecycle_before_action &&
        edge.after() == action)
      result.push_back(edge.before());
    if (!before &&
        *edge.phase_order() ==
            pkgtransaction::phase_order_kind::action_before_post_lifecycle &&
        edge.before() == action)
      result.push_back(edge.after());
  }
  std::sort(result.begin(), result.end());
  return result;
}

void validate_lifecycle_order(
    const pkgtransaction::transaction_program& program,
    const pkgtransaction::transaction_node_identity& action,
    const lifecycle_order& order)
{
  auto before = order.before();
  auto after = order.after();
  std::sort(before.begin(), before.end());
  std::sort(after.begin(), after.end());
  if (before != required_lifecycle(program, action, true) ||
      after != required_lifecycle(program, action, false))
  {
    throw error(error_code::invalid_effect_request,
                "lifecycle order does not contain the exact transaction phase nodes");
  }
}

const pkgstate::installed_package* transaction_installed_authority(
    const pkgtransaction::transaction_node& node)
{
  if (const auto* value = node.installed())
    return value;
  if (const auto* selected = node.selection())
    return selected->installed();
  return nullptr;
}

void validate_lifecycle_binding(
    const effectful_operation_request& request,
    const pkgtransaction::transaction_node_identity& transaction_node,
    const pkgapply_exec::admitted_lifecycle_session& execution)
{
  const auto* node = request.transaction().program().find(transaction_node);
  if (node == nullptr ||
      node->action() != pkgtransaction::transaction_action_kind::lifecycle ||
      !node->lifecycle())
  {
    throw error(error_code::invalid_effect_session,
                "lifecycle execution is not bound to a transaction lifecycle node");
  }

  const auto& application = request.application();
  const auto& lifecycle = execution.node();
  if (execution.request().identity() != application.identity() ||
      lifecycle.application_request() != application.identity() ||
      lifecycle.plan() != application.plan() ||
      lifecycle.operation() != application.kind() ||
      lifecycle.target() != application.target().identity() ||
      !application.target().lifecycle_executor() ||
      lifecycle.executor() != *application.target().lifecycle_executor() ||
      lifecycle.action() != *node->lifecycle() ||
      lifecycle.release().name() != node->package().name())
  {
    throw error(error_code::invalid_effect_session,
                "lifecycle execution authority differs from transaction or application");
  }

  if (lifecycle.subject() == pkgapply_exec::lifecycle_subject::incoming)
  {
    const auto* selected = node->selection();
    if (selected == nullptr || !lifecycle.source() ||
        selected->source_snapshot().hex() != lifecycle.source()->hex() ||
        selected->release().package().name() != lifecycle.release().name() ||
        selected->release().version() != lifecycle.release().version() ||
        std::to_string(selected->release().release()) !=
            lifecycle.release().release())
    {
      throw error(error_code::invalid_effect_session,
                  "incoming lifecycle source differs from transaction authority");
    }
  }
  else
  {
    const auto* installed = transaction_installed_authority(*node);
    if (installed == nullptr || !lifecycle.installed_control() ||
        !same_string_identity(*lifecycle.installed_control(),
                              installed->control().identity()) ||
        installed->release().name() != lifecycle.release().name() ||
        installed->release().version() != lifecycle.release().version() ||
        std::to_string(installed->release().release()) !=
            lifecycle.release().release())
    {
      throw error(error_code::invalid_effect_session,
                  "installed lifecycle control differs from transaction authority");
    }
  }
}

void append_execution_session_fields(
    std::vector<std::string>& fields,
    const pkgapply_exec::admitted_lifecycle_session& session)
{
  fields.push_back(session.node().identity().hex());
  fields.push_back(session.paths().execution_root.hex());
  fields.push_back(session.paths().target_root.string());
  fields.push_back(session.execution_identity().interpreter.hex());
  fields.push_back(std::to_string(session.execution_identity().user_id));
  fields.push_back(std::to_string(session.execution_identity().group_id));
  fields.push_back(std::to_string(
      session.execution_identity().supplementary_groups.size()));
  for (const auto value : session.execution_identity().supplementary_groups)
    fields.push_back(std::to_string(value));
}

void validate_lifecycle_result(
    const pkgapply_exec::admitted_lifecycle_session& session,
    const pkgapply_exec::lifecycle_execution_result& result)
{
  const auto& request = result.execution().request();
  const auto& identity = session.execution_identity();
  if (result.node().identity() != session.node().identity() ||
      request.program() != session.node().program() ||
      request.purpose().kind() != pkgexec::execution_purpose_kind::lifecycle ||
      !request.purpose().action() ||
      *request.purpose().action() != session.node().action() ||
      request.interpreter() != identity.interpreter ||
      request.root_view() != session.paths().execution_root ||
      request.credentials().user_id() != identity.user_id ||
      request.credentials().group_id() != identity.group_id ||
      request.credentials().supplementary_groups() !=
          identity.supplementary_groups ||
      !request.credentials().no_new_privileges() ||
      request.environment().network() != pkgexec::network_policy::denied)
  {
    throw error(error_code::driver_contract_violation,
                "lifecycle driver returned evidence for another execution session");
  }
}

std::array<std::uint8_t, 32> decode_hex(const std::string& value)
{
  if (value.size() != 64U)
    throw error(error_code::identity_failure,
                "controller identity is not a SHA-256 hex value");
  std::array<std::uint8_t, 32> result{};
  const auto digit = [](char value) -> unsigned int {
    if (value >= '0' && value <= '9')
      return static_cast<unsigned int>(value - '0');
    if (value >= 'a' && value <= 'f')
      return static_cast<unsigned int>(value - 'a' + 10);
    return 16U;
  };
  for (std::size_t index = 0; index < result.size(); ++index)
  {
    const unsigned int high = digit(value[index * 2U]);
    const unsigned int low = digit(value[index * 2U + 1U]);
    if (high > 15U || low > 15U)
      throw error(error_code::identity_failure,
                  "controller identity contains invalid hex");
    result[index] = static_cast<std::uint8_t>((high << 4U) | low);
  }
  return result;
}

pkgstate::transaction_evidence_identity transaction_evidence(
    const effectful_operation_session& session,
    const std::vector<pkgapply_exec::lifecycle_execution_result>& before,
    const pkgapply::completed_application_evidence& application,
    const std::vector<pkgapply_exec::lifecycle_execution_result>& after)
{
  std::vector<std::string> fields{session.identity().hex()};
  fields.push_back(std::to_string(before.size()));
  for (const auto& value : before)
    fields.push_back(value.identity().hex());
  fields.push_back(application.identity().string());
  fields.push_back(std::to_string(after.size()));
  for (const auto& value : after)
    fields.push_back(value.identity().hex());
  const session_identity identity = make_session_identity(
      "pkgctl/transaction-evidence/1", fields);
  return pkgstate::transaction_evidence_identity::from_sha256(
      decode_hex(identity.hex()));
}

pkgstate::state_publication_request project_publication(
    const effectful_operation_request& request,
    const pkgapply::lease_bound_state_projection& state,
    const pkgapply::completed_application_evidence& evidence,
    pkgstate::transaction_evidence_identity transaction)
{
  const auto& expected = request.transaction().resolution().installed();
  if (const auto* value = request.application().installation())
  {
    if (!request.installation_reason())
      throw error(error_code::invalid_effect_request,
                  "installation effect lacks an installation reason");
    return pkgstate::apply_adapter::project_completed_application(
        expected, state, *value, evidence, *request.installation_reason(),
        std::move(transaction));
  }
  if (const auto* value = request.application().upgrade())
    return pkgstate::apply_adapter::project_completed_application(
        expected, state, *value, evidence, std::move(transaction));
  if (const auto* value = request.application().removal())
    return pkgstate::apply_adapter::project_completed_application(
        expected, state, *value, evidence, std::move(transaction));
  throw error(error_code::invalid_effect_request,
              "effect application request has no operation body");
}

void validate_application_receipt(
    const effectful_operation_request& request,
    const pkgapply::lease_bound_state_projection& state,
    const pkgapply::application_receipt& receipt)
{
  if (receipt.request() != request.application().identity() ||
      receipt.plan() != request.application().plan() ||
      receipt.kind() != request.application().kind() ||
      receipt.target() != request.application().target().identity() ||
      receipt.control() != request.application().control().identity() ||
      receipt.state_projection() != state.identity())
  {
    throw error(error_code::driver_contract_violation,
                "application driver returned evidence for another authority universe");
  }
  if (receipt.outcome() == pkgapply::application_attempt_outcome::completed &&
      !receipt.completed_evidence())
  {
    throw error(error_code::driver_contract_violation,
                "completed application receipt lacks completed evidence");
  }
}

session_identity result_identity(
    const effectful_operation_session& session,
    effectful_operation_outcome outcome,
    const std::vector<pkgapply_exec::lifecycle_execution_result>& before,
    const std::optional<pkgapply::application_receipt>& application,
    const std::vector<pkgapply_exec::lifecycle_execution_result>& after,
    const std::optional<pkgstate::transaction_evidence_identity>& transaction,
    const std::optional<pkgstate::state_publication_request>& publication_request,
    const std::optional<pkgstate::state_publication_receipt>& publication_receipt,
    const std::optional<pkgstate::installed_state_snapshot_identity>&
        reconciled_state)
{
  std::vector<std::string> fields{
      session.identity().hex(),
      std::to_string(static_cast<unsigned int>(outcome)),
      std::to_string(before.size())};
  for (const auto& value : before)
    fields.push_back(value.identity().hex());
  fields.push_back(application ? application->identity().string() : std::string());
  fields.push_back(std::to_string(after.size()));
  for (const auto& value : after)
    fields.push_back(value.identity().hex());
  fields.push_back(transaction ? transaction->string() : std::string());
  fields.push_back(publication_request
                       ? publication_request->identity().string()
                       : std::string());
  fields.push_back(publication_receipt
                       ? publication_receipt->identity().string()
                       : std::string());
  if (!reconciled_state)
    return make_session_identity("pkgctl/effectful-operation-result/1", fields);
  fields.push_back(reconciled_state->string());
  return make_session_identity("pkgctl/effectful-operation-result/2", fields);
}

effectful_operation_outcome publication_outcome(
    const pkgstate::state_publication_receipt& receipt)
{
  switch (receipt.outcome())
  {
    case pkgstate::state_publication_outcome::published:
      return effectful_operation_outcome::completed;
    case pkgstate::state_publication_outcome::published_durability_unconfirmed:
    case pkgstate::state_publication_outcome::indeterminate:
      return effectful_operation_outcome::state_publication_indeterminate;
    case pkgstate::state_publication_outcome::stale_expected_state:
    case pkgstate::state_publication_outcome::request_rejected:
    case pkgstate::state_publication_outcome::failed_before_publication:
      return effectful_operation_outcome::state_publication_not_completed;
  }
  throw error(error_code::driver_contract_violation,
              "state driver returned an invalid publication outcome");
}

pkgstate::snapshot resulting_snapshot(
    const pkgstate::snapshot& expected,
    const pkgstate::state_publication_request& request)
{
  if (request.expected_snapshot() != expected.identity() ||
      request.target_binding() != expected.target_binding())
    throw error(error_code::driver_contract_violation,
                "publication request differs from the expected state authority");

  auto packages = expected.packages();
  for (const auto& delta : request.deltas())
  {
    const auto iterator = std::find_if(
        packages.begin(), packages.end(), [&delta](const auto& package) {
          return package.release().name() == delta.package_name();
        });
    switch (delta.kind())
    {
      case pkgstate::package_state_delta_kind::install:
        if (iterator != packages.end() || !delta.proposed_package())
          throw error(error_code::driver_contract_violation,
                      "installation publication delta is not realizable");
        packages.push_back(*delta.proposed_package());
        break;
      case pkgstate::package_state_delta_kind::replace:
        if (iterator == packages.end() || !delta.expected_package() ||
            !delta.proposed_package() ||
            iterator->identity() != *delta.expected_package())
          throw error(error_code::driver_contract_violation,
                      "replacement publication delta is not realizable");
        *iterator = *delta.proposed_package();
        break;
      case pkgstate::package_state_delta_kind::remove:
        if (iterator == packages.end() || !delta.expected_package() ||
            iterator->identity() != *delta.expected_package())
          throw error(error_code::driver_contract_violation,
                      "removal publication delta is not realizable");
        packages.erase(iterator);
        break;
    }
  }
  return pkgstate::snapshot::make(expected.target_binding(), std::move(packages));
}

pkgstate::installed_state_snapshot_identity state_identity_from_string(
    const std::string& value)
{
  return pkgstate::installed_state_snapshot_identity::parse(value);
}

pkgstate::transaction_evidence_identity transaction_identity_from_string(
    const std::string& value)
{
  return pkgstate::transaction_evidence_identity::parse(value);
}

effect_attempt_record append_record(
    effect_journal_store& store,
    const effect_attempt_record& record)
{
  effect_attempt_record stored = store.append(record);
  if (stored.identity() != record.identity())
    throw error(error_code::driver_contract_violation,
                "effect journal store returned another snapshot");
  return stored;
}

} // namespace

lifecycle_order::lifecycle_order(
    std::vector<pkgtransaction::transaction_node_identity> before,
    std::vector<pkgtransaction::transaction_node_identity> after)
    : before_(std::move(before)), after_(std::move(after))
{
}

lifecycle_order lifecycle_order::make(
    std::vector<pkgtransaction::transaction_node_identity> before,
    std::vector<pkgtransaction::transaction_node_identity> after)
{
  require_unique(before, "pre-application order");
  require_unique(after, "post-application order");
  std::set<std::string> all;
  for (const auto& value : before)
    all.insert(value.hex());
  for (const auto& value : after)
  {
    if (!all.insert(value.hex()).second)
      throw error(error_code::invalid_effect_request,
                  "lifecycle node appears on both sides of the action");
  }
  return lifecycle_order(std::move(before), std::move(after));
}

const std::vector<pkgtransaction::transaction_node_identity>&
lifecycle_order::before() const noexcept { return before_; }
const std::vector<pkgtransaction::transaction_node_identity>&
lifecycle_order::after() const noexcept { return after_; }

effectful_operation_request::effectful_operation_request(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::package_application_request application,
    lifecycle_order lifecycle,
    std::optional<pkgstate::installation_reason> installation_reason,
    session_identity identity)
    : transaction_(std::move(transaction)),
      action_node_(std::move(action_node)),
      application_(std::move(application)),
      lifecycle_(std::move(lifecycle)),
      installation_reason_(std::move(installation_reason)),
      identity_(std::move(identity))
{
}

effectful_operation_request effectful_operation_request::make(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity action_node,
    pkgapply::package_application_request application,
    lifecycle_order lifecycle,
    std::optional<pkgstate::installation_reason> installation_reason)
{
  const auto* action = transaction.program().find(action_node);
  if (action == nullptr)
    throw error(error_code::invalid_effect_request,
                "effect action node is absent from the transaction program");
  validate_action_authority(transaction, *action, application);
  validate_single_package_program(transaction.program(), *action);
  validate_lifecycle_order(transaction.program(), action_node, lifecycle);

  if (application.kind() == pkgplan::operation_kind::install)
  {
    if (!installation_reason)
      throw error(error_code::invalid_effect_request,
                  "installation effect requires an installation reason");
  }
  else if (installation_reason)
  {
    throw error(error_code::invalid_effect_request,
                "upgrade or removal effect cannot replace installed reason policy");
  }

  std::vector<std::string> fields{
      transaction.identity().hex(), action_node.hex(),
      application.identity().string()};
  const auto before = identity_text(lifecycle.before());
  fields.push_back(std::to_string(before.size()));
  fields.insert(fields.end(), before.begin(), before.end());
  const auto after = identity_text(lifecycle.after());
  fields.push_back(std::to_string(after.size()));
  fields.insert(fields.end(), after.begin(), after.end());
  fields.push_back(installation_reason
                       ? reason_kind(*installation_reason)
                       : std::string());
  session_identity identity = make_session_identity(
      "pkgctl/effectful-operation-request/1", fields);
  return effectful_operation_request(
      std::move(transaction), std::move(action_node), std::move(application),
      std::move(lifecycle), std::move(installation_reason),
      std::move(identity));
}

const transaction_session& effectful_operation_request::transaction() const noexcept
{ return transaction_; }
const pkgtransaction::transaction_node_identity&
effectful_operation_request::action_node() const noexcept { return action_node_; }
const pkgapply::package_application_request&
effectful_operation_request::application() const noexcept { return application_; }
const lifecycle_order& effectful_operation_request::lifecycle() const noexcept
{ return lifecycle_; }
const std::optional<pkgstate::installation_reason>&
effectful_operation_request::installation_reason() const noexcept
{ return installation_reason_; }
const session_identity& effectful_operation_request::identity() const noexcept
{ return identity_; }

effectful_operation_session::effectful_operation_session(
    effectful_operation_request request,
    std::vector<pkgapply_exec::admitted_lifecycle_session> before,
    std::vector<pkgapply_exec::admitted_lifecycle_session> after,
    session_identity identity)
    : request_(std::move(request)), before_(std::move(before)),
      after_(std::move(after)), identity_(std::move(identity))
{
}

effectful_operation_session effectful_operation_session::admit(
    effectful_operation_request request,
    std::vector<pkgapply_exec::admitted_lifecycle_session> before,
    std::vector<pkgapply_exec::admitted_lifecycle_session> after)
{
  if (before.size() != request.lifecycle().before().size() ||
      after.size() != request.lifecycle().after().size())
    throw error(error_code::invalid_effect_session,
                "lifecycle execution count differs from the requested order");
  for (std::size_t index = 0; index < before.size(); ++index)
    validate_lifecycle_binding(
        request, request.lifecycle().before()[index], before[index]);
  for (std::size_t index = 0; index < after.size(); ++index)
    validate_lifecycle_binding(
        request, request.lifecycle().after()[index], after[index]);

  std::vector<std::string> fields{request.identity().hex()};
  fields.push_back(std::to_string(before.size()));
  for (const auto& value : before)
    append_execution_session_fields(fields, value);
  fields.push_back(std::to_string(after.size()));
  for (const auto& value : after)
    append_execution_session_fields(fields, value);
  session_identity identity = make_session_identity(
      "pkgctl/effectful-operation-session/1", fields);
  return effectful_operation_session(
      std::move(request), std::move(before), std::move(after),
      std::move(identity));
}

const effectful_operation_request&
effectful_operation_session::request() const noexcept { return request_; }
const std::vector<pkgapply_exec::admitted_lifecycle_session>&
effectful_operation_session::before() const noexcept { return before_; }
const std::vector<pkgapply_exec::admitted_lifecycle_session>&
effectful_operation_session::after() const noexcept { return after_; }
const session_identity& effectful_operation_session::identity() const noexcept
{ return identity_; }

pkgapply::application_receipt transaction_effect_driver::resume_application(
    const pkgapply::package_application_request&,
    const pkgapply::application_journal_record&)
{
  throw error(error_code::invalid_effect_session,
              "effect driver does not support application restart");
}

pkgstate::snapshot transaction_effect_driver::read_state() const
{
  throw error(error_code::invalid_effect_session,
              "effect driver does not support state reconciliation");
}

native_transaction_effect_driver::native_transaction_effect_driver(
    const pkgapply::lease_bound_state_projection& state,
    pkgapply::target_mutation_lease& lease,
    pkgapply::application_backend& application_backend,
    const pkgimage::package_archive* incoming_archive,
    pkgexec::execution_backend& lifecycle_backend,
    pkgstate::canonical_store& state_store)
    : state_(state), lease_(lease), application_backend_(application_backend),
      incoming_archive_(incoming_archive), lifecycle_backend_(lifecycle_backend),
      state_store_(state_store)
{
}

pkgapply::target_mutation_lease&
native_transaction_effect_driver::lease() noexcept { return lease_; }
const pkgapply::lease_bound_state_projection&
native_transaction_effect_driver::state_projection() const noexcept
{ return state_; }

pkgapply_exec::lifecycle_execution_result
native_transaction_effect_driver::execute_lifecycle(
    const pkgapply_exec::admitted_lifecycle_session& session)
{
  return pkgapply_exec::execute(session, lifecycle_backend_);
}

pkgapply::application_receipt
native_transaction_effect_driver::apply_application(
    const pkgapply::package_application_request& request)
{
  if (const auto* value = request.installation())
  {
    if (incoming_archive_ == nullptr)
      throw error(error_code::invalid_effect_session,
                  "installation effect lacks incoming archive authority");
    return pkgapply::apply(
        *value, state_, lease_, application_backend_, *incoming_archive_);
  }
  if (const auto* value = request.upgrade())
  {
    if (incoming_archive_ == nullptr)
      throw error(error_code::invalid_effect_session,
                  "upgrade effect lacks incoming archive authority");
    return pkgapply::apply(
        *value, state_, lease_, application_backend_, *incoming_archive_);
  }
  if (const auto* value = request.removal())
    return pkgapply::apply(*value, state_, lease_, application_backend_);
  throw error(error_code::invalid_effect_session,
              "application request has no operation body");
}

pkgapply::application_receipt
native_transaction_effect_driver::resume_application(
    const pkgapply::package_application_request& request,
    const pkgapply::application_journal_record& journal)
{
  if (const auto* value = request.installation())
  {
    if (incoming_archive_ == nullptr)
      throw error(error_code::invalid_effect_session,
                  "installation restart lacks incoming archive authority");
    return pkgapply::resume_application(
        *value, state_, lease_, application_backend_, journal,
        *incoming_archive_);
  }
  if (const auto* value = request.upgrade())
  {
    if (incoming_archive_ == nullptr)
      throw error(error_code::invalid_effect_session,
                  "upgrade restart lacks incoming archive authority");
    return pkgapply::resume_application(
        *value, state_, lease_, application_backend_, journal,
        *incoming_archive_);
  }
  if (const auto* value = request.removal())
    return pkgapply::resume_application(
        *value, state_, lease_, application_backend_, journal);
  throw error(error_code::invalid_effect_session,
              "application restart request has no operation body");
}

pkgstate::snapshot native_transaction_effect_driver::read_state() const
{
  return state_store_.read();
}

pkgstate::state_publication_receipt
native_transaction_effect_driver::publish_state(
    const pkgstate::state_publication_request& request)
{
  return state_store_.compare_and_publish(request);
}

effectful_operation_result effectful_operation_result::seal(
    effectful_operation_session session,
    effectful_operation_outcome outcome,
    std::vector<pkgapply_exec::lifecycle_execution_result> before,
    std::optional<pkgapply::application_receipt> application,
    std::vector<pkgapply_exec::lifecycle_execution_result> after,
    std::optional<pkgstate::transaction_evidence_identity> transaction_evidence,
    std::optional<pkgstate::state_publication_request> publication_request,
    std::optional<pkgstate::state_publication_receipt> publication_receipt,
    std::optional<pkgstate::installed_state_snapshot_identity> reconciled_state)
{
  session_identity identity = result_identity(
      session, outcome, before, application, after, transaction_evidence,
      publication_request, publication_receipt, reconciled_state);
  return effectful_operation_result(
      std::move(session), outcome, std::move(before), std::move(application),
      std::move(after), std::move(transaction_evidence),
      std::move(publication_request), std::move(publication_receipt),
      std::move(reconciled_state), std::move(identity));
}

effectful_operation_result::effectful_operation_result(
    effectful_operation_session session,
    effectful_operation_outcome outcome,
    std::vector<pkgapply_exec::lifecycle_execution_result> before,
    std::optional<pkgapply::application_receipt> application,
    std::vector<pkgapply_exec::lifecycle_execution_result> after,
    std::optional<pkgstate::transaction_evidence_identity> transaction_evidence,
    std::optional<pkgstate::state_publication_request> publication_request,
    std::optional<pkgstate::state_publication_receipt> publication_receipt,
    std::optional<pkgstate::installed_state_snapshot_identity> reconciled_state,
    session_identity identity)
    : session_(std::move(session)), outcome_(outcome),
      before_(std::move(before)), application_(std::move(application)),
      after_(std::move(after)),
      transaction_evidence_(std::move(transaction_evidence)),
      publication_request_(std::move(publication_request)),
      publication_receipt_(std::move(publication_receipt)),
      reconciled_state_(std::move(reconciled_state)),
      identity_(std::move(identity))
{
}

effectful_operation_outcome effectful_operation_result::outcome() const noexcept
{ return outcome_; }
bool effectful_operation_result::succeeded() const noexcept
{ return outcome_ == effectful_operation_outcome::completed; }
const effectful_operation_session&
effectful_operation_result::session() const noexcept { return session_; }
const std::vector<pkgapply_exec::lifecycle_execution_result>&
effectful_operation_result::before() const noexcept { return before_; }
const std::optional<pkgapply::application_receipt>&
effectful_operation_result::application() const noexcept { return application_; }
const std::vector<pkgapply_exec::lifecycle_execution_result>&
effectful_operation_result::after() const noexcept { return after_; }
const std::optional<pkgstate::transaction_evidence_identity>&
effectful_operation_result::transaction_evidence() const noexcept
{ return transaction_evidence_; }
const std::optional<pkgstate::state_publication_request>&
effectful_operation_result::publication_request() const noexcept
{ return publication_request_; }
const std::optional<pkgstate::state_publication_receipt>&
effectful_operation_result::publication_receipt() const noexcept
{ return publication_receipt_; }
const std::optional<pkgstate::installed_state_snapshot_identity>&
effectful_operation_result::reconciled_state() const noexcept
{ return reconciled_state_; }
const session_identity& effectful_operation_result::identity() const noexcept
{ return identity_; }

effectful_operation_result execute_effectful_operation(
    effectful_operation_session session,
    transaction_effect_driver& driver)
{
  const auto& request = session.request();
  pkgapply::validate_target_mutation_lease(
      request.application().target(), driver.state_projection(), driver.lease());

  std::vector<pkgapply_exec::lifecycle_execution_result> before;
  before.reserve(session.before().size());
  for (const auto& value : session.before())
  {
    if (!driver.lease().held())
      return effectful_operation_result::seal(std::move(session),
                         effectful_operation_outcome::outer_lease_lost,
                         std::move(before), std::nullopt, {}, std::nullopt,
                         std::nullopt, std::nullopt);
    auto result = driver.execute_lifecycle(value);
    validate_lifecycle_result(value, result);
    const bool succeeded = result.succeeded();
    before.push_back(std::move(result));
    if (!succeeded)
      return effectful_operation_result::seal(
          std::move(session),
          effectful_operation_outcome::lifecycle_failed_before_application,
          std::move(before), std::nullopt, {}, std::nullopt, std::nullopt,
          std::nullopt);
  }

  if (!driver.lease().held())
    return effectful_operation_result::seal(std::move(session),
                       effectful_operation_outcome::outer_lease_lost,
                       std::move(before), std::nullopt, {}, std::nullopt,
                       std::nullopt, std::nullopt);

  pkgapply::application_receipt application =
      driver.apply_application(request.application());
  validate_application_receipt(request, driver.state_projection(), application);
  if (application.outcome() != pkgapply::application_attempt_outcome::completed)
    return effectful_operation_result::seal(
        std::move(session),
        effectful_operation_outcome::application_not_completed,
        std::move(before), std::move(application), {}, std::nullopt,
        std::nullopt, std::nullopt);

  std::vector<pkgapply_exec::lifecycle_execution_result> after;
  after.reserve(session.after().size());
  for (const auto& value : session.after())
  {
    if (!driver.lease().held())
      return effectful_operation_result::seal(std::move(session),
                         effectful_operation_outcome::outer_lease_lost,
                         std::move(before), std::move(application),
                         std::move(after), std::nullopt, std::nullopt,
                         std::nullopt);
    auto result = driver.execute_lifecycle(value);
    validate_lifecycle_result(value, result);
    const bool succeeded = result.succeeded();
    after.push_back(std::move(result));
    if (!succeeded)
      return effectful_operation_result::seal(
          std::move(session),
          effectful_operation_outcome::lifecycle_failed_after_application,
          std::move(before), std::move(application), std::move(after),
          std::nullopt, std::nullopt, std::nullopt);
  }

  if (!driver.lease().held())
    return effectful_operation_result::seal(std::move(session),
                       effectful_operation_outcome::outer_lease_lost,
                       std::move(before), std::move(application),
                       std::move(after), std::nullopt, std::nullopt,
                       std::nullopt);

  const auto& completed = *application.completed_evidence();
  auto transaction = transaction_evidence(session, before, completed, after);
  auto publication_request = project_publication(
      request, driver.state_projection(), completed, transaction);
  auto publication_receipt = driver.publish_state(publication_request);
  if (publication_receipt.request() != publication_request.identity())
    throw error(error_code::driver_contract_violation,
                "state driver returned evidence for another publication request");
  if (!driver.lease().held())
    return effectful_operation_result::seal(
        std::move(session), effectful_operation_outcome::outer_lease_lost,
        std::move(before), std::move(application), std::move(after),
        std::move(transaction), std::move(publication_request),
        std::move(publication_receipt));

  effectful_operation_outcome outcome =
      effectful_operation_outcome::state_publication_not_completed;
  switch (publication_receipt.outcome())
  {
    case pkgstate::state_publication_outcome::published:
      outcome = effectful_operation_outcome::completed;
      break;
    case pkgstate::state_publication_outcome::published_durability_unconfirmed:
    case pkgstate::state_publication_outcome::indeterminate:
      outcome = effectful_operation_outcome::state_publication_indeterminate;
      break;
    case pkgstate::state_publication_outcome::stale_expected_state:
    case pkgstate::state_publication_outcome::request_rejected:
    case pkgstate::state_publication_outcome::failed_before_publication:
      break;
  }

  return effectful_operation_result::seal(
      std::move(session), outcome, std::move(before), std::move(application),
      std::move(after), std::move(transaction),
      std::move(publication_request), std::move(publication_receipt));
}

effectful_operation_result execute_effectful_operation_durable(
    effectful_operation_session session,
    const effect_attempt_nonce& nonce,
    transaction_effect_driver& driver,
    effect_journal_store& journal_store)
{
  const auto& request = session.request();
  pkgapply::validate_target_mutation_lease(
      request.application().target(), driver.state_projection(), driver.lease());

  effect_attempt_record journal = append_record(
      journal_store,
      effect_attempt_record::admit(session.identity(), session.before().size(),
                                   session.after().size(), nonce));

  std::vector<pkgapply_exec::lifecycle_execution_result> before;
  before.reserve(session.before().size());
  for (std::size_t index = 0; index < session.before().size(); ++index)
  {
    if (!driver.lease().held())
    {
      journal = append_record(
          journal_store,
          journal.seal_terminal(effectful_operation_outcome::outer_lease_lost));
      return effectful_operation_result::seal(
          std::move(session), effectful_operation_outcome::outer_lease_lost,
          std::move(before), std::nullopt, {}, std::nullopt, std::nullopt,
          std::nullopt);
    }
    journal = append_record(journal_store, journal.begin_before(index));
    auto result = driver.execute_lifecycle(session.before()[index]);
    validate_lifecycle_result(session.before()[index], result);
    const bool succeeded = result.succeeded();
    before.push_back(result);
    journal = append_record(journal_store, journal.complete_before(result));
    if (!succeeded)
    {
      journal = append_record(
          journal_store,
          journal.seal_terminal(
              effectful_operation_outcome::lifecycle_failed_before_application));
      return effectful_operation_result::seal(
          std::move(session),
          effectful_operation_outcome::lifecycle_failed_before_application,
          std::move(before), std::nullopt, {}, std::nullopt, std::nullopt,
          std::nullopt);
    }
  }

  if (!driver.lease().held())
  {
    journal = append_record(
        journal_store,
        journal.seal_terminal(effectful_operation_outcome::outer_lease_lost));
    return effectful_operation_result::seal(
        std::move(session), effectful_operation_outcome::outer_lease_lost,
        std::move(before), std::nullopt, {}, std::nullopt, std::nullopt,
        std::nullopt);
  }

  journal = append_record(journal_store, journal.begin_application());
  pkgapply::application_receipt application =
      driver.apply_application(request.application());
  validate_application_receipt(request, driver.state_projection(), application);
  journal = append_record(journal_store,
                          journal.complete_application(application));
  if (application.outcome() != pkgapply::application_attempt_outcome::completed)
  {
    journal = append_record(
        journal_store,
        journal.seal_terminal(
            effectful_operation_outcome::application_not_completed));
    return effectful_operation_result::seal(
        std::move(session),
        effectful_operation_outcome::application_not_completed,
        std::move(before), std::move(application), {}, std::nullopt,
        std::nullopt, std::nullopt);
  }

  std::vector<pkgapply_exec::lifecycle_execution_result> after;
  after.reserve(session.after().size());
  for (std::size_t index = 0; index < session.after().size(); ++index)
  {
    if (!driver.lease().held())
    {
      journal = append_record(
          journal_store,
          journal.seal_terminal(effectful_operation_outcome::outer_lease_lost));
      return effectful_operation_result::seal(
          std::move(session), effectful_operation_outcome::outer_lease_lost,
          std::move(before), std::move(application), std::move(after),
          std::nullopt, std::nullopt, std::nullopt);
    }
    journal = append_record(journal_store, journal.begin_after(index));
    auto result = driver.execute_lifecycle(session.after()[index]);
    validate_lifecycle_result(session.after()[index], result);
    const bool succeeded = result.succeeded();
    after.push_back(result);
    journal = append_record(journal_store, journal.complete_after(result));
    if (!succeeded)
    {
      journal = append_record(
          journal_store,
          journal.seal_terminal(
              effectful_operation_outcome::lifecycle_failed_after_application));
      return effectful_operation_result::seal(
          std::move(session),
          effectful_operation_outcome::lifecycle_failed_after_application,
          std::move(before), std::move(application), std::move(after),
          std::nullopt, std::nullopt, std::nullopt);
    }
  }

  if (!driver.lease().held())
  {
    journal = append_record(
        journal_store,
        journal.seal_terminal(effectful_operation_outcome::outer_lease_lost));
    return effectful_operation_result::seal(
        std::move(session), effectful_operation_outcome::outer_lease_lost,
        std::move(before), std::move(application), std::move(after),
        std::nullopt, std::nullopt, std::nullopt);
  }

  const auto& completed = *application.completed_evidence();
  auto transaction = transaction_evidence(session, before, completed, after);
  auto publication_request = project_publication(
      request, driver.state_projection(), completed, transaction);
  journal = append_record(
      journal_store, journal.begin_publication(transaction, publication_request));
  auto publication_receipt = driver.publish_state(publication_request);
  if (publication_receipt.request() != publication_request.identity())
    throw error(error_code::driver_contract_violation,
                "state driver returned evidence for another publication request");
  journal = append_record(
      journal_store, journal.complete_publication(publication_receipt));

  if (!driver.lease().held())
  {
    journal = append_record(
        journal_store,
        journal.seal_terminal(effectful_operation_outcome::outer_lease_lost));
    return effectful_operation_result::seal(
        std::move(session), effectful_operation_outcome::outer_lease_lost,
        std::move(before), std::move(application), std::move(after),
        std::move(transaction), std::move(publication_request),
        std::move(publication_receipt));
  }

  const auto outcome = publication_outcome(publication_receipt);
  if (outcome != effectful_operation_outcome::state_publication_indeterminate)
    journal = append_record(journal_store, journal.seal_terminal(outcome));
  return effectful_operation_result::seal(
      std::move(session), outcome, std::move(before), std::move(application),
      std::move(after), std::move(transaction),
      std::move(publication_request), std::move(publication_receipt));
}

effect_restart_result resume_effectful_operation(
    effect_restart_checkpoint checkpoint,
    transaction_effect_driver& driver,
    effect_journal_store& journal_store)
{
  effect_attempt_record journal = checkpoint.record();
  const auto latest = journal_store.load_latest(journal.attempt());
  if (!latest || latest->identity() != journal.identity())
    throw error(error_code::invalid_effect_session,
                "restart checkpoint is not the latest durable controller record");

  auto session = checkpoint.session();
  auto before = checkpoint.before();
  auto application = checkpoint.application();
  auto after = checkpoint.after();
  auto publication_request = checkpoint.publication_request();
  auto publication_receipt = checkpoint.publication_receipt();

  const auto current_transaction = [&]()
      -> std::optional<pkgstate::transaction_evidence_identity> {
    if (publication_request && publication_request->transaction_evidence())
      return *publication_request->transaction_evidence();
    if (journal.transaction_evidence())
      return transaction_identity_from_string(*journal.transaction_evidence());
    return std::nullopt;
  };

  auto finish = [&](effectful_operation_outcome outcome,
                          std::optional<pkgstate::installed_state_snapshot_identity>
                              reconciled = std::nullopt) {
    journal = append_record(
        journal_store, journal.seal_terminal(outcome, reconciled));
    auto operation = effectful_operation_result::seal(
        session, outcome, before, application, after, current_transaction(),
        publication_request, publication_receipt, reconciled);
    return effect_restart_result(
        effect_restart_disposition::terminal, journal, std::move(operation));
  };

  auto assessment = assess_effect_restart(journal);
  if (assessment.disposition() ==
      effect_restart_disposition::external_resolution_required)
    return effect_restart_result(assessment.disposition(), journal, std::nullopt);
  if (assessment.disposition() ==
          effect_restart_disposition::resume_application &&
      !checkpoint.application_journal())
    return effect_restart_result(
        effect_restart_disposition::external_resolution_required,
        journal, std::nullopt);

  if (assessment.disposition() == effect_restart_disposition::terminal)
  {
    std::optional<pkgstate::installed_state_snapshot_identity> reconciled;
    if (journal.reconciled_state())
      reconciled = state_identity_from_string(*journal.reconciled_state());
    auto operation = effectful_operation_result::seal(
        std::move(session), *journal.terminal_outcome(), std::move(before),
        std::move(application), std::move(after), current_transaction(),
        std::move(publication_request), std::move(publication_receipt),
        std::move(reconciled));
    return effect_restart_result(
        effect_restart_disposition::terminal, std::move(journal),
        std::move(operation));
  }

  if (assessment.disposition() == effect_restart_disposition::seal_terminal)
  {
    switch (journal.stage())
    {
      case effect_attempt_stage::before_lifecycle_terminal:
        return finish(
            effectful_operation_outcome::lifecycle_failed_before_application);
      case effect_attempt_stage::application_terminal:
        return finish(effectful_operation_outcome::application_not_completed);
      case effect_attempt_stage::after_lifecycle_terminal:
        return finish(
            effectful_operation_outcome::lifecycle_failed_after_application);
      case effect_attempt_stage::publication_terminal:
        return finish(publication_outcome(*publication_receipt));
      default:
        throw error(error_code::invalid_effect_session,
                    "restart terminal disposition has no terminal evidence");
    }
  }

  const auto& request = session.request();
  if (!driver.lease().held())
    throw error(error_code::invalid_effect_session,
                "effect restart requires a newly held target lease");

  if (assessment.disposition() ==
      effect_restart_disposition::reconcile_publication)
  {
    if (!publication_request)
      throw error(error_code::invalid_effect_session,
                  "publication reconciliation lacks the exact request");
    const auto observed = driver.read_state();
    const auto& expected = request.transaction().resolution().installed();
    const auto resulting = resulting_snapshot(expected, *publication_request);
    if (observed.identity() == resulting.identity())
      return finish(effectful_operation_outcome::completed,
                    observed.identity());
    if (observed.identity() != expected.identity() ||
        journal.stage() == effect_attempt_stage::publication_terminal)
    {
      return effect_restart_result(
          effect_restart_disposition::external_resolution_required,
          journal, std::nullopt);
    }
    pkgapply::validate_target_mutation_lease(
        request.application().target(), driver.state_projection(),
        driver.lease());
    publication_receipt = driver.publish_state(*publication_request);
    if (publication_receipt->request() != publication_request->identity())
      throw error(error_code::driver_contract_violation,
                  "state driver returned evidence for another publication request");
    journal = append_record(
        journal_store, journal.complete_publication(*publication_receipt));
    const auto outcome = publication_outcome(*publication_receipt);
    if (outcome == effectful_operation_outcome::state_publication_indeterminate)
      return effect_restart_result(
          effect_restart_disposition::reconcile_publication, journal,
          effectful_operation_result::seal(
              session, outcome, before, application, after,
              current_transaction(), publication_request,
              publication_receipt));
    return finish(outcome);
  }

  pkgapply::validate_target_mutation_lease(
      request.application().target(), driver.state_projection(), driver.lease());

  if (assessment.disposition() ==
          effect_restart_disposition::continue_before_lifecycle ||
      journal.stage() == effect_attempt_stage::admitted ||
      journal.stage() == effect_attempt_stage::before_lifecycle_terminal)
  {
    for (std::size_t index = before.size(); index < session.before().size(); ++index)
    {
      if (!driver.lease().held())
        return finish(effectful_operation_outcome::outer_lease_lost);
      journal = append_record(journal_store, journal.begin_before(index));
      auto result = driver.execute_lifecycle(session.before()[index]);
      validate_lifecycle_result(session.before()[index], result);
      const bool succeeded = result.succeeded();
      before.push_back(result);
      journal = append_record(journal_store, journal.complete_before(result));
      if (!succeeded)
        return finish(
            effectful_operation_outcome::lifecycle_failed_before_application);
    }
  }

  if (!application)
  {
    if (!driver.lease().held())
      return finish(effectful_operation_outcome::outer_lease_lost);
    if (journal.stage() != effect_attempt_stage::application_intent)
      journal = append_record(journal_store, journal.begin_application());
    if (assessment.disposition() == effect_restart_disposition::resume_application)
    {
      application = driver.resume_application(
          request.application(), *checkpoint.application_journal());
    }
    else
      application = driver.apply_application(request.application());
    validate_application_receipt(
        request, driver.state_projection(), *application);
    journal = append_record(
        journal_store, journal.complete_application(*application));
    if (application->outcome() !=
        pkgapply::application_attempt_outcome::completed)
      return finish(effectful_operation_outcome::application_not_completed);
  }

  for (std::size_t index = after.size(); index < session.after().size(); ++index)
  {
    if (!driver.lease().held())
      return finish(effectful_operation_outcome::outer_lease_lost);
    journal = append_record(journal_store, journal.begin_after(index));
    auto result = driver.execute_lifecycle(session.after()[index]);
    validate_lifecycle_result(session.after()[index], result);
    const bool succeeded = result.succeeded();
    after.push_back(result);
    journal = append_record(journal_store, journal.complete_after(result));
    if (!succeeded)
      return finish(
          effectful_operation_outcome::lifecycle_failed_after_application);
  }

  if (!driver.lease().held())
    return finish(effectful_operation_outcome::outer_lease_lost);

  if (!publication_request)
  {
    const auto& completed = *application->completed_evidence();
    const auto transaction = transaction_evidence(
        session, before, completed, after);
    publication_request = project_publication(
        request, driver.state_projection(), completed, transaction);
    journal = append_record(
        journal_store,
        journal.begin_publication(transaction, *publication_request));
  }
  publication_receipt = driver.publish_state(*publication_request);
  if (publication_receipt->request() != publication_request->identity())
    throw error(error_code::driver_contract_violation,
                "state driver returned evidence for another publication request");
  journal = append_record(
      journal_store, journal.complete_publication(*publication_receipt));
  if (!driver.lease().held())
    return finish(effectful_operation_outcome::outer_lease_lost);
  const auto outcome = publication_outcome(*publication_receipt);
  if (outcome == effectful_operation_outcome::state_publication_indeterminate)
  {
    auto operation = effectful_operation_result::seal(
        session, outcome, before, application, after, current_transaction(),
        publication_request, publication_receipt);
    return effect_restart_result(
        effect_restart_disposition::reconcile_publication, journal,
        std::move(operation));
  }
  return finish(outcome);
}

} // namespace pkgctl
