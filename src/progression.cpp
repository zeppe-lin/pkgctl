// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/progression.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

using node_id = pkgtransaction::transaction_node_identity;

struct unit_spec final {
  transaction_unit_kind kind;
  node_id primary;
  std::vector<node_id> members;
};

bool target_action(pkgtransaction::transaction_action_kind action) noexcept
{
  return action == pkgtransaction::transaction_action_kind::install ||
         action == pkgtransaction::transaction_action_kind::upgrade ||
         action == pkgtransaction::transaction_action_kind::remove;
}

std::vector<node_id> lifecycle_members(
    const pkgtransaction::transaction_program& program,
    const node_id& action)
{
  std::vector<node_id> result{action};
  for (const auto& edge : program.edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::phase ||
        !edge.phase_order())
      continue;
    if (*edge.phase_order() ==
            pkgtransaction::phase_order_kind::pre_lifecycle_before_action &&
        edge.after() == action)
      result.push_back(edge.before());
    if (*edge.phase_order() ==
            pkgtransaction::phase_order_kind::action_before_post_lifecycle &&
        edge.before() == action)
      result.push_back(edge.after());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

std::vector<unit_spec> units(const transaction_session& transaction)
{
  const auto& program = transaction.program();
  std::vector<unit_spec> result;
  std::map<std::string, std::string> lifecycle_owner;

  for (const auto& node : program.nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build)
      result.push_back({transaction_unit_kind::construction,
                        node.identity(), {node.identity()}});
    else if (node.action() == pkgtransaction::transaction_action_kind::check)
      result.push_back({transaction_unit_kind::check,
                        node.identity(), {node.identity()}});
    else if (target_action(node.action()))
    {
      auto members = lifecycle_members(program, node.identity());
      for (const auto& member : members)
      {
        if (member == node.identity())
          continue;
        const auto* lifecycle = program.find(member);
        if (lifecycle == nullptr ||
            lifecycle->action() !=
                pkgtransaction::transaction_action_kind::lifecycle)
          throw error(error_code::invalid_progression,
                      "operation unit contains a non-lifecycle phase member");
        const auto inserted = lifecycle_owner.emplace(
            member.hex(), node.identity().hex());
        if (!inserted.second && inserted.first->second != node.identity().hex())
          throw error(error_code::invalid_progression,
                      "lifecycle node belongs to multiple target operations");
      }
      result.push_back({transaction_unit_kind::operation,
                        node.identity(), std::move(members)});
    }
  }

  for (const auto& node : program.nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::lifecycle &&
        lifecycle_owner.find(node.identity().hex()) == lifecycle_owner.end())
      throw error(error_code::invalid_progression,
                  "transaction lifecycle node has no target operation unit");
  }

  std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.primary < rhs.primary;
  });
  return result;
}

bool contains(const std::vector<node_id>& values, const node_id& value)
{
  return std::binary_search(values.begin(), values.end(), value);
}

const construction_result* find_construction(
    const std::vector<construction_result>& values,
    const node_id& node)
{
  const auto found = std::find_if(
      values.begin(), values.end(), [&](const auto& value) {
        return value.session().request().build_node() == node;
      });
  return found == values.end() ? nullptr : &*found;
}

const transaction_check_result* find_check(
    const std::vector<transaction_check_result>& values,
    const node_id& node)
{
  const auto found = std::find_if(
      values.begin(), values.end(), [&](const auto& value) {
        return value.session().request().check_node() == node;
      });
  return found == values.end() ? nullptr : &*found;
}

const effectful_operation_result* find_effect(
    const std::vector<effectful_operation_result>& values,
    const node_id& node)
{
  const auto found = std::find_if(
      values.begin(), values.end(), [&](const auto& value) {
        return value.session().request().action_node() == node;
      });
  return found == values.end() ? nullptr : &*found;
}

void apply_lifecycle_results(
    std::map<std::string, transaction_node_status>& state,
    const std::vector<node_id>& order,
    const std::vector<pkgapply_exec::lifecycle_execution_result>& results)
{
  for (std::size_t index = 0; index < order.size(); ++index)
  {
    if (index >= results.size())
    {
      state[order[index].hex()] = transaction_node_status::blocked;
      continue;
    }
    state[order[index].hex()] = results[index].succeeded()
        ? transaction_node_status::satisfied
        : transaction_node_status::failed;
  }
}

std::map<std::string, transaction_node_status> derive_statuses(
    const transaction_session& transaction,
    const std::vector<construction_result>& constructions,
    const std::vector<transaction_check_result>& checks,
    const std::vector<effectful_operation_result>& effects,
    const std::vector<unit_spec>& unit_specs)
{
  std::map<std::string, transaction_node_status> state;
  for (const auto& node : transaction.program().nodes())
  {
    state.emplace(
        node.identity().hex(),
        node.action() == pkgtransaction::transaction_action_kind::retain
            ? transaction_node_status::satisfied
            : transaction_node_status::pending);
  }

  for (const auto& unit : unit_specs)
  {
    if (unit.kind == transaction_unit_kind::construction)
    {
      const auto* evidence = find_construction(constructions, unit.primary);
      if (evidence != nullptr)
        state[unit.primary.hex()] = evidence->succeeded()
            ? transaction_node_status::satisfied
            : transaction_node_status::failed;
    }
    else if (unit.kind == transaction_unit_kind::check)
    {
      const auto* evidence = find_check(checks, unit.primary);
      if (evidence != nullptr)
        state[unit.primary.hex()] = evidence->succeeded()
            ? transaction_node_status::satisfied
            : transaction_node_status::failed;
    }
    else if (unit.kind == transaction_unit_kind::operation)
    {
      const auto* evidence = find_effect(effects, unit.primary);
      if (evidence != nullptr)
      {
        if (evidence->succeeded())
        {
          for (const auto& member : unit.members)
            state[member.hex()] = transaction_node_status::satisfied;
        }
        else
        {
          for (const auto& member : unit.members)
            state[member.hex()] = transaction_node_status::blocked;
          state[unit.primary.hex()] = transaction_node_status::failed;
          const auto& lifecycle = evidence->session().request().lifecycle();
          apply_lifecycle_results(state, lifecycle.before(), evidence->before());
          apply_lifecycle_results(state, lifecycle.after(), evidence->after());
        }
      }
    }
  }

  bool changed = true;
  while (changed)
  {
    changed = false;
    for (const auto& unit : unit_specs)
    {
      const auto primary_status = state.at(unit.primary.hex());
      if (primary_status == transaction_node_status::satisfied ||
          primary_status == transaction_node_status::failed)
        continue;

      bool all_satisfied = true;
      bool dependency_failed = false;
      for (const auto& edge : transaction.program().edges())
      {
        if (!contains(unit.members, edge.after()) ||
            contains(unit.members, edge.before()))
          continue;
        const auto predecessor = state.at(edge.before().hex());
        if (predecessor == transaction_node_status::failed ||
            predecessor == transaction_node_status::blocked)
          dependency_failed = true;
        if (predecessor != transaction_node_status::satisfied)
          all_satisfied = false;
      }

      const auto derived = dependency_failed
          ? transaction_node_status::blocked
          : (all_satisfied ? transaction_node_status::ready
                           : transaction_node_status::pending);
      for (const auto& member : unit.members)
      {
        auto& current = state[member.hex()];
        if (current != derived)
        {
          current = derived;
          changed = true;
        }
      }
    }
  }
  return state;
}

session_identity ready_unit_identity(
    const session_identity& transaction,
    const unit_spec& unit)
{
  std::vector<std::string> fields{
      transaction.hex(),
      std::to_string(static_cast<unsigned int>(unit.kind)),
      unit.primary.hex(), std::to_string(unit.members.size())};
  for (const auto& member : unit.members)
    fields.push_back(member.hex());
  return make_session_identity("pkgctl/transaction-ready-unit/1", fields);
}


bool definitive_failure(effectful_operation_outcome outcome) noexcept
{
  return outcome == effectful_operation_outcome::lifecycle_failed_before_application ||
         outcome == effectful_operation_outcome::application_not_completed ||
         outcome == effectful_operation_outcome::lifecycle_failed_after_application ||
         outcome == effectful_operation_outcome::state_publication_not_completed;
}

void validate_effect_success(
    const transaction_progress& progress,
    const effectful_operation_result& effect,
    const pkgstate::snapshot& resulting_state)
{
  if (!effect.publication_request())
    throw error(error_code::invalid_progression,
                "completed effect lacks its exact publication request");
  const auto& request = *effect.publication_request();
  if (request.expected_snapshot() != progress.current_state().identity() ||
      request.target_binding() != progress.current_state().target_binding() ||
      resulting_state.target_binding() != progress.current_state().target_binding())
    throw error(error_code::invalid_progression,
                "effect publication authority is not based on the current epoch");

  if (effect.publication_receipt())
  {
    const auto& receipt = *effect.publication_receipt();
    if (receipt.request() != request.identity() ||
        receipt.expected_prior_snapshot() != progress.current_state().identity() ||
        receipt.actual_prior_snapshot() != progress.current_state().identity() ||
        receipt.target_binding() != progress.current_state().target_binding() ||
        !receipt.resulting_snapshot() ||
        *receipt.resulting_snapshot() != resulting_state.identity())
      throw error(error_code::invalid_progression,
                  "publication receipt does not prove the claimed state epoch");
  }
  else if (!effect.reconciled_state() ||
           *effect.reconciled_state() != resulting_state.identity())
  {
    throw error(error_code::invalid_progression,
                "receipt-free completion lacks authoritative reconciliation");
  }

  if (effect.reconciled_state() &&
      *effect.reconciled_state() != resulting_state.identity())
    throw error(error_code::invalid_progression,
                "reconciled state differs from the supplied current epoch");
}

} // namespace

transaction_progress transaction_progress::rebuild(
    transaction_session transaction,
    pkgstate::snapshot current_state,
    std::vector<construction_result> constructions,
    std::vector<transaction_check_result> checks,
    std::vector<effectful_operation_result> effects)
{
  const auto unit_specs = units(transaction);
  const auto status = derive_statuses(
      transaction, constructions, checks, effects, unit_specs);

  std::vector<transaction_progress::node_record> nodes;
  nodes.reserve(transaction.program().nodes().size());
  for (const auto& node : transaction.program().nodes())
    nodes.push_back({node.identity(), status.at(node.identity().hex())});

  std::vector<ready_transaction_unit> ready;
  for (const auto& unit : unit_specs)
  {
    if (status.at(unit.primary.hex()) != transaction_node_status::ready)
      continue;
    ready.push_back(ready_transaction_unit(
        unit.kind, unit.primary, unit.members,
        ready_unit_identity(transaction.identity(), unit)));
  }

  std::vector<std::string> identity_fields{
      transaction.identity().hex(), current_state.identity().string(),
      std::to_string(constructions.size())};
  for (const auto& value : constructions)
  {
    identity_fields.push_back(
        value.session().request().build_node().hex());
    identity_fields.push_back(value.identity().hex());
  }
  identity_fields.push_back(std::to_string(checks.size()));
  for (const auto& value : checks)
  {
    identity_fields.push_back(value.session().request().check_node().hex());
    identity_fields.push_back(value.identity().hex());
  }
  identity_fields.push_back(std::to_string(effects.size()));
  for (const auto& value : effects)
  {
    identity_fields.push_back(value.session().request().action_node().hex());
    identity_fields.push_back(value.identity().hex());
  }
  identity_fields.push_back(std::to_string(nodes.size()));
  for (const auto& value : nodes)
  {
    identity_fields.push_back(value.node.hex());
    identity_fields.push_back(
        std::to_string(static_cast<unsigned int>(value.status)));
  }
  identity_fields.push_back(std::to_string(ready.size()));
  for (const auto& value : ready)
    identity_fields.push_back(value.identity().hex());
  auto identity = make_session_identity(
      "pkgctl/transaction-progress/1", identity_fields);
  return transaction_progress(
      std::move(transaction), std::move(current_state),
      std::move(constructions), std::move(checks), std::move(effects),
      std::move(nodes), std::move(ready), std::move(identity));
}

ready_transaction_unit ready_transaction_unit::restore(
    const session_identity& transaction,
    transaction_unit_kind kind,
    pkgtransaction::transaction_node_identity primary_node,
    std::vector<pkgtransaction::transaction_node_identity> members,
    const session_identity& expected_identity)
{
  if (members.empty() ||
      !std::binary_search(members.begin(), members.end(), primary_node) ||
      !std::is_sorted(members.begin(), members.end()) ||
      std::adjacent_find(members.begin(), members.end()) != members.end())
    throw error(error_code::invalid_transaction_run,
                "durable ready unit has invalid canonical members");
  if ((kind == transaction_unit_kind::construction ||
       kind == transaction_unit_kind::check) &&
      (members.size() != 1U || members.front() != primary_node))
    throw error(error_code::invalid_transaction_run,
                "durable non-operation unit does not contain one exact node");

  unit_spec specification{
      kind,
      primary_node,
      members,
  };
  const auto identity = ready_unit_identity(transaction, specification);
  if (identity != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable ready-unit identity does not match its fields");
  return ready_transaction_unit(
      kind, std::move(primary_node), std::move(members), identity);
}

ready_transaction_unit::ready_transaction_unit(
    transaction_unit_kind kind,
    pkgtransaction::transaction_node_identity primary_node,
    std::vector<pkgtransaction::transaction_node_identity> members,
    session_identity identity)
    : kind_(kind), primary_node_(std::move(primary_node)),
      members_(std::move(members)), identity_(std::move(identity))
{
}

transaction_unit_kind ready_transaction_unit::kind() const noexcept
{ return kind_; }
const pkgtransaction::transaction_node_identity&
ready_transaction_unit::primary_node() const noexcept { return primary_node_; }
const std::vector<pkgtransaction::transaction_node_identity>&
ready_transaction_unit::members() const noexcept { return members_; }
const session_identity& ready_transaction_unit::identity() const noexcept
{ return identity_; }

transaction_progress::transaction_progress(
    transaction_session transaction,
    pkgstate::snapshot current_state,
    std::vector<construction_result> constructions,
    std::vector<transaction_check_result> checks,
    std::vector<effectful_operation_result> effects,
    std::vector<node_record> nodes,
    std::vector<ready_transaction_unit> ready_units,
    session_identity identity)
    : transaction_(std::move(transaction)),
      current_state_(std::move(current_state)),
      constructions_(std::move(constructions)), checks_(std::move(checks)),
      effects_(std::move(effects)), nodes_(std::move(nodes)),
      ready_units_(std::move(ready_units)), identity_(std::move(identity))
{
}

transaction_progress transaction_progress::begin(transaction_session transaction)
{
  auto initial_state = transaction.resolution().installed();
  return transaction_progress::rebuild(
      std::move(transaction), std::move(initial_state), {}, {}, {});
}

const transaction_session& transaction_progress::transaction() const noexcept
{ return transaction_; }
const pkgstate::snapshot& transaction_progress::current_state() const noexcept
{ return current_state_; }
const std::vector<construction_result>&
transaction_progress::constructions() const noexcept { return constructions_; }
const std::vector<transaction_check_result>&
transaction_progress::checks() const noexcept { return checks_; }
const std::vector<effectful_operation_result>&
transaction_progress::effects() const noexcept { return effects_; }

transaction_node_status transaction_progress::status(const node_id& node) const
{
  const auto found = std::find_if(
      nodes_.begin(), nodes_.end(), [&](const auto& value) {
        return value.node == node;
      });
  if (found == nodes_.end())
    throw error(error_code::invalid_progression,
                "node is absent from the transaction progression");
  return found->status;
}

std::vector<node_id> transaction_progress::nodes(
    transaction_node_status wanted) const
{
  std::vector<node_id> result;
  for (const auto& value : nodes_)
    if (value.status == wanted)
      result.push_back(value.node);
  return result;
}

const std::vector<ready_transaction_unit>&
transaction_progress::ready_units() const noexcept { return ready_units_; }

bool transaction_progress::contains_unit(
    const ready_transaction_unit& unit) const
{
  const auto unit_specs = units(transaction_);
  const auto found = std::find_if(
      unit_specs.begin(), unit_specs.end(), [&](const auto& candidate) {
        return candidate.kind == unit.kind() &&
            candidate.primary == unit.primary_node() &&
            candidate.members == unit.members();
      });
  return found != unit_specs.end() &&
      ready_unit_identity(transaction_.identity(), *found) == unit.identity();
}

const construction_result* transaction_progress::construction(
    const node_id& node) const noexcept
{ return find_construction(constructions_, node); }
const transaction_check_result* transaction_progress::check(
    const node_id& node) const noexcept
{ return find_check(checks_, node); }
const effectful_operation_result* transaction_progress::effect(
    const node_id& node) const noexcept
{ return find_effect(effects_, node); }

bool transaction_progress::complete() const noexcept
{
  return std::all_of(nodes_.begin(), nodes_.end(), [](const auto& value) {
    return value.status == transaction_node_status::satisfied;
  });
}
bool transaction_progress::failed() const noexcept
{
  return std::any_of(nodes_.begin(), nodes_.end(), [](const auto& value) {
    return value.status == transaction_node_status::failed;
  });
}
const session_identity& transaction_progress::identity() const noexcept
{ return identity_; }

transaction_progress advance_construction(
    transaction_progress progress,
    construction_result construction)
{
  const auto& request = construction.session().request();
  if (request.transaction().identity() != progress.transaction().identity())
    throw error(error_code::invalid_progression,
                "construction evidence belongs to another transaction");
  if (progress.status(request.build_node()) != transaction_node_status::ready)
    throw error(error_code::invalid_progression,
                "construction evidence does not belong to a ready build node");
  if (progress.construction(request.build_node()) != nullptr)
    throw error(error_code::invalid_progression,
                "build node already has terminal construction evidence");

  auto constructions = progress.constructions();
  constructions.push_back(std::move(construction));
  std::sort(constructions.begin(), constructions.end(), [](const auto& lhs,
                                                            const auto& rhs) {
    return lhs.session().request().build_node() <
           rhs.session().request().build_node();
  });
  return transaction_progress::rebuild(
      progress.transaction(), progress.current_state(),
      std::move(constructions), progress.checks(), progress.effects());
}

transaction_progress advance_check(
    transaction_progress progress,
    transaction_check_result check)
{
  const auto& session = check.session();
  const auto& request = session.request();
  const auto& check_authority = request.check();
  const auto& check_node = request.check_node();

  if (request.transaction().identity() != progress.transaction().identity())
    throw error(error_code::invalid_progression,
                "check evidence belongs to another transaction");
  if (check_authority.transaction() !=
      progress.transaction().program().identity())
    throw error(error_code::invalid_progression,
                "check request names another transaction program");
  if (check_authority.check_node().identity() != check_node)
    throw error(error_code::invalid_progression,
                "check result contradicts its controller request node");
  if (check.execution().check().request().identity() !=
      check_authority.identity())
    throw error(error_code::invalid_progression,
                "check result belongs to another check request");
  if (check.execution().execution().request().identity() !=
      session.execution_request())
    throw error(error_code::invalid_progression,
                "check result belongs to another execution request");
  if (progress.status(check_node) != transaction_node_status::ready)
    throw error(error_code::invalid_progression,
                "check evidence does not belong to a ready check node");
  if (progress.check(check_node) != nullptr)
    throw error(error_code::invalid_progression,
                "check node already has terminal execution evidence");

  const auto& build_node = check_authority.build_node().identity();
  const auto* construction = progress.construction(build_node);
  if (construction == nullptr || !construction->succeeded() ||
      construction->identity() != request.construction().identity() ||
      construction->build().build().identity() !=
          check_authority.build().identity())
    throw error(error_code::invalid_progression,
                "check evidence is not bound to the retained construction");

  auto checks = progress.checks();
  checks.push_back(std::move(check));
  std::sort(checks.begin(), checks.end(), [](const auto& lhs,
                                             const auto& rhs) {
    return lhs.session().request().check_node() <
           rhs.session().request().check_node();
  });
  return transaction_progress::rebuild(
      progress.transaction(), progress.current_state(),
      progress.constructions(), std::move(checks), progress.effects());
}

transaction_progress advance_effect(
    transaction_progress progress,
    effectful_operation_result effect,
    std::optional<pkgstate::snapshot> resulting_state)
{
  const auto& request = effect.session().request();
  if (request.transaction().identity() != progress.transaction().identity())
    throw error(error_code::invalid_progression,
                "effect evidence belongs to another transaction");
  if (request.expected_state().identity() != progress.current_state().identity() ||
      request.expected_state().target_binding() !=
          progress.current_state().target_binding())
    throw error(error_code::invalid_progression,
                "effect evidence was attempted against a stale state epoch");
  if (progress.status(request.action_node()) != transaction_node_status::ready)
    throw error(error_code::invalid_progression,
                "effect evidence does not belong to a ready operation unit");
  if (progress.effect(request.action_node()) != nullptr)
    throw error(error_code::invalid_progression,
                "operation unit already has terminal effect evidence");

  auto next_state = progress.current_state();
  if (effect.succeeded())
  {
    if (!resulting_state)
      throw error(error_code::invalid_progression,
                  "completed effect requires the resulting canonical state");
    validate_effect_success(progress, effect, *resulting_state);
    next_state = std::move(*resulting_state);
  }
  else
  {
    if (resulting_state)
      throw error(error_code::invalid_progression,
                  "failed effect cannot advance the canonical state epoch");
    if (!definitive_failure(effect.outcome()))
      throw error(error_code::invalid_progression,
                  "indeterminate effect is not terminal progression evidence");
  }

  auto effects = progress.effects();
  effects.push_back(std::move(effect));
  std::sort(effects.begin(), effects.end(), [](const auto& lhs,
                                               const auto& rhs) {
    return lhs.session().request().action_node() <
           rhs.session().request().action_node();
  });
  return transaction_progress::rebuild(
      progress.transaction(), std::move(next_state),
      progress.constructions(), progress.checks(), std::move(effects));
}

} // namespace pkgctl
