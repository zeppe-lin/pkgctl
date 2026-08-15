// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/dispatch.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

using node_identity = pkgtransaction::transaction_node_identity;

bool operation_outcome_is_terminal(
    effectful_operation_outcome outcome) noexcept
{
  switch (outcome)
  {
    case effectful_operation_outcome::lifecycle_failed_before_application:
    case effectful_operation_outcome::application_not_completed:
    case effectful_operation_outcome::lifecycle_failed_after_application:
    case effectful_operation_outcome::state_publication_not_completed:
    case effectful_operation_outcome::completed:
      return true;
    case effectful_operation_outcome::outer_lease_lost:
    case effectful_operation_outcome::state_publication_indeterminate:
      return false;
  }
  return false;
}

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  throw error(error_code::invalid_dispatch,
              "dispatch nonce contains invalid hexadecimal data");
}

std::string hexadecimal(
    const transaction_dispatch_nonce::byte_array& bytes)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2U);
  for (const std::uint8_t byte : bytes)
  {
    result.push_back(digits[(byte >> 4U) & 0x0fU]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}

bool contains_node(
    const std::vector<node_identity>& nodes,
    const node_identity& wanted)
{
  return std::find(nodes.begin(), nodes.end(), wanted) != nodes.end();
}

const ready_transaction_unit* find_ready_unit(
    const transaction_progress& progress,
    const ready_transaction_unit& unit)
{
  const auto found = std::find_if(
      progress.ready_units().begin(),
      progress.ready_units().end(),
      [&](const ready_transaction_unit& candidate) {
        return candidate.identity() == unit.identity();
      });
  if (found == progress.ready_units().end())
    return nullptr;
  if (found->kind() != unit.kind() ||
      found->primary_node() != unit.primary_node() ||
      found->members() != unit.members())
    return nullptr;
  return &*found;
}

std::optional<session_identity> terminal_evidence_for_node(
    const transaction_progress& progress,
    const node_identity& node)
{
  if (const auto* construction = progress.construction(node))
    return construction->identity();
  if (const auto* check = progress.check(node))
    return check->identity();
  if (const auto* effect = progress.effect(node))
    return effect->identity();

  for (const auto& effect : progress.effects())
  {
    const auto& request = effect.session().request();
    const auto& lifecycle = request.lifecycle();
    if (contains_node(lifecycle.before(), node) ||
        contains_node(lifecycle.after(), node))
      return effect.identity();
  }
  return std::nullopt;
}

std::vector<node_identity> external_predecessors(
    const transaction_progress& progress,
    const ready_transaction_unit& unit)
{
  std::vector<node_identity> result;
  for (const auto& edge : progress.transaction().program().edges())
  {
    if (!contains_node(unit.members(), edge.after()) ||
        contains_node(unit.members(), edge.before()))
      continue;
    result.push_back(edge.before());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

std::vector<transaction_dispatch_dependency> capture_dependencies(
    const transaction_progress& progress,
    const ready_transaction_unit& unit);

bool records_overlap(
    const transaction_dispatch& lhs,
    const transaction_dispatch& rhs)
{
  for (const auto& member : lhs.unit().members())
    if (contains_node(rhs.unit().members(), member))
      return true;
  return false;
}

std::size_t policy_capacity(
    const transaction_dispatch_policy& policy,
    transaction_unit_kind kind)
{
  switch (kind)
  {
    case transaction_unit_kind::construction:
      return policy.construction_capacity();
    case transaction_unit_kind::check:
      return policy.check_capacity();
    case transaction_unit_kind::operation:
      return policy.operation_capacity();
  }
  return 0U;
}

std::string_view failure_containment_name(
    transaction_failure_containment value) noexcept
{
  switch (value)
  {
    case transaction_failure_containment::stop_after_terminal_failure:
      return "stop-after-terminal-failure";
  }
  return "unknown";
}

std::string_view dispatch_state_name(
    transaction_dispatch_state value) noexcept
{
  switch (value)
  {
    case transaction_dispatch_state::reserved:
      return "reserved";
    case transaction_dispatch_state::started:
      return "started";
    case transaction_dispatch_state::completed:
      return "completed";
    case transaction_dispatch_state::released_unstarted:
      return "released-unstarted";
  }
  return "unknown";
}

bool active_state(transaction_dispatch_state state) noexcept
{
  return state == transaction_dispatch_state::reserved ||
         state == transaction_dispatch_state::started;
}

void validate_record_shape(const transaction_dispatch_record& record)
{
  switch (record.state())
  {
    case transaction_dispatch_state::reserved:
      if (record.attempt_session() || record.effect_attempt() ||
          !record.observations().empty() ||
          record.terminal_evidence())
        throw error(error_code::invalid_transaction_run,
                    "reserved dispatch record contains attempt evidence");
      return;

    case transaction_dispatch_state::started:
      if (!record.attempt_session() || record.terminal_evidence())
        throw error(error_code::invalid_transaction_run,
                    "started dispatch record has an invalid evidence shape");
      if ((record.dispatch().unit().kind() == transaction_unit_kind::operation) !=
          record.effect_attempt().has_value())
        throw error(error_code::invalid_transaction_run,
                    "started dispatch has invalid effect-attempt authority");
      if (!record.observations().empty() &&
          record.dispatch().unit().kind() != transaction_unit_kind::operation)
        throw error(error_code::invalid_transaction_run,
                    "only operation dispatches may retain uncertain observations");
      return;

    case transaction_dispatch_state::completed:
      if (!record.attempt_session() || !record.terminal_evidence())
        throw error(error_code::invalid_transaction_run,
                    "completed dispatch record lacks terminal evidence");
      if ((record.dispatch().unit().kind() == transaction_unit_kind::operation) !=
          record.effect_attempt().has_value())
        throw error(error_code::invalid_transaction_run,
                    "completed dispatch has invalid effect-attempt authority");
      if (!record.observations().empty() &&
          record.dispatch().unit().kind() != transaction_unit_kind::operation)
        throw error(error_code::invalid_transaction_run,
                    "non-operation completion contains uncertain observations");
      return;

    case transaction_dispatch_state::released_unstarted:
      if (record.attempt_session() || record.effect_attempt() ||
          !record.observations().empty() ||
          record.terminal_evidence())
        throw error(error_code::invalid_transaction_run,
                    "released dispatch record contains execution evidence");
      return;
  }
}

void validate_run_records(
    const transaction_progress& progress,
    const transaction_dispatch_policy& policy,
    const std::vector<transaction_dispatch_record>& records)
{
  std::set<std::string> nonces;
  std::set<std::string> dispatches;
  std::vector<const transaction_dispatch_record*> active;

  for (const auto& record : records)
  {
    validate_record_shape(record);
    const auto& dispatch = record.dispatch();
    if (!nonces.insert(dispatch.nonce().hex()).second)
      throw error(error_code::invalid_transaction_run,
                  "transaction run reuses a dispatch nonce");
    if (!dispatches.insert(dispatch.identity().hex()).second)
      throw error(error_code::invalid_transaction_run,
                  "transaction run duplicates a dispatch record");

    std::set<std::string> observations;
    for (const auto& observation : record.observations())
    {
      if (!observations.insert(observation.hex()).second)
        throw error(error_code::invalid_transaction_run,
                    "transaction run repeats an uncertainty observation");
    }

    if (active_state(record.state()))
    {
      if (!progress.contains_unit(dispatch.unit()))
        throw error(error_code::invalid_transaction_run,
                    "active transaction run contains a foreign graph unit");
      const auto current_dependencies =
          capture_dependencies(progress, dispatch.unit());
      if (current_dependencies != dispatch.dependencies())
        throw error(error_code::invalid_transaction_run,
                    "active dispatch contains forged predecessor evidence");
      if (dispatch.unit().kind() == transaction_unit_kind::operation &&
          dispatch.reserved_state() != progress.current_state().identity())
        throw error(error_code::invalid_transaction_run,
                    "active operation dispatch retains a stale state epoch");
      active.push_back(&record);
    }
  }

  for (std::size_t left = 0; left < active.size(); ++left)
  {
    for (std::size_t right = left + 1U; right < active.size(); ++right)
    {
      if (records_overlap(
              active[left]->dispatch(), active[right]->dispatch()))
        throw error(error_code::invalid_transaction_run,
                    "active transaction dispatches overlap graph members");
    }
  }

  for (const auto kind : {
           transaction_unit_kind::construction,
           transaction_unit_kind::check,
           transaction_unit_kind::operation})
  {
    const auto count = static_cast<std::size_t>(std::count_if(
        active.begin(), active.end(), [&](const auto* record) {
          return record->dispatch().unit().kind() == kind;
        }));
    if (count > policy_capacity(policy, kind))
      throw error(error_code::invalid_transaction_run,
                  "active dispatch count exceeds policy capacity");
  }

  for (const auto* record : active)
  {
    const auto& dispatch = record->dispatch();
    if (find_ready_unit(progress, dispatch.unit()) == nullptr)
      throw error(error_code::invalid_transaction_run,
                  "active dispatch unit is no longer graph-ready");
  }
}

void validate_rehydrated_run_history(
    const transaction_progress& progress,
    const std::vector<transaction_dispatch_record>& records)
{
  for (const auto& record : records)
  {
    const auto& dispatch = record.dispatch();
    if (!progress.contains_unit(dispatch.unit()))
      throw error(error_code::invalid_transaction_run,
                  "durable transaction run contains a foreign graph unit");

    const auto current_dependencies =
        capture_dependencies(progress, dispatch.unit());
    if (current_dependencies != dispatch.dependencies())
      throw error(error_code::invalid_transaction_run,
                  "durable transaction run contains forged predecessor evidence");

    if (record.state() != transaction_dispatch_state::completed)
      continue;

    const auto primary = dispatch.unit().primary_node();
    const auto expected = terminal_evidence_for_node(progress, primary);
    if (!expected || !record.terminal_evidence() ||
        *expected != *record.terminal_evidence())
      throw error(error_code::invalid_transaction_run,
                  "completed dispatch contradicts progression evidence");

    switch (dispatch.unit().kind())
    {
      case transaction_unit_kind::construction:
      {
        const auto* result = progress.construction(primary);
        if (result == nullptr || !record.attempt_session() ||
            *record.attempt_session() != result->session().identity())
          throw error(error_code::invalid_transaction_run,
                      "completed construction dispatch has forged attempt authority");
        break;
      }

      case transaction_unit_kind::check:
      {
        const auto* result = progress.check(primary);
        if (result == nullptr || !record.attempt_session() ||
            *record.attempt_session() != result->session().identity())
          throw error(error_code::invalid_transaction_run,
                      "completed check dispatch has forged attempt authority");
        break;
      }

      case transaction_unit_kind::operation:
      {
        const auto* result = progress.effect(primary);
        if (result == nullptr || !record.attempt_session() ||
            *record.attempt_session() != result->session().identity())
          throw error(error_code::invalid_transaction_run,
                      "completed operation dispatch has forged attempt authority");
        if (dispatch.reserved_state() !=
            result->session().request().expected_state().identity())
          throw error(error_code::invalid_transaction_run,
                      "completed operation dispatch has forged state authority");
        break;
      }
    }
  }
}

std::vector<std::string> dependency_identity_fields(
    const node_identity& node,
    const session_identity& evidence)
{
  return {node.hex(), evidence.hex()};
}

void validate_dispatch_may_start(const transaction_run& run)
{
  if (run.stopped())
    throw error(error_code::invalid_dispatch,
                "failure containment forbids starting reserved work");
}

void validate_dispatch_is_current(
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  if (find_ready_unit(run.progress(), dispatch.unit()) == nullptr)
    throw error(error_code::invalid_dispatch,
                "dispatch unit is no longer ready in current progression");

  const auto current = capture_dependencies(run.progress(), dispatch.unit());
  if (current != dispatch.dependencies())
    throw error(error_code::invalid_dispatch,
                "dispatch predecessor evidence changed after reservation");

  if (dispatch.unit().kind() == transaction_unit_kind::operation &&
      dispatch.reserved_state() != run.progress().current_state().identity())
    throw error(error_code::invalid_dispatch,
                "operation dispatch was reserved against a stale state epoch");
}

std::size_t record_index(
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  for (std::size_t index = 0; index < run.records().size(); ++index)
  {
    if (run.records()[index].dispatch().identity() == dispatch.identity())
      return index;
  }
  throw error(error_code::invalid_dispatch,
              "dispatch is absent from the transaction run");
}

void validate_record_dispatch(
    const transaction_dispatch_record& record,
    const transaction_dispatch& dispatch)
{
  if (record.dispatch().identity() != dispatch.identity() ||
      record.dispatch().nonce() != dispatch.nonce() ||
      record.dispatch().unit().identity() != dispatch.unit().identity())
    throw error(error_code::invalid_dispatch,
                "dispatch contradicts the retained run record");
}

std::vector<std::string> record_identity_fields(
    const transaction_dispatch& dispatch,
    transaction_dispatch_state state,
    const std::optional<session_identity>& attempt,
    const std::optional<session_identity>& effect_attempt,
    const std::vector<session_identity>& observations,
    const std::optional<session_identity>& terminal)
{
  std::vector<std::string> fields{
      dispatch.identity().hex(),
      std::string(dispatch_state_name(state)),
      attempt ? attempt->hex() : std::string{},
      effect_attempt ? effect_attempt->hex() : std::string{},
      std::to_string(observations.size())};
  for (const auto& observation : observations)
    fields.push_back(observation.hex());
  fields.push_back(terminal ? terminal->hex() : std::string{});
  return fields;
}

} // namespace

struct detail_dispatch_access final {
  static transaction_dispatch_dependency dependency(
      node_identity node,
      session_identity evidence)
  {
    auto identity = make_session_identity(
        "pkgctl/transaction-dispatch-dependency/1",
        dependency_identity_fields(node, evidence));
    return transaction_dispatch_dependency(
        std::move(node), std::move(evidence), std::move(identity));
  }

  static transaction_dispatch dispatch(
      ready_transaction_unit unit,
      transaction_dispatch_nonce nonce,
      session_identity progress,
      pkgstate::installed_state_snapshot_identity state,
      std::vector<transaction_dispatch_dependency> dependencies)
  {
    std::vector<std::string> fields{
        unit.identity().hex(), nonce.hex(), progress.hex(), state.string(),
        std::to_string(dependencies.size())};
    for (const auto& dependency : dependencies)
      fields.push_back(dependency.identity().hex());
    auto identity = make_session_identity(
        "pkgctl/transaction-dispatch/1", fields);
    return transaction_dispatch(
        std::move(unit), std::move(nonce), std::move(progress),
        std::move(state), std::move(dependencies), std::move(identity));
  }

  static transaction_dispatch_record record(
      transaction_dispatch dispatch,
      transaction_dispatch_state state,
      std::optional<session_identity> attempt,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations,
      std::optional<session_identity> terminal)
  {
    auto identity = make_session_identity(
        "pkgctl/transaction-dispatch-record/1",
        record_identity_fields(
            dispatch, state, attempt, effect_attempt, observations, terminal));
    return transaction_dispatch_record(
        std::move(dispatch), state, std::move(attempt),
        std::move(effect_attempt), std::move(observations),
        std::move(terminal), std::move(identity));
  }

  static transaction_run run(
      transaction_progress progress,
      transaction_dispatch_policy policy,
      std::vector<transaction_dispatch_record> records)
  {
    validate_run_records(progress, policy, records);
    std::vector<std::string> fields{
        progress.identity().hex(), policy.identity().hex(),
        std::to_string(records.size())};
    for (const auto& record : records)
      fields.push_back(record.identity().hex());
    auto identity = make_session_identity(
        "pkgctl/transaction-run/1", fields);
    return transaction_run(
        std::move(progress), std::move(policy),
        std::move(records), std::move(identity));
  }
};

namespace {

std::vector<transaction_dispatch_dependency> capture_dependencies(
    const transaction_progress& progress,
    const ready_transaction_unit& unit)
{
  std::vector<transaction_dispatch_dependency> result;
  for (const auto& predecessor : external_predecessors(progress, unit))
  {
    if (progress.status(predecessor) != transaction_node_status::satisfied)
      throw error(error_code::invalid_transaction_run,
                  "ready unit has an unsatisfied external predecessor");

    const auto* node = progress.transaction().program().find(predecessor);
    if (node == nullptr)
      throw error(error_code::invalid_transaction_run,
                  "ready unit predecessor is absent from transaction program");
    if (node->action() == pkgtransaction::transaction_action_kind::retain)
      continue;

    auto evidence = terminal_evidence_for_node(progress, predecessor);
    if (!evidence)
      throw error(error_code::invalid_transaction_run,
                  "satisfied predecessor lacks retained terminal evidence");
    result.push_back(detail_dispatch_access::dependency(
        predecessor, std::move(*evidence)));
  }
  std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.node() < rhs.node();
  });
  return result;
}

bool nonce_used(
    const transaction_run& run,
    const transaction_dispatch_nonce& nonce)
{
  return std::any_of(
      run.records().begin(), run.records().end(), [&](const auto& record) {
        return record.dispatch().nonce() == nonce;
      });
}

bool overlaps_active(
    const transaction_run& run,
    const ready_transaction_unit& unit)
{
  for (const auto& record : run.records())
  {
    if (!record.active())
      continue;
    for (const auto& member : unit.members())
    {
      if (contains_node(record.dispatch().unit().members(), member))
        return true;
    }
  }
  return false;
}

bool capacity_available(
    const transaction_run& run,
    transaction_unit_kind kind)
{
  return run.active_count(kind) < policy_capacity(run.policy(), kind);
}

transaction_dispatch_record started_record(
    const transaction_dispatch_record& current,
    const session_identity& session,
    std::optional<session_identity> effect_attempt = std::nullopt)
{
  return detail_dispatch_access::record(
      current.dispatch(), transaction_dispatch_state::started,
      session, std::move(effect_attempt), {}, std::nullopt);
}

transaction_dispatch_record completed_record(
    const transaction_dispatch_record& current,
    const session_identity& evidence)
{
  return detail_dispatch_access::record(
      current.dispatch(), transaction_dispatch_state::completed,
      current.attempt_session(), current.effect_attempt(),
      current.observations(), evidence);
}

transaction_dispatch_record observed_record(
    const transaction_dispatch_record& current,
    const session_identity& observation)
{
  auto observations = current.observations();
  if (std::find(observations.begin(), observations.end(), observation) !=
      observations.end())
    throw error(error_code::invalid_dispatch,
                "operation dispatch already retains this observation");
  observations.push_back(observation);
  return detail_dispatch_access::record(
      current.dispatch(), transaction_dispatch_state::started,
      current.attempt_session(), current.effect_attempt(),
      std::move(observations), std::nullopt);
}

transaction_dispatch_record released_record(
    const transaction_dispatch_record& current)
{
  return detail_dispatch_access::record(
      current.dispatch(), transaction_dispatch_state::released_unstarted,
      std::nullopt, std::nullopt, {}, std::nullopt);
}

transaction_run replace_record(
    transaction_run run,
    std::size_t index,
    transaction_dispatch_record replacement,
    std::optional<transaction_progress> progress = std::nullopt)
{
  auto records = run.records();
  records[index] = std::move(replacement);
  return detail_dispatch_access::run(
      progress ? std::move(*progress) : run.progress(),
      run.policy(), std::move(records));
}

const transaction_dispatch_record& require_record_state(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    transaction_dispatch_state expected)
{
  const auto index = record_index(run, dispatch);
  const auto& record = run.records()[index];
  validate_record_dispatch(record, dispatch);
  if (record.state() != expected)
    throw error(error_code::invalid_dispatch,
                "dispatch is not in the required ledger state");
  return record;
}

const transaction_dispatch_dependency* dispatch_dependency(
    const transaction_dispatch& dispatch,
    const node_identity& node)
{
  const auto found = std::find_if(
      dispatch.dependencies().begin(), dispatch.dependencies().end(),
      [&](const auto& dependency) {
        return dependency.node() == node;
      });
  return found == dispatch.dependencies().end() ? nullptr : &*found;
}

pkgsource::requirement_scope package_input_scope(
    pkgbuild::input_scope scope)
{
  switch (scope)
  {
    case pkgbuild::input_scope::build:
      return pkgsource::requirement_scope::build();
    case pkgbuild::input_scope::check:
      return pkgsource::requirement_scope::check();
  }
  throw error(error_code::invalid_dispatch,
              "construction input has an unknown requirement scope");
}

const pkgtransaction::transaction_edge& require_input_edge(
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgbuild::build_input& input)
{
  const auto expected_scope = package_input_scope(input.scope());
  const pkgtransaction::transaction_edge* match = nullptr;
  for (const auto& edge : progress.transaction().program().edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::requirement ||
        edge.after() != dispatch.unit().primary_node() || !edge.scope() ||
        *edge.scope() != expected_scope || !edge.requirement_witness() ||
        *edge.requirement_witness() != input.requirement().identity())
      continue;

    if (match != nullptr)
      throw error(error_code::invalid_dispatch,
                  "construction input has ambiguous transaction authority");
    match = &edge;
  }

  if (match == nullptr)
    throw error(error_code::invalid_dispatch,
                "construction input lacks exact transaction requirement authority");
  return *match;
}

void validate_selected_input_authority(
    const pkgtransaction::transaction_node& predecessor,
    const pkgbuild::build_input& input)
{
  const auto* selection = predecessor.selection();
  if (selection == nullptr)
    throw error(error_code::invalid_dispatch,
                "construction input lacks selected package authority");
  if (selection->identity() != input.selection().identity() ||
      selection->package() != input.package())
    throw error(error_code::invalid_dispatch,
                "construction input differs from selected package authority");
}

void validate_built_input(
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgtransaction::transaction_node& predecessor,
    const pkgbuild::build_input& input)
{
  validate_selected_input_authority(predecessor, input);

  const auto* construction = progress.construction(predecessor.identity());
  if (construction == nullptr || !construction->succeeded())
    throw error(error_code::invalid_dispatch,
                "construction input lacks successful predecessor evidence");

  const auto& build = construction->build().build();
  if (build.request().subject().identity() != input.selection().identity())
    throw error(error_code::invalid_dispatch,
                "construction input differs from predecessor build authority");

  const auto* dependency =
      dispatch_dependency(dispatch, predecessor.identity());
  if (dependency == nullptr ||
      dependency->evidence() != construction->identity())
    throw error(error_code::invalid_dispatch,
                "dispatch does not retain predecessor construction evidence");
}

void validate_retained_input(
    const transaction_progress& progress,
    const pkgtransaction::transaction_node& predecessor,
    const pkgbuild::build_input& input)
{
  const auto* installed = predecessor.installed();
  if (installed == nullptr)
    throw error(error_code::invalid_dispatch,
                "retained construction input lacks installed authority");

  const auto* current = progress.current_state().find_package(
      installed->release().name());
  if (current == nullptr || current->identity() != installed->identity())
    throw error(error_code::invalid_dispatch,
                "retained construction input is stale in current state");

  validate_selected_input_authority(predecessor, input);
  const auto* selected_installed = input.selection().installed();
  if (selected_installed == nullptr ||
      selected_installed->identity() != installed->identity())
    throw error(error_code::invalid_dispatch,
                "construction input differs from retained package authority");
}

void validate_construction_inputs(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const construction_session& session)
{
  std::set<pkgtransaction::transaction_edge_identity> matched_edges;
  for (const auto& input : session.request().build().inputs().for_scope(
           pkgbuild::input_scope::build))
  {
    const auto& edge = require_input_edge(run.progress(), dispatch, input);
    if (!matched_edges.insert(edge.identity()).second)
      throw error(error_code::invalid_dispatch,
                  "construction input repeats transaction requirement authority");

    const auto* predecessor =
        run.progress().transaction().program().find(edge.before());
    if (predecessor == nullptr)
      throw error(error_code::invalid_dispatch,
                  "construction input predecessor is absent from transaction");

    switch (predecessor->action())
    {
      case pkgtransaction::transaction_action_kind::build:
        validate_built_input(run.progress(), dispatch, *predecessor, input);
        break;
      case pkgtransaction::transaction_action_kind::retain:
        validate_retained_input(run.progress(), *predecessor, input);
        break;
      default:
        throw error(error_code::invalid_dispatch,
                    "construction input predecessor is not build authority");
    }
  }

  for (const auto& edge : run.progress().transaction().program().edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::requirement ||
        edge.after() != dispatch.unit().primary_node() || !edge.scope())
      continue;
    if (edge.scope()->kind() != pkgsource::requirement_scope_kind::build)
      continue;
    if (matched_edges.find(edge.identity()) == matched_edges.end())
      throw error(error_code::invalid_dispatch,
                  "construction session omits a transaction build input");
  }
}

void validate_exact_check_construction(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const transaction_check_session& session)
{
  const auto& supplied = session.request().construction();
  const auto& build_node = supplied.session().request().build_node();
  const auto* retained = run.progress().construction(build_node);
  if (retained == nullptr || retained->identity() != supplied.identity())
    throw error(error_code::invalid_dispatch,
                "check session differs from retained construction evidence");

  const auto* dependency = dispatch_dependency(dispatch, build_node);
  if (dependency == nullptr || dependency->evidence() != retained->identity())
    throw error(error_code::invalid_dispatch,
                "check dispatch lacks exact construction dependency");
}

void validate_construction_binding(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const construction_session& session)
{
  if (dispatch.unit().kind() != transaction_unit_kind::construction)
    throw error(error_code::invalid_dispatch,
                "construction session was supplied for another unit kind");
  if (session.request().transaction().identity() !=
          run.progress().transaction().identity() ||
      session.request().build_node() != dispatch.unit().primary_node())
    throw error(error_code::invalid_dispatch,
                "construction session does not match the reserved build unit");
  validate_construction_inputs(run, dispatch, session);
}

void validate_check_binding(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const transaction_check_session& session)
{
  if (dispatch.unit().kind() != transaction_unit_kind::check)
    throw error(error_code::invalid_dispatch,
                "check session was supplied for another unit kind");
  if (session.request().transaction().identity() !=
          run.progress().transaction().identity() ||
      session.request().check_node() != dispatch.unit().primary_node())
    throw error(error_code::invalid_dispatch,
                "check session does not match the reserved check unit");
  validate_exact_check_construction(run, dispatch, session);
}

void validate_operation_binding(
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session)
{
  if (dispatch.unit().kind() != transaction_unit_kind::operation)
    throw error(error_code::invalid_dispatch,
                "operation session was supplied for another unit kind");
  const auto& request = session.request();
  if (request.transaction().identity() !=
          run.progress().transaction().identity() ||
      request.action_node() != dispatch.unit().primary_node() ||
      request.expected_state().identity() != dispatch.reserved_state())
    throw error(error_code::invalid_dispatch,
                "operation session does not match the reserved operation unit");
}

} // namespace

transaction_dispatch_nonce::transaction_dispatch_nonce(byte_array bytes)
    : bytes_(std::move(bytes))
{
}

transaction_dispatch_nonce transaction_dispatch_nonce::from_bytes(
    byte_array bytes)
{
  const bool all_zero = std::all_of(
      bytes.begin(), bytes.end(), [](std::uint8_t value) {
        return value == 0U;
      });
  if (all_zero)
    throw error(error_code::invalid_dispatch,
                "dispatch nonce must not be all zero");
  return transaction_dispatch_nonce(std::move(bytes));
}

transaction_dispatch_nonce transaction_dispatch_nonce::from_hex(
    std::string value)
{
  if (value.size() != transaction_dispatch_nonce_size * 2U)
    throw error(error_code::invalid_dispatch,
                "dispatch nonce is not a 32-byte hexadecimal value");
  byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    const auto high = hexadecimal_digit(value[index * 2U]);
    const auto low = hexadecimal_digit(value[index * 2U + 1U]);
    bytes[index] = static_cast<std::uint8_t>((high << 4U) | low);
  }
  return from_bytes(std::move(bytes));
}

const transaction_dispatch_nonce::byte_array&
transaction_dispatch_nonce::bytes() const noexcept
{
  return bytes_;
}

std::string transaction_dispatch_nonce::hex() const
{
  return hexadecimal(bytes_);
}

bool operator==(const transaction_dispatch_nonce& lhs,
                const transaction_dispatch_nonce& rhs) noexcept
{
  return lhs.bytes_ == rhs.bytes_;
}

bool operator!=(const transaction_dispatch_nonce& lhs,
                const transaction_dispatch_nonce& rhs) noexcept
{
  return !(lhs == rhs);
}

bool operator<(const transaction_dispatch_nonce& lhs,
               const transaction_dispatch_nonce& rhs) noexcept
{
  return lhs.bytes_ < rhs.bytes_;
}

transaction_dispatch_policy::transaction_dispatch_policy(
    std::size_t construction_capacity,
    std::size_t check_capacity,
    transaction_failure_containment failure_containment,
    session_identity identity)
    : construction_capacity_(construction_capacity),
      check_capacity_(check_capacity),
      failure_containment_(failure_containment),
      identity_(std::move(identity))
{
}

transaction_dispatch_policy transaction_dispatch_policy::make(
    std::size_t construction_capacity,
    std::size_t check_capacity,
    transaction_failure_containment failure_containment)
{
  if (construction_capacity == 0U || check_capacity == 0U)
    throw error(error_code::invalid_dispatch_policy,
                "dispatch capacities must be nonzero");
  if (failure_containment !=
      transaction_failure_containment::stop_after_terminal_failure)
    throw error(error_code::invalid_dispatch_policy,
                "unsupported transaction failure-containment policy");

  auto identity = make_session_identity(
      "pkgctl/transaction-dispatch-policy/1",
      {std::to_string(construction_capacity),
       std::to_string(check_capacity),
       std::string(failure_containment_name(failure_containment))});
  return transaction_dispatch_policy(
      construction_capacity, check_capacity,
      failure_containment, std::move(identity));
}

transaction_dispatch_policy transaction_dispatch_policy::restore(
    std::size_t construction_capacity,
    std::size_t check_capacity,
    transaction_failure_containment failure_containment,
    const session_identity& expected_identity)
{
  auto policy = make(
      construction_capacity, check_capacity, failure_containment);
  if (policy.identity() != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable dispatch policy identity does not match its fields");
  return policy;
}

std::size_t transaction_dispatch_policy::construction_capacity() const noexcept
{
  return construction_capacity_;
}

std::size_t transaction_dispatch_policy::check_capacity() const noexcept
{
  return check_capacity_;
}

std::size_t transaction_dispatch_policy::operation_capacity() const noexcept
{
  return 1U;
}

transaction_failure_containment
transaction_dispatch_policy::failure_containment() const noexcept
{
  return failure_containment_;
}

const session_identity& transaction_dispatch_policy::identity() const noexcept
{
  return identity_;
}

transaction_dispatch_dependency::transaction_dispatch_dependency(
    node_identity node,
    session_identity evidence,
    session_identity identity)
    : node_(std::move(node)), evidence_(std::move(evidence)),
      identity_(std::move(identity))
{
}

transaction_dispatch_dependency transaction_dispatch_dependency::restore(
    node_identity node,
    session_identity evidence,
    const session_identity& expected_identity)
{
  auto dependency = detail_dispatch_access::dependency(
      std::move(node), std::move(evidence));
  if (dependency.identity() != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable dispatch dependency identity does not match its fields");
  return dependency;
}

const node_identity& transaction_dispatch_dependency::node() const noexcept
{
  return node_;
}

const session_identity&
transaction_dispatch_dependency::evidence() const noexcept
{
  return evidence_;
}

const session_identity&
transaction_dispatch_dependency::identity() const noexcept
{
  return identity_;
}

bool operator==(const transaction_dispatch_dependency& lhs,
                const transaction_dispatch_dependency& rhs) noexcept
{
  return lhs.node_ == rhs.node_ &&
         lhs.evidence_ == rhs.evidence_ &&
         lhs.identity_ == rhs.identity_;
}

bool operator!=(const transaction_dispatch_dependency& lhs,
                const transaction_dispatch_dependency& rhs) noexcept
{
  return !(lhs == rhs);
}

transaction_dispatch::transaction_dispatch(
    ready_transaction_unit unit,
    transaction_dispatch_nonce nonce,
    session_identity reserved_from_progress,
    pkgstate::installed_state_snapshot_identity reserved_state,
    std::vector<transaction_dispatch_dependency> dependencies,
    session_identity identity)
    : unit_(std::move(unit)), nonce_(std::move(nonce)),
      reserved_from_progress_(std::move(reserved_from_progress)),
      reserved_state_(std::move(reserved_state)),
      dependencies_(std::move(dependencies)), identity_(std::move(identity))
{
}

transaction_dispatch transaction_dispatch::restore(
    ready_transaction_unit unit,
    transaction_dispatch_nonce nonce,
    session_identity reserved_from_progress,
    pkgstate::installed_state_snapshot_identity reserved_state,
    std::vector<transaction_dispatch_dependency> dependencies,
    const session_identity& expected_identity)
{
  auto dispatch = detail_dispatch_access::dispatch(
      std::move(unit), std::move(nonce),
      std::move(reserved_from_progress), std::move(reserved_state),
      std::move(dependencies));
  if (dispatch.identity() != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable dispatch identity does not match its fields");
  return dispatch;
}

const ready_transaction_unit& transaction_dispatch::unit() const noexcept
{
  return unit_;
}

const transaction_dispatch_nonce& transaction_dispatch::nonce() const noexcept
{
  return nonce_;
}

const session_identity&
transaction_dispatch::reserved_from_progress() const noexcept
{
  return reserved_from_progress_;
}

const pkgstate::installed_state_snapshot_identity&
transaction_dispatch::reserved_state() const noexcept
{
  return reserved_state_;
}

const std::vector<transaction_dispatch_dependency>&
transaction_dispatch::dependencies() const noexcept
{
  return dependencies_;
}

const session_identity& transaction_dispatch::identity() const noexcept
{
  return identity_;
}

transaction_dispatch_record::transaction_dispatch_record(
    transaction_dispatch dispatch,
    transaction_dispatch_state state,
    std::optional<session_identity> attempt_session,
    std::optional<session_identity> effect_attempt,
    std::vector<session_identity> observations,
    std::optional<session_identity> terminal_evidence,
    session_identity identity)
    : dispatch_(std::move(dispatch)), state_(state),
      attempt_session_(std::move(attempt_session)),
      effect_attempt_(std::move(effect_attempt)),
      observations_(std::move(observations)),
      terminal_evidence_(std::move(terminal_evidence)),
      identity_(std::move(identity))
{
}

transaction_dispatch_record transaction_dispatch_record::restore(
    transaction_dispatch dispatch,
    transaction_dispatch_state state,
    std::optional<session_identity> attempt_session,
    std::optional<session_identity> effect_attempt,
    std::vector<session_identity> observations,
    std::optional<session_identity> terminal_evidence,
    const session_identity& expected_identity)
{
  auto record = detail_dispatch_access::record(
      std::move(dispatch), state, std::move(attempt_session),
      std::move(effect_attempt), std::move(observations),
      std::move(terminal_evidence));
  validate_record_shape(record);
  if (record.identity() != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable dispatch-record identity does not match its fields");
  return record;
}

const transaction_dispatch&
transaction_dispatch_record::dispatch() const noexcept
{
  return dispatch_;
}

transaction_dispatch_state
transaction_dispatch_record::state() const noexcept
{
  return state_;
}

bool transaction_dispatch_record::active() const noexcept
{
  return active_state(state_);
}

const std::optional<session_identity>&
transaction_dispatch_record::attempt_session() const noexcept
{
  return attempt_session_;
}

const std::optional<session_identity>&
transaction_dispatch_record::effect_attempt() const noexcept
{
  return effect_attempt_;
}

const std::vector<session_identity>&
transaction_dispatch_record::observations() const noexcept
{
  return observations_;
}

const std::optional<session_identity>&
transaction_dispatch_record::terminal_evidence() const noexcept
{
  return terminal_evidence_;
}

const session_identity&
transaction_dispatch_record::identity() const noexcept
{
  return identity_;
}

transaction_run::transaction_run(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    std::vector<transaction_dispatch_record> records,
    session_identity identity)
    : progress_(std::move(progress)), policy_(std::move(policy)),
      records_(std::move(records)), identity_(std::move(identity))
{
}

transaction_run transaction_run::restore(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    std::vector<transaction_dispatch_record> records,
    const session_identity& expected_identity)
{
  validate_rehydrated_run_history(progress, records);
  auto run = detail_dispatch_access::run(
      std::move(progress), std::move(policy), std::move(records));
  if (run.identity() != expected_identity)
    throw error(error_code::invalid_transaction_run,
                "durable transaction-run identity does not match its fields");
  return run;
}

transaction_run transaction_run::begin(
    transaction_progress progress,
    transaction_dispatch_policy policy)
{
  return detail_dispatch_access::run(
      std::move(progress), std::move(policy), {});
}

const transaction_progress& transaction_run::progress() const noexcept
{
  return progress_;
}

const transaction_dispatch_policy& transaction_run::policy() const noexcept
{
  return policy_;
}

const std::vector<transaction_dispatch_record>&
transaction_run::records() const noexcept
{
  return records_;
}

const transaction_dispatch_record* transaction_run::record(
    const session_identity& dispatch) const noexcept
{
  const auto found = std::find_if(
      records_.begin(), records_.end(), [&](const auto& candidate) {
        return candidate.dispatch().identity() == dispatch;
      });
  return found == records_.end() ? nullptr : &*found;
}

std::size_t transaction_run::active_count(
    transaction_unit_kind kind) const noexcept
{
  return static_cast<std::size_t>(std::count_if(
      records_.begin(), records_.end(), [&](const auto& record) {
        return record.active() && record.dispatch().unit().kind() == kind;
      }));
}

bool transaction_run::stopped() const noexcept
{
  return policy_.failure_containment() ==
             transaction_failure_containment::stop_after_terminal_failure &&
         progress_.failed();
}

const session_identity& transaction_run::identity() const noexcept
{
  return identity_;
}

transaction_dispatch_result reserve_next(
    transaction_run run,
    transaction_dispatch_nonce nonce)
{
  if (nonce_used(run, nonce))
    throw error(error_code::invalid_dispatch,
                "transaction run already used this dispatch nonce");

  if (run.stopped())
    return {std::move(run), std::nullopt};

  const ready_transaction_unit* selected = nullptr;
  for (const auto& unit : run.progress().ready_units())
  {
    if (!capacity_available(run, unit.kind()))
      continue;
    if (overlaps_active(run, unit))
      continue;
    selected = &unit;
    break;
  }

  if (selected == nullptr)
    return {std::move(run), std::nullopt};

  auto dispatch = detail_dispatch_access::dispatch(
      *selected, std::move(nonce), run.progress().identity(),
      run.progress().current_state().identity(),
      capture_dependencies(run.progress(), *selected));
  auto records = run.records();
  records.push_back(detail_dispatch_access::record(
      dispatch, transaction_dispatch_state::reserved,
      std::nullopt, std::nullopt, {}, std::nullopt));
  auto next = detail_dispatch_access::run(
      run.progress(), run.policy(), std::move(records));
  return {std::move(next), std::move(dispatch)};
}

transaction_run start_construction_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const construction_session& session)
{
  validate_dispatch_may_start(run);
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::reserved);
  validate_dispatch_is_current(run, dispatch);
  validate_construction_binding(run, dispatch, session);
  auto replacement = started_record(record, session.identity());
  return replace_record(
      std::move(run), index, std::move(replacement));
}

transaction_run start_check_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const transaction_check_session& session)
{
  validate_dispatch_may_start(run);
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::reserved);
  validate_dispatch_is_current(run, dispatch);
  validate_check_binding(run, dispatch, session);
  auto replacement = started_record(record, session.identity());
  return replace_record(
      std::move(run), index, std::move(replacement));
}

operation_dispatch_start_result start_operation_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    const effectful_operation_session& session,
    effect_attempt_nonce nonce)
{
  validate_dispatch_may_start(run);
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::reserved);
  validate_dispatch_is_current(run, dispatch);
  validate_operation_binding(run, dispatch, session);

  auto attempt = effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      std::move(nonce));
  auto replacement = started_record(
      record, session.identity(), attempt.attempt());
  auto started = replace_record(
      std::move(run), index, std::move(replacement));
  return operation_dispatch_start_result{
      std::move(started), std::move(attempt)};
}

transaction_run release_unstarted_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch)
{
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::reserved);
  auto replacement = released_record(record);
  return replace_record(
      std::move(run), index, std::move(replacement));
}

transaction_run complete_construction_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_result construction)
{
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::started);
  validate_dispatch_is_current(run, dispatch);
  validate_construction_binding(run, dispatch, construction.session());
  if (!record.attempt_session() ||
      *record.attempt_session() != construction.session().identity())
    throw error(error_code::invalid_dispatch,
                "construction result belongs to another started session");

  const auto evidence = construction.identity();
  auto progress = [&]() -> transaction_progress {
    try
    {
      return advance_construction(
          run.progress(), std::move(construction));
    }
    catch (const error& problem)
    {
      if (problem.code() != error_code::invalid_progression)
        throw;
      throw error(error_code::invalid_dispatch,
                  "construction evidence cannot retire this dispatch");
    }
  }();
  auto replacement = completed_record(record, evidence);
  return replace_record(
      std::move(run), index, std::move(replacement),
      std::move(progress));
}

transaction_run complete_check_dispatch(
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_result check)
{
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::started);
  validate_dispatch_is_current(run, dispatch);
  validate_check_binding(run, dispatch, check.session());
  if (!record.attempt_session() ||
      *record.attempt_session() != check.session().identity())
    throw error(error_code::invalid_dispatch,
                "check result belongs to another started session");

  const auto evidence = check.identity();
  auto progress = [&]() -> transaction_progress {
    try
    {
      return advance_check(run.progress(), std::move(check));
    }
    catch (const error& problem)
    {
      if (problem.code() != error_code::invalid_progression)
        throw;
      throw error(error_code::invalid_dispatch,
                  "check evidence cannot retire this dispatch");
    }
  }();
  auto replacement = completed_record(record, evidence);
  return replace_record(
      std::move(run), index, std::move(replacement),
      std::move(progress));
}

transaction_run submit_operation_dispatch_result(
    transaction_run run,
    const transaction_dispatch& dispatch,
    effectful_operation_result effect,
    std::optional<pkgstate::snapshot> resulting_state)
{
  const auto index = record_index(run, dispatch);
  const auto& record = require_record_state(
      run, dispatch, transaction_dispatch_state::started);
  validate_dispatch_is_current(run, dispatch);
  validate_operation_binding(run, dispatch, effect.session());
  if (!record.attempt_session() ||
      *record.attempt_session() != effect.session().identity())
    throw error(error_code::invalid_dispatch,
                "operation result belongs to another started session");

  const auto evidence = effect.identity();
  if (!operation_outcome_is_terminal(effect.outcome()))
  {
    if (resulting_state)
      throw error(error_code::invalid_dispatch,
                  "indeterminate operation observation cannot advance state");
    auto replacement = observed_record(record, evidence);
    return replace_record(
        std::move(run), index, std::move(replacement));
  }

  auto progress = [&]() -> transaction_progress {
    try
    {
      return advance_effect(
          run.progress(), std::move(effect), std::move(resulting_state));
    }
    catch (const error& problem)
    {
      if (problem.code() != error_code::invalid_progression)
        throw;
      throw error(error_code::invalid_dispatch,
                  "operation evidence cannot retire this dispatch");
    }
  }();
  auto replacement = completed_record(record, evidence);
  return replace_record(
      std::move(run), index, std::move(replacement),
      std::move(progress));
}

} // namespace pkgctl
