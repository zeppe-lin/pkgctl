// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_execute.h>
#include <pkgctl/error.h>

#include <optional>
#include <string>
#include <utility>

namespace pkgctl {
namespace {

void validate_started_replay(
    const transaction_run_journal_record& started_record,
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const session_identity& session,
    transaction_unit_kind kind)
{
  if (dispatch.unit().kind() != kind ||
      started_record.run() != run.identity() ||
      started_record.transaction() != run.progress().transaction().identity())
  {
    throw error(
        error_code::invalid_dispatch,
        "started replay authority names another durable run");
  }
  const auto reopened = started_record.reopen(run.progress());
  if (reopened.identity() != run.identity())
    throw error(
        error_code::invalid_dispatch,
        "started replay record does not reopen the supplied run");
  const auto* retained = run.record(dispatch.identity());
  if (retained == nullptr ||
      retained->dispatch().identity() != dispatch.identity() ||
      retained->state() != transaction_dispatch_state::started ||
      !retained->attempt_session() || *retained->attempt_session() != session)
  {
    throw error(
        error_code::invalid_dispatch,
        "started replay session differs from durable attempt ownership");
  }
}

} // namespace

construction_dispatch_execution_checkpoint
reexecute_started_construction_dispatch_durable(
    const transaction_run_journal_record& started_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_session session,
    construction_driver& driver,
    transaction_run_evidence_store& evidence_store,
    transaction_run_journal_store& run_store)
{
  validate_started_replay(
      started_record, run, dispatch, session.identity(),
      transaction_unit_kind::construction);
  auto result = execute_construction_unpublished(std::move(session), driver);
  auto admitted_evidence = construction_dispatch_evidence_record::admit(
      started_record, dispatch, result);
  auto evidence = evidence_store.publish(admitted_evidence);
  publish_construction(result, driver);
  auto completed = complete_construction_dispatch(
      std::move(run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_record, std::move(completed), run_store);
  return construction_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result),
      std::move(evidence)};
}

check_dispatch_execution_checkpoint reexecute_started_check_dispatch_durable(
    const transaction_run_journal_record& started_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_session session,
    transaction_check_driver& driver,
    transaction_run_evidence_store& evidence_store,
    transaction_run_journal_store& run_store)
{
  validate_started_replay(
      started_record, run, dispatch, session.identity(),
      transaction_unit_kind::check);
  auto result = execute_transaction_check(std::move(session), driver);
  auto admitted_evidence = check_dispatch_evidence_record::admit(
      started_record, dispatch, result);
  auto evidence = evidence_store.publish(admitted_evidence);
  auto completed = complete_check_dispatch(std::move(run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_record, std::move(completed), run_store);
  return check_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result),
      std::move(evidence)};
}

construction_dispatch_execution_checkpoint
execute_construction_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_session session,
    construction_driver& driver,
    transaction_run_evidence_store& evidence_store,
    transaction_run_journal_store& run_store)
{
  auto attempt = construction_dispatch_attempt_record::admit(
      current_record, dispatch, session);
  (void)evidence_store.publish(attempt);
  auto started = start_construction_dispatch(
      std::move(run), dispatch, session);
  auto started_checkpoint = commit_transaction_run_successor(
      current_record, std::move(started), run_store);

  auto result = execute_construction_unpublished(std::move(session), driver);
  auto admitted_evidence = construction_dispatch_evidence_record::admit(
      started_checkpoint.record, dispatch, result);
  auto evidence = evidence_store.publish(admitted_evidence);
  publish_construction(result, driver);
  auto completed = complete_construction_dispatch(
      std::move(started_checkpoint.run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_checkpoint.record, std::move(completed), run_store);

  return construction_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result),
      std::move(evidence)};
}

check_dispatch_execution_checkpoint
execute_check_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_session session,
    transaction_check_driver& driver,
    transaction_run_evidence_store& evidence_store,
    transaction_run_journal_store& run_store)
{
  auto attempt = check_dispatch_attempt_record::admit(
      current_record, dispatch, session);
  (void)evidence_store.publish(attempt);
  auto started = start_check_dispatch(
      std::move(run), dispatch, session);
  auto started_checkpoint = commit_transaction_run_successor(
      current_record, std::move(started), run_store);

  auto result = execute_transaction_check(std::move(session), driver);
  auto admitted_evidence = check_dispatch_evidence_record::admit(
      started_checkpoint.record, dispatch, result);
  auto evidence = evidence_store.publish(admitted_evidence);
  auto completed = complete_check_dispatch(
      std::move(started_checkpoint.run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_checkpoint.record, std::move(completed), run_store);

  return check_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result),
      std::move(evidence)};
}

operation_dispatch_execution_checkpoint
execute_operation_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    effectful_operation_session session,
    effect_attempt_nonce nonce,
    transaction_effect_driver& continuation,
    transaction_effect_state_observer& resulting_state,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store,
    transaction_effect_body_sink* bodies)
{
  auto started = commit_operation_dispatch_start(
      current_record, std::move(run), dispatch, session, nonce,
      effect_store, run_store);

  const auto& target = session.request().application().target();
  try
  {
    pkgapply::validate_target_mutation_lease_scope(
        target, resulting_state.lease());
  }
  catch (const pkgapply::mutation_lease_error& problem)
  {
    throw error(
        error_code::driver_contract_violation,
        std::string("resulting-state observer has invalid target authority: ") +
            problem.what());
  }
  if (continuation.lease().identity() != resulting_state.lease().identity())
  {
    throw error(
        error_code::driver_contract_violation,
        "continuation and resulting-state authorities use different leases");
  }

  auto result = execute_effectful_operation_durable(
      session, nonce, continuation, effect_store, bodies);

  std::optional<pkgstate::snapshot> observed_state;
  if (result.succeeded())
    observed_state = resulting_state.read_state();

  auto next = submit_operation_dispatch_result(
      std::move(started.run), dispatch, result, std::move(observed_state));
  auto completed = commit_transaction_run_successor(
      started.run_record, std::move(next), run_store);

  return operation_dispatch_execution_checkpoint{
      std::move(completed.run),
      std::move(completed.record),
      std::move(started.effect_attempt),
      std::move(result)};
}

} // namespace pkgctl
