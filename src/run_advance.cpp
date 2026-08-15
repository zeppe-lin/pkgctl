// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_advance.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace pkgctl {

struct detail_transaction_run_advance_access final {
  static transaction_run_advance_result make(
      transaction_run run,
      transaction_run_journal_record record,
      transaction_run_advance_disposition disposition,
      std::optional<transaction_dispatch> dispatch,
      transaction_run_advance_evidence evidence)
  {
    return transaction_run_advance_result(
        std::move(run), std::move(record), disposition,
        std::move(dispatch), std::move(evidence));
  }
};

namespace {

[[noreturn]] void invalid_advancement(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition,
      message);
}

transaction_run_journal_record load_committed_head(
    const session_identity& journal,
    const transaction_run_journal_store& store)
{
  auto latest = store.load_latest(journal);
  if (!latest)
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_conflict,
        "run advancement journal has no committed store head");
  }
  if (latest->journal() != journal)
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_contract_violation,
        "run store returned foreign advancement authority");
  }
  return *latest;
}

construction_driver& require_construction_driver(
    transaction_run_advance_drivers drivers)
{
  if (drivers.construction == nullptr)
    invalid_advancement(
        "construction advancement requires a construction driver");
  return *drivers.construction;
}

transaction_check_driver& require_check_driver(
    transaction_run_advance_drivers drivers)
{
  if (drivers.check == nullptr)
    invalid_advancement("check advancement requires a check driver");
  return *drivers.check;
}

transaction_effect_driver_source& require_operation_driver_source(
    transaction_run_advance_drivers drivers)
{
  if (drivers.operation == nullptr)
    invalid_advancement(
        "operation advancement requires an effect driver source");
  return *drivers.operation;
}

void validate_operation_driver(
    const effectful_operation_session& session,
    transaction_effect_driver& driver)
{
  const auto& request = session.request();
  try
  {
    pkgapply::validate_target_mutation_lease(
        request.application().target(), driver.state_projection(),
        driver.lease());
  }
  catch (const pkgapply::mutation_lease_error& problem)
  {
    invalid_advancement(
        std::string("operation driver has invalid target authority: ") +
        problem.what());
  }

  const auto& expected = request.expected_state();
  const auto& state = driver.state_projection();
  if (state.snapshot().string() != expected.identity().string() ||
      state.ownership_inventory().string() !=
          expected.ownership_identity().string())
  {
    invalid_advancement(
        "operation driver state projection belongs to another state epoch");
  }
}

void validate_state_observer(
    const effectful_operation_session& session,
    transaction_effect_state_observer& observer)
{
  try
  {
    pkgapply::validate_target_mutation_lease_scope(
        session.request().application().target(), observer.lease());
  }
  catch (const pkgapply::mutation_lease_error& problem)
  {
    invalid_advancement(
        std::string("state observer has invalid target authority: ") +
        problem.what());
  }
}

void validate_shared_lease(
    transaction_effect_driver& continuation,
    transaction_effect_state_observer& observer)
{
  if (continuation.lease().identity() != observer.lease().identity())
  {
    invalid_advancement(
        "continuation and resulting-state authorities use different leases");
  }
}

transaction_effect_execution_drivers acquire_execution_drivers(
    transaction_effect_driver_source& source,
    const transaction_dispatch_execution_handoff& handoff)
{
  const auto* authority = handoff.operation();
  if (authority == nullptr)
    invalid_advancement(
        "operation execution handoff carries no effect authority");

  auto drivers = source.acquire_execution_drivers(handoff);
  if (!drivers.continuation)
    invalid_advancement(
        "operation effect driver source returned no execution continuation");
  if (!drivers.resulting_state)
    invalid_advancement(
        "operation effect driver source returned no execution state observer");

  validate_operation_driver(authority->session, *drivers.continuation);
  validate_state_observer(authority->session, *drivers.resulting_state);
  validate_shared_lease(*drivers.continuation, *drivers.resulting_state);
  return drivers;
}

transaction_effect_recovery_drivers acquire_recovery_drivers(
    transaction_effect_driver_source& source,
    const transaction_dispatch_recovery_handoff& handoff)
{
  const auto* checkpoint = handoff.operation();
  if (checkpoint == nullptr)
    invalid_advancement(
        "operation recovery handoff carries no effect checkpoint");

  const bool needs_continuation =
      operation_reconciliation_requires_continuation_driver(*checkpoint);
  const bool needs_state =
      operation_reconciliation_requires_state_observer(*checkpoint);
  const bool needs_publication =
      operation_reconciliation_requires_publication_driver(*checkpoint);

  auto drivers = source.acquire_recovery_drivers(handoff);
  if (static_cast<bool>(drivers.continuation) != needs_continuation)
    invalid_advancement(
        "operation recovery source returned the wrong continuation authority");
  if (static_cast<bool>(drivers.resulting_state) != needs_state)
    invalid_advancement(
        "operation recovery source returned the wrong state-observer authority");
  if (static_cast<bool>(drivers.publication) != needs_publication)
    invalid_advancement(
        "operation recovery source returned the wrong publication authority");

  const auto& session = checkpoint->session();
  if (drivers.continuation)
    validate_operation_driver(session, *drivers.continuation);
  if (drivers.resulting_state)
    validate_state_observer(session, *drivers.resulting_state);
  if (drivers.publication)
    validate_state_observer(session, *drivers.publication);
  if (drivers.continuation && drivers.resulting_state)
    validate_shared_lease(*drivers.continuation, *drivers.resulting_state);
  return drivers;
}

effect_journal_store& require_effect_store(
    transaction_run_advance_stores stores)
{
  if (stores.effects == nullptr)
    invalid_advancement("operation advancement requires an effect store");
  return *stores.effects;
}

void require_execution_dependencies(
    transaction_unit_kind kind,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  switch (kind)
  {
    case transaction_unit_kind::construction:
      (void)require_construction_driver(drivers);
      return;
    case transaction_unit_kind::check:
      (void)require_check_driver(drivers);
      return;
    case transaction_unit_kind::operation:
      (void)require_operation_driver_source(drivers);
      (void)require_effect_store(stores);
      return;
  }
  invalid_advancement("unknown transaction dispatch kind");
}

transaction_dispatch require_dispatch(
    const transaction_run& run,
    const session_identity& identity)
{
  const auto* record = run.record(identity);
  if (record == nullptr || !record->active())
    invalid_advancement(
        "restart assessment does not name an active durable dispatch");
  return record->dispatch();
}

transaction_run_advance_result reconcile_active(
    transaction_run_restart_checkpoint checkpoint,
    transaction_dispatch dispatch,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  auto handoff = acquire_transaction_dispatch_recovery_authority(
      std::move(checkpoint), dispatch, authorities.recovery);

  switch (handoff.disposition())
  {
    case transaction_dispatch_restart_disposition::release_reserved:
    {
      auto value = reconcile_reserved_dispatch_durable(
          handoff.checkpoint(), handoff.dispatch(), stores.runs);
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::released_reserved,
          handoff.dispatch(), std::monostate{});
    }
    case transaction_dispatch_restart_disposition::recover_construction:
    {
      if (const auto* session = handoff.construction_retry())
      {
        auto value = reexecute_started_construction_dispatch_durable(
            handoff.checkpoint().record(), handoff.checkpoint().run(),
            handoff.dispatch(), *session, require_construction_driver(drivers),
            stores.evidence, stores.runs);
        auto evidence = value.result;
        return detail_transaction_run_advance_access::make(
            std::move(value.run), std::move(value.record),
            transaction_run_advance_disposition::reconciled_construction,
            handoff.dispatch(), std::move(evidence));
      }
      const auto* result = handoff.construction();
      if (result == nullptr)
        invalid_advancement(
            "construction recovery handoff carries no replay or result authority");
      auto value = reconcile_construction_dispatch_durable(
          handoff.checkpoint(), handoff.dispatch(), *result, stores.runs);
      auto evidence = value.result;
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::reconciled_construction,
          handoff.dispatch(), std::move(evidence));
    }
    case transaction_dispatch_restart_disposition::recover_check:
    {
      if (const auto* session = handoff.check_retry())
      {
        auto value = reexecute_started_check_dispatch_durable(
            handoff.checkpoint().record(), handoff.checkpoint().run(),
            handoff.dispatch(), *session, require_check_driver(drivers),
            stores.evidence, stores.runs);
        auto evidence = value.result;
        return detail_transaction_run_advance_access::make(
            std::move(value.run), std::move(value.record),
            transaction_run_advance_disposition::reconciled_check,
            handoff.dispatch(), std::move(evidence));
      }
      const auto* result = handoff.check();
      if (result == nullptr)
        invalid_advancement(
            "check recovery handoff carries no replay or result authority");
      auto value = reconcile_check_dispatch_durable(
          handoff.checkpoint(), handoff.dispatch(), *result, stores.runs);
      auto evidence = value.result;
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::reconciled_check,
          handoff.dispatch(), std::move(evidence));
    }
    case transaction_dispatch_restart_disposition::inspect_effect_journal:
    {
      const auto* recovery = handoff.operation();
      if (recovery == nullptr)
        invalid_advancement(
            "operation recovery handoff carries no effect checkpoint");
      transaction_effect_recovery_drivers acquired{};
      if (operation_reconciliation_requires_continuation_driver(*recovery) ||
          operation_reconciliation_requires_state_observer(*recovery) ||
          operation_reconciliation_requires_publication_driver(*recovery))
      {
        try
        {
          acquired = acquire_recovery_drivers(
              require_operation_driver_source(drivers), handoff);
        }
        catch (const transaction_effect_authority_unavailable&)
        {
          return detail_transaction_run_advance_access::make(
              handoff.checkpoint().run(), handoff.checkpoint().record(),
              transaction_run_advance_disposition::
                  mutation_authority_unavailable,
              handoff.dispatch(), std::monostate{});
        }
      }
      auto value = reconcile_operation_dispatch_durable(
          handoff.checkpoint(), handoff.dispatch(), *recovery,
          acquired.continuation.get(), acquired.resulting_state.get(),
          acquired.publication.get(), require_effect_store(stores), stores.runs,
          acquired.bodies);
      const auto disposition = value.run_advanced
          ? transaction_run_advance_disposition::reconciled_operation
          : transaction_run_advance_disposition::external_resolution_required;
      transaction_run_operation_advance_evidence evidence{
          value.effect_record, value.result, value.disposition};
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record), disposition,
          handoff.dispatch(), std::move(evidence));
    }
  }
  invalid_advancement("unknown transaction restart disposition");
}

void validate_advance_result(
    const transaction_run& run,
    const transaction_run_journal_record& record,
    transaction_run_advance_disposition disposition,
    const std::optional<transaction_dispatch>& dispatch,
    const transaction_run_advance_evidence& evidence)
{
  const auto reopened = record.reopen(run.progress());
  if (reopened.identity() != run.identity())
    invalid_advancement(
        "transaction advancement result contradicts its durable record");

  if (dispatch && run.record(dispatch->identity()) == nullptr)
    invalid_advancement(
        "transaction advancement result names an unretained dispatch");

  const auto construction = std::holds_alternative<construction_result>(
      evidence);
  const auto check = std::holds_alternative<transaction_check_result>(
      evidence);
  const auto* operation =
      std::get_if<transaction_run_operation_advance_evidence>(&evidence);
  const auto empty = std::holds_alternative<std::monostate>(evidence);

  switch (disposition)
  {
    case transaction_run_advance_disposition::quiescent:
      if (dispatch || !empty)
        invalid_advancement(
            "quiescent advancement carries dispatch or semantic evidence");
      return;
    case transaction_run_advance_disposition::released_reserved:
      if (!dispatch || !empty)
        invalid_advancement(
            "released advancement has malformed reservation evidence");
      return;
    case transaction_run_advance_disposition::reconciled_construction:
    case transaction_run_advance_disposition::executed_construction:
      if (!dispatch || !construction)
        invalid_advancement(
            "construction advancement has malformed semantic evidence");
      return;
    case transaction_run_advance_disposition::reconciled_check:
    case transaction_run_advance_disposition::executed_check:
      if (!dispatch || !check)
        invalid_advancement(
            "check advancement has malformed semantic evidence");
      return;
    case transaction_run_advance_disposition::executed_operation:
      if (!dispatch || operation == nullptr || !operation->result ||
          operation->restart_disposition)
        invalid_advancement(
            "fresh operation advancement has malformed effect evidence");
      return;
    case transaction_run_advance_disposition::reconciled_operation:
      if (!dispatch || operation == nullptr || !operation->result ||
          !operation->restart_disposition)
        invalid_advancement(
            "reconciled operation advancement has malformed effect evidence");
      return;
    case transaction_run_advance_disposition::external_resolution_required:
      if (!dispatch || operation == nullptr || operation->result ||
          operation->restart_disposition !=
              std::optional<effect_restart_disposition>(
                  effect_restart_disposition::external_resolution_required))
        invalid_advancement(
            "externally blocked advancement has malformed effect evidence");
      return;
    case transaction_run_advance_disposition::mutation_authority_unavailable:
    {
      if (!dispatch || !empty)
        invalid_advancement(
            "mutation-authority block has malformed semantic evidence");
      const auto* retained = run.record(dispatch->identity());
      if (retained == nullptr ||
          (retained->state() != transaction_dispatch_state::started &&
           retained->state() !=
               transaction_dispatch_state::released_unstarted))
        invalid_advancement(
            "mutation-authority block has malformed dispatch state");
      return;
    }
  }
  invalid_advancement("unknown transaction advancement disposition");
}

transaction_run_advance_result execute_reserved(
    transaction_run_commit_checkpoint reserved,
    transaction_dispatch dispatch,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  auto handoff = acquire_transaction_dispatch_execution_authority(
      reserved.record, reserved.run, dispatch, authorities.execution);

  switch (handoff.kind())
  {
    case transaction_unit_kind::construction:
    {
      const auto* session = handoff.construction();
      if (session == nullptr)
        invalid_advancement(
            "construction execution handoff carries no admitted session");
      auto value = execute_construction_dispatch_durable(
          handoff.record(), handoff.run(), handoff.dispatch(), *session,
          require_construction_driver(drivers), stores.evidence, stores.runs);
      auto evidence = value.result;
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::executed_construction,
          handoff.dispatch(), std::move(evidence));
    }
    case transaction_unit_kind::check:
    {
      const auto* session = handoff.check();
      if (session == nullptr)
        invalid_advancement(
            "check execution handoff carries no admitted session");
      auto value = execute_check_dispatch_durable(
          handoff.record(), handoff.run(), handoff.dispatch(), *session,
          require_check_driver(drivers), stores.evidence, stores.runs);
      auto evidence = value.result;
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::executed_check,
          handoff.dispatch(), std::move(evidence));
    }
    case transaction_unit_kind::operation:
    {
      const auto* authority = handoff.operation();
      if (authority == nullptr)
        invalid_advancement(
            "operation execution handoff carries no effect authority");
      transaction_effect_execution_drivers acquired;
      try
      {
        acquired = acquire_execution_drivers(
            require_operation_driver_source(drivers), handoff);
      }
      catch (const transaction_effect_authority_unavailable&)
      {
        auto released = release_unstarted_dispatch(
            handoff.run(), handoff.dispatch());
        auto committed = commit_transaction_run_successor(
            handoff.record(), std::move(released), stores.runs);
        return detail_transaction_run_advance_access::make(
            std::move(committed.run), std::move(committed.record),
            transaction_run_advance_disposition::
                mutation_authority_unavailable,
            handoff.dispatch(), std::monostate{});
      }
      auto value = execute_operation_dispatch_durable(
          handoff.record(), handoff.run(), handoff.dispatch(),
          authority->session, authority->nonce, *acquired.continuation,
          *acquired.resulting_state, require_effect_store(stores), stores.runs,
          acquired.bodies);
      transaction_run_operation_advance_evidence evidence{
          value.admission, value.result, std::nullopt};
      return detail_transaction_run_advance_access::make(
          std::move(value.run), std::move(value.record),
          transaction_run_advance_disposition::executed_operation,
          handoff.dispatch(), std::move(evidence));
    }
  }
  invalid_advancement("unknown transaction dispatch kind");
}

transaction_run_advance_result advance_loaded_transaction_run_once(
    session_identity journal,
    std::optional<transaction_dispatch_nonce> supplied_nonce,
    transaction_dispatch_nonce_source* nonce_source,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  auto committed = load_committed_head(journal, stores.runs);
  auto checkpoint = rehydrate_transaction_run(
      std::move(committed), authorities.progress);

  if (!checkpoint.assessment().active().empty())
  {
    const auto dispatch = require_dispatch(
        checkpoint.run(), checkpoint.assessment().active().front().dispatch());
    return reconcile_active(
        std::move(checkpoint), dispatch, authorities, drivers, stores);
  }

  if (!supplied_nonce &&
      (checkpoint.run().stopped() ||
       checkpoint.run().progress().ready_units().empty()))
  {
    return detail_transaction_run_advance_access::make(
        checkpoint.run(), checkpoint.record(),
        transaction_run_advance_disposition::quiescent,
        std::nullopt, std::monostate{});
  }

  if (!supplied_nonce && nonce_source == nullptr)
    invalid_advancement("fresh advancement has no dispatch nonce authority");
  transaction_dispatch_nonce nonce = supplied_nonce
      ? std::move(*supplied_nonce)
      : nonce_source->issue(checkpoint.record(), checkpoint.run());
  auto reservation = reserve_next(checkpoint.run(), std::move(nonce));
  if (!reservation.dispatch)
  {
    if (supplied_nonce)
    {
      return detail_transaction_run_advance_access::make(
          std::move(reservation.run), checkpoint.record(),
          transaction_run_advance_disposition::quiescent,
          std::nullopt, std::monostate{});
    }
    invalid_advancement(
        "reservable transaction run produced no fresh dispatch");
  }

  auto dispatch = *reservation.dispatch;
  require_execution_dependencies(dispatch.unit().kind(), drivers, stores);
  auto reserved = commit_transaction_run_successor(
      checkpoint.record(), std::move(reservation.run), stores.runs);
  return execute_reserved(
      std::move(reserved), std::move(dispatch),
      authorities, drivers, stores);
}

} // namespace

transaction_run_advance_result::transaction_run_advance_result(
    transaction_run run,
    transaction_run_journal_record record,
    transaction_run_advance_disposition disposition,
    std::optional<transaction_dispatch> dispatch,
    transaction_run_advance_evidence evidence)
    : run_(std::move(run)), record_(std::move(record)),
      disposition_(disposition), dispatch_(std::move(dispatch)),
      evidence_(std::move(evidence))
{
  validate_advance_result(
      run_, record_, disposition_, dispatch_, evidence_);
}

const transaction_run& transaction_run_advance_result::run() const noexcept
{
  return run_;
}

const transaction_run_journal_record&
transaction_run_advance_result::record() const noexcept
{
  return record_;
}

transaction_run_advance_disposition
transaction_run_advance_result::disposition() const noexcept
{
  return disposition_;
}

const std::optional<transaction_dispatch>&
transaction_run_advance_result::dispatch() const noexcept
{
  return dispatch_;
}

const transaction_run_advance_evidence&
transaction_run_advance_result::evidence() const noexcept
{
  return evidence_;
}

const construction_result*
transaction_run_advance_result::construction() const noexcept
{
  return std::get_if<construction_result>(&evidence_);
}

const transaction_check_result*
transaction_run_advance_result::check() const noexcept
{
  return std::get_if<transaction_check_result>(&evidence_);
}

const transaction_run_operation_advance_evidence*
transaction_run_advance_result::operation() const noexcept
{
  return std::get_if<transaction_run_operation_advance_evidence>(&evidence_);
}

bool transaction_run_advance_result::durable_transition_committed()
    const noexcept
{
  if (disposition_ ==
      transaction_run_advance_disposition::mutation_authority_unavailable)
  {
    if (!dispatch_)
      return false;
    const auto* retained = run_.record(dispatch_->identity());
    return retained != nullptr &&
        retained->state() == transaction_dispatch_state::released_unstarted;
  }
  return disposition_ != transaction_run_advance_disposition::quiescent &&
      disposition_ !=
          transaction_run_advance_disposition::external_resolution_required;
}

bool transaction_run_advance_result::external_resolution_required()
    const noexcept
{
  return disposition_ ==
      transaction_run_advance_disposition::external_resolution_required;
}

bool transaction_run_advance_result::mutation_authority_unavailable()
    const noexcept
{
  return disposition_ ==
      transaction_run_advance_disposition::mutation_authority_unavailable;
}

transaction_run_advance_result advance_transaction_run_once(
    session_identity journal,
    transaction_dispatch_nonce nonce,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  return advance_loaded_transaction_run_once(
      std::move(journal), std::move(nonce), nullptr,
      authorities, drivers, stores);
}

transaction_run_advance_result advance_transaction_run_once(
    session_identity journal,
    transaction_dispatch_nonce_source& nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  return advance_loaded_transaction_run_once(
      std::move(journal), std::nullopt, &nonces,
      authorities, drivers, stores);
}

} // namespace pkgctl
