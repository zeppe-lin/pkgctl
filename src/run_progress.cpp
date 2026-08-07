// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_progress.h>

#include "effect_rehydration.h"
#include "run_recovery_detail.h"

#include <libpkgstate/error.h>
#include <libpkgstate/publication_projection.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

[[noreturn]] void rehydration_failure(
    transaction_progress_rehydration_error_code code,
    std::string message)
{
  throw transaction_progress_rehydration_error(code, std::move(message));
}

void require_completed_shape(const transaction_dispatch_record& record)
{
  if (record.state() != transaction_dispatch_state::completed ||
      !record.attempt_session() || !record.terminal_evidence())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::invalid_record,
        "completed progress evidence has an invalid dispatch shape");
  }

  const bool operation =
      record.dispatch().unit().kind() == transaction_unit_kind::operation;
  if (operation != record.effect_attempt().has_value())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::invalid_record,
        "completed progress evidence has invalid effect-attempt authority");
  }
}

template<typename Evidence>
void validate_evidence_binding(
    const transaction_run_journal_record& record,
    const transaction_dispatch_record& completed,
    const Evidence& evidence)
{
  const auto& dispatch = completed.dispatch();
  if (evidence.journal() != record.journal() ||
      evidence.transaction() != record.transaction() ||
      evidence.dispatch() != dispatch.identity() ||
      evidence.node() != dispatch.unit().primary_node() ||
      evidence.attempt_session() != *completed.attempt_session() ||
      evidence.result() != *completed.terminal_evidence())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "dispatch evidence belongs to another durable completion");
  }
}

construction_result rehydrate_construction(
    const transaction_run_journal_record& record,
    const transaction_dispatch_record& completed,
    const transaction_progress& partial,
    transaction_run_evidence_store& evidence_store,
    transaction_progress_rehydration_context_source& context)
{
  const auto& dispatch = completed.dispatch();
  auto evidence = evidence_store.load_construction(
      record.journal(), dispatch.identity(), *completed.attempt_session());
  if (!evidence)
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_missing,
        "completed construction lacks durable execution evidence");
  }
  validate_evidence_binding(record, completed, *evidence);
  auto bodies = context.construction(record, partial, dispatch, *evidence);
  auto result = detail::rehydrate_construction_dispatch_evidence(
      *evidence, std::move(bodies));
  if (result.identity() != *completed.terminal_evidence())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "construction result differs from durable terminal evidence");
  }
  return result;
}

transaction_check_result rehydrate_check(
    const transaction_run_journal_record& record,
    const transaction_dispatch_record& completed,
    const transaction_progress& partial,
    transaction_run_evidence_store& evidence_store,
    transaction_progress_rehydration_context_source& context)
{
  const auto& dispatch = completed.dispatch();
  auto evidence = evidence_store.load_check(
      record.journal(), dispatch.identity(), *completed.attempt_session());
  if (!evidence)
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_missing,
        "completed check lacks durable execution evidence");
  }
  validate_evidence_binding(record, completed, *evidence);
  auto bodies = context.check(record, partial, dispatch, *evidence);
  auto result = detail::rehydrate_check_dispatch_evidence(
      *evidence, std::move(bodies));
  if (result.identity() != *completed.terminal_evidence())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "check result differs from durable terminal evidence");
  }
  return result;
}

effectful_operation_result rehydrate_operation(
    const transaction_run_journal_record& record,
    const transaction_dispatch_record& completed,
    const transaction_progress& partial,
    effect_journal_store& effect_store,
    transaction_progress_rehydration_context_source& context)
{
  const auto& dispatch = completed.dispatch();
  auto evidence = effect_store.load_latest(*completed.effect_attempt());
  if (!evidence)
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_missing,
        "completed operation lacks durable effect evidence");
  }
  if (evidence->attempt() != *completed.effect_attempt() ||
      evidence->session() != *completed.attempt_session() ||
      evidence->stage() != effect_attempt_stage::terminal)
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "effect evidence belongs to another or nonterminal attempt");
  }

  auto checkpoint = context.operation(
      record, partial, dispatch, *evidence);
  if (checkpoint.record().identity() != evidence->identity() ||
      checkpoint.record().attempt() != evidence->attempt() ||
      checkpoint.session().identity() != *completed.attempt_session())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "operation checkpoint differs from exact durable effect evidence");
  }

  auto result = detail::rehydrate_terminal_effectful_operation(
      std::move(checkpoint));
  if (result.identity() != *completed.terminal_evidence())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "operation result differs from durable terminal evidence");
  }
  return result;
}

std::optional<pkgstate::snapshot> resulting_state(
    const transaction_progress& partial,
    const effectful_operation_result& effect)
{
  if (!effect.succeeded())
    return std::nullopt;
  if (!effect.publication_request())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "successful operation lacks exact publication request authority");
  }

  const auto result = [&]() -> pkgstate::snapshot {
    try
    {
      return pkgstate::project_publication_request(
          *effect.publication_request(), partial.current_state());
    }
    catch (const pkgstate::state_error& failure)
    {
      rehydration_failure(
          transaction_progress_rehydration_error_code::evidence_mismatch,
          std::string("operation publication projection is invalid: ") +
              failure.what());
    }
  }();
  if (effect.reconciled_state() &&
      *effect.reconciled_state() != result.identity())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::evidence_mismatch,
        "reconciled operation state differs from publication projection");
  }
  return result;
}

bool ready_for_replay(
    const transaction_progress& progress,
    const transaction_dispatch_record& completed)
{
  const auto& unit = completed.dispatch().unit();
  return progress.contains_unit(unit) &&
      progress.status(unit.primary_node()) == transaction_node_status::ready;
}

} // namespace

transaction_progress_rehydration_error::
transaction_progress_rehydration_error(
    transaction_progress_rehydration_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

transaction_progress_rehydration_error_code
transaction_progress_rehydration_error::code() const noexcept
{
  return code_;
}

stored_transaction_progress_rehydration_source::
stored_transaction_progress_rehydration_source(
    transaction_session transaction,
    transaction_run_evidence_store& evidence,
    effect_journal_store& effects,
    transaction_progress_rehydration_context_source& context)
    : transaction_(std::move(transaction)), evidence_(evidence),
      effects_(effects), context_(context)
{
}

transaction_progress
stored_transaction_progress_rehydration_source::rehydrate_progress(
    const transaction_run_journal_record& record)
{
  if (record.transaction() != transaction_.identity())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::invalid_record,
        "durable run record belongs to another transaction");
  }

  transaction_progress progress = transaction_progress::begin(transaction_);
  std::vector<const transaction_dispatch_record*> pending;
  pending.reserve(record.dispatches().size());
  for (const auto& dispatch : record.dispatches())
  {
    if (dispatch.state() != transaction_dispatch_state::completed)
      continue;
    require_completed_shape(dispatch);
    pending.push_back(&dispatch);
  }

  while (!pending.empty())
  {
    bool advanced = false;
    for (auto position = pending.begin(); position != pending.end();)
    {
      const auto& completed = **position;
      if (!ready_for_replay(progress, completed))
      {
        ++position;
        continue;
      }

      switch (completed.dispatch().unit().kind())
      {
        case transaction_unit_kind::construction:
        {
          auto construction = rehydrate_construction(
              record, completed, progress, evidence_, context_);
          progress = advance_construction(
              std::move(progress), std::move(construction));
          break;
        }

        case transaction_unit_kind::check:
        {
          auto check = rehydrate_check(
              record, completed, progress, evidence_, context_);
          progress = advance_check(std::move(progress), std::move(check));
          break;
        }

        case transaction_unit_kind::operation:
        {
          auto effect = rehydrate_operation(
              record, completed, progress, effects_, context_);
          auto state = resulting_state(progress, effect);
          progress = advance_effect(
              std::move(progress), std::move(effect), std::move(state));
          break;
        }
      }

      position = pending.erase(position);
      advanced = true;
    }

    if (!advanced)
    {
      rehydration_failure(
          transaction_progress_rehydration_error_code::unresolved_history,
          "completed dispatch history cannot be replayed from graph authority");
    }
  }

  if (progress.identity() != record.progress() ||
      progress.current_state().identity() != record.current_state() ||
      progress.complete() != record.complete() ||
      progress.failed() != record.failed())
  {
    rehydration_failure(
        transaction_progress_rehydration_error_code::progress_mismatch,
        "reconstructed progress differs from durable run authority");
  }
  return progress;
}

} // namespace pkgctl
