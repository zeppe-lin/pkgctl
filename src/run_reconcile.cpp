// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_reconcile.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_reconciliation(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition,
      message);
}

const transaction_dispatch_restart_assessment& require_assessment(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch& dispatch,
    transaction_dispatch_restart_disposition expected)
{
  const auto& active = checkpoint.assessment().active();
  const auto found = std::find_if(
      active.begin(), active.end(), [&](const auto& assessment) {
        return assessment.dispatch() == dispatch.identity();
      });
  if (found == active.end())
    invalid_reconciliation(
        "restart checkpoint does not retain the selected active dispatch");
  if (found->kind() != dispatch.unit().kind() ||
      found->disposition() != expected)
    invalid_reconciliation(
        "restart disposition does not authorize the requested reconciliation");

  const auto* record = checkpoint.run().record(dispatch.identity());
  if (record == nullptr || !record->active() ||
      record->dispatch().identity() != dispatch.identity())
    invalid_reconciliation(
        "restart checkpoint run contradicts the selected dispatch");
  return *found;
}

void validate_started_attempt(
    const transaction_dispatch_restart_assessment& assessment,
    const session_identity& session)
{
  if (assessment.state() != transaction_dispatch_state::started ||
      !assessment.attempt_session() ||
      *assessment.attempt_session() != session)
    invalid_reconciliation(
        "recovered evidence belongs to another started dispatch session");
}

void require_latest_effect_record(
    const effect_attempt_record& record,
    const effect_journal_store& effect_store)
{
  const auto latest = effect_store.load_latest(record.attempt());
  if (!latest || latest->identity() != record.identity())
    throw error(
        error_code::invalid_effect_session,
        "effect reconciliation checkpoint is not the latest durable record");
}

} // namespace

reserved_dispatch_reconciliation_checkpoint
reconcile_reserved_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    transaction_run_journal_store& run_store)
{
  const auto& assessment = require_assessment(
      checkpoint, dispatch,
      transaction_dispatch_restart_disposition::release_reserved);
  if (assessment.state() != transaction_dispatch_state::reserved ||
      assessment.external_evidence_required())
    invalid_reconciliation(
        "restart checkpoint does not authorize unstarted release");

  auto next = release_unstarted_dispatch(checkpoint.run(), dispatch);
  auto committed = commit_transaction_run_successor(
      checkpoint.record(), std::move(next), run_store);
  return reserved_dispatch_reconciliation_checkpoint{
      std::move(committed.run), std::move(committed.record)};
}

construction_dispatch_reconciliation_checkpoint
reconcile_construction_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    construction_result result,
    transaction_run_journal_store& run_store)
{
  const auto& assessment = require_assessment(
      checkpoint, dispatch,
      transaction_dispatch_restart_disposition::recover_construction);
  validate_started_attempt(assessment, result.session().identity());

  auto next = complete_construction_dispatch(
      checkpoint.run(), dispatch, result);
  auto committed = commit_transaction_run_successor(
      checkpoint.record(), std::move(next), run_store);
  return construction_dispatch_reconciliation_checkpoint{
      std::move(committed.run), std::move(committed.record),
      std::move(result)};
}

check_dispatch_reconciliation_checkpoint
reconcile_check_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    transaction_check_result result,
    transaction_run_journal_store& run_store)
{
  const auto& assessment = require_assessment(
      checkpoint, dispatch,
      transaction_dispatch_restart_disposition::recover_check);
  validate_started_attempt(assessment, result.session().identity());

  auto next = complete_check_dispatch(checkpoint.run(), dispatch, result);
  auto committed = commit_transaction_run_successor(
      checkpoint.record(), std::move(next), run_store);
  return check_dispatch_reconciliation_checkpoint{
      std::move(committed.run), std::move(committed.record),
      std::move(result)};
}

operation_dispatch_reconciliation_result
reconcile_operation_dispatch_durable(
    transaction_run_restart_checkpoint checkpoint,
    const transaction_dispatch& dispatch,
    effect_restart_checkpoint effect_checkpoint,
    transaction_effect_driver& driver,
    effect_journal_store& effect_store,
    transaction_run_journal_store& run_store)
{
  const auto& dispatch_assessment = require_assessment(
      checkpoint, dispatch,
      transaction_dispatch_restart_disposition::inspect_effect_journal);
  validate_started_attempt(
      dispatch_assessment, effect_checkpoint.session().identity());
  if (!dispatch_assessment.effect_attempt() ||
      *dispatch_assessment.effect_attempt() !=
          effect_checkpoint.record().attempt())
  {
    invalid_reconciliation(
        "effect checkpoint belongs to another durable operation attempt");
  }

  require_latest_effect_record(effect_checkpoint.record(), effect_store);
  const auto effect_assessment = assess_effect_restart(
      effect_checkpoint.record());
  if (!effect_assessment.automatically_continuable())
  {
    return operation_dispatch_reconciliation_result{
        checkpoint.run(), checkpoint.record(),
        effect_assessment.disposition(), effect_checkpoint.record(),
        std::nullopt, false};
  }

  auto effect = resume_effectful_operation(
      std::move(effect_checkpoint), driver, effect_store);
  if (!effect.operation())
  {
    if (!effect.external_resolution_required())
      invalid_reconciliation(
          "effect continuation returned no operation evidence");
    return operation_dispatch_reconciliation_result{
        checkpoint.run(), checkpoint.record(), effect.disposition(),
        effect.journal(), std::nullopt, false};
  }

  std::optional<pkgstate::snapshot> resulting_state;
  if (effect.operation()->succeeded())
    resulting_state = driver.read_state();

  auto next = submit_operation_dispatch_result(
      checkpoint.run(), dispatch, *effect.operation(),
      std::move(resulting_state));
  auto committed = commit_transaction_run_successor(
      checkpoint.record(), std::move(next), run_store);
  return operation_dispatch_reconciliation_result{
      std::move(committed.run), std::move(committed.record),
      effect.disposition(), effect.journal(), effect.operation(), true};
}

} // namespace pkgctl
