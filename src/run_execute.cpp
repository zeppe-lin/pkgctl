// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_execute.h>

#include <optional>
#include <utility>

namespace pkgctl {

construction_dispatch_execution_checkpoint
execute_construction_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    construction_session session,
    construction_driver& driver,
    transaction_run_journal_store& run_store)
{
  auto started = start_construction_dispatch(
      std::move(run), dispatch, session);
  auto started_checkpoint = commit_transaction_run_successor(
      current_record, std::move(started), run_store);

  auto result = execute_construction(std::move(session), driver);
  auto completed = complete_construction_dispatch(
      std::move(started_checkpoint.run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_checkpoint.record, std::move(completed), run_store);

  return construction_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result)};
}

check_dispatch_execution_checkpoint
execute_check_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    transaction_check_session session,
    transaction_check_driver& driver,
    transaction_run_journal_store& run_store)
{
  auto started = start_check_dispatch(
      std::move(run), dispatch, session);
  auto started_checkpoint = commit_transaction_run_successor(
      current_record, std::move(started), run_store);

  auto result = execute_transaction_check(std::move(session), driver);
  auto completed = complete_check_dispatch(
      std::move(started_checkpoint.run), dispatch, result);
  auto completed_checkpoint = commit_transaction_run_successor(
      started_checkpoint.record, std::move(completed), run_store);

  return check_dispatch_execution_checkpoint{
      std::move(completed_checkpoint.run),
      std::move(completed_checkpoint.record),
      std::move(result)};
}

operation_dispatch_execution_checkpoint
execute_operation_dispatch_durable(
    const transaction_run_journal_record& current_record,
    transaction_run run,
    const transaction_dispatch& dispatch,
    effectful_operation_session session,
    effect_attempt_nonce nonce,
    transaction_effect_driver& driver,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store)
{
  auto started = commit_operation_dispatch_start(
      current_record, std::move(run), dispatch, session, nonce,
      effect_store, run_store);

  auto result = execute_effectful_operation_durable(
      session, nonce, driver, effect_store);

  std::optional<pkgstate::snapshot> resulting_state;
  if (result.succeeded())
    resulting_state = driver.read_state();

  auto next = submit_operation_dispatch_result(
      std::move(started.run), dispatch, result, std::move(resulting_state));
  auto completed = commit_transaction_run_successor(
      started.run_record, std::move(next), run_store);

  return operation_dispatch_execution_checkpoint{
      std::move(completed.run),
      std::move(completed.record),
      std::move(started.effect_attempt),
      std::move(result)};
}

} // namespace pkgctl
