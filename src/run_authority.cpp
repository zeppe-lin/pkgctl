// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_authority.h>

#include <pkgctl/error.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_authority(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition,
      message);
}

std::string kind_field(transaction_unit_kind kind)
{
  switch (kind)
  {
    case transaction_unit_kind::construction:
      return "construction";
    case transaction_unit_kind::check:
      return "check";
    case transaction_unit_kind::operation:
      return "operation";
  }
  invalid_authority("unknown transaction dispatch kind");
}

std::string disposition_field(
    transaction_dispatch_restart_disposition disposition)
{
  switch (disposition)
  {
    case transaction_dispatch_restart_disposition::release_reserved:
      return "release-reserved";
    case transaction_dispatch_restart_disposition::recover_construction:
      return "recover-construction";
    case transaction_dispatch_restart_disposition::recover_check:
      return "recover-check";
    case transaction_dispatch_restart_disposition::inspect_effect_journal:
      return "inspect-effect-journal";
  }
  invalid_authority("unknown transaction restart disposition");
}

void validate_record_run(
    const transaction_run_journal_record& record,
    const transaction_run& run)
{
  if (record.run() != run.identity() ||
      record.progress() != run.progress().identity() ||
      record.transaction() != run.progress().transaction().identity() ||
      record.policy().identity() != run.policy().identity())
  {
    invalid_authority(
        "durable record and transaction run name different authority");
  }

  const auto reopened = record.reopen(run.progress());
  if (reopened.identity() != run.identity())
    invalid_authority(
        "durable record does not reopen the supplied transaction run");
}

const transaction_dispatch_record& require_dispatch_record(
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  const auto* record = run.record(dispatch.identity());
  if (record == nullptr ||
      record->dispatch().identity() != dispatch.identity())
  {
    invalid_authority(
        "transaction run does not retain the selected dispatch");
  }
  return *record;
}

const transaction_dispatch_restart_assessment& require_assessment(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch& dispatch)
{
  const auto& active = checkpoint.assessment().active();
  const auto found = std::find_if(
      active.begin(), active.end(), [&](const auto& assessment) {
        return assessment.dispatch() == dispatch.identity();
      });
  if (found == active.end())
    invalid_authority(
        "restart checkpoint does not retain the selected active dispatch");
  if (found->kind() != dispatch.unit().kind())
    invalid_authority(
        "restart assessment and dispatch classify different work");

  const auto& record = require_dispatch_record(checkpoint.run(), dispatch);
  if (!record.active() || record.state() != found->state())
    invalid_authority(
        "restart assessment contradicts the selected dispatch record");
  return *found;
}

session_identity execution_identity(
    const transaction_run_journal_record& record,
    const transaction_run& run,
    const transaction_dispatch& dispatch,
    const transaction_dispatch_execution_authority_body& authority)
{
  std::vector<std::string> fields{
      record.identity().hex(), run.identity().hex(), dispatch.identity().hex(),
      kind_field(dispatch.unit().kind())};
  if (const auto* value = std::get_if<construction_session>(&authority))
    fields.push_back(value->identity().hex());
  else if (const auto* value =
               std::get_if<transaction_check_session>(&authority))
    fields.push_back(value->identity().hex());
  else
  {
    const auto& operation =
        std::get<operation_dispatch_execution_authority>(authority);
    fields.push_back(operation.session.identity().hex());
    fields.push_back(operation.nonce.hex());
  }
  return make_session_identity(
      "pkgctl.transaction-dispatch-execution-authority.v1", fields);
}

session_identity recovery_identity(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch,
    const transaction_dispatch_recovery_authority_body& authority)
{
  std::vector<std::string> fields{
      checkpoint.record().identity().hex(),
      checkpoint.run().identity().hex(),
      dispatch.identity().hex(),
      disposition_field(assessment.disposition())};
  if (const auto* value = std::get_if<construction_result>(&authority))
    fields.push_back(value->identity().hex());
  else if (const auto* value =
               std::get_if<transaction_check_result>(&authority))
    fields.push_back(value->identity().hex());
  else if (const auto* value =
               std::get_if<effect_restart_checkpoint>(&authority))
  {
    fields.push_back(value->record().identity().hex());
    fields.push_back(value->session().identity().hex());
  }
  return make_session_identity(
      "pkgctl.transaction-dispatch-recovery-authority.v1", fields);
}

void validate_construction_recovery(
    const transaction_dispatch_restart_assessment& assessment,
    const construction_result& result)
{
  if (assessment.disposition() !=
          transaction_dispatch_restart_disposition::recover_construction ||
      assessment.state() != transaction_dispatch_state::started ||
      !assessment.attempt_session() ||
      *assessment.attempt_session() != result.session().identity())
  {
    invalid_authority(
        "construction recovery authority belongs to another started attempt");
  }
}

void validate_check_recovery(
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_check_result& result)
{
  if (assessment.disposition() !=
          transaction_dispatch_restart_disposition::recover_check ||
      assessment.state() != transaction_dispatch_state::started ||
      !assessment.attempt_session() ||
      *assessment.attempt_session() != result.session().identity())
  {
    invalid_authority(
        "check recovery authority belongs to another started attempt");
  }
}

void validate_operation_recovery(
    const transaction_dispatch_restart_assessment& assessment,
    const effect_restart_checkpoint& checkpoint)
{
  if (assessment.disposition() !=
          transaction_dispatch_restart_disposition::inspect_effect_journal ||
      assessment.state() != transaction_dispatch_state::started ||
      !assessment.attempt_session() ||
      *assessment.attempt_session() != checkpoint.session().identity() ||
      !assessment.effect_attempt() ||
      *assessment.effect_attempt() != checkpoint.record().attempt())
  {
    invalid_authority(
        "operation recovery authority belongs to another durable attempt");
  }
}

} // namespace

transaction_dispatch_execution_handoff::
transaction_dispatch_execution_handoff(
    transaction_run_journal_record record,
    transaction_run run,
    transaction_dispatch dispatch,
    transaction_dispatch_execution_authority_body authority,
    session_identity identity)
    : record_(std::move(record)), run_(std::move(run)),
      dispatch_(std::move(dispatch)), authority_(std::move(authority)),
      identity_(std::move(identity))
{
}

const transaction_run_journal_record&
transaction_dispatch_execution_handoff::record() const noexcept
{ return record_; }
const transaction_run& transaction_dispatch_execution_handoff::run() const noexcept
{ return run_; }
const transaction_dispatch&
transaction_dispatch_execution_handoff::dispatch() const noexcept
{ return dispatch_; }
transaction_unit_kind transaction_dispatch_execution_handoff::kind() const noexcept
{ return dispatch_.unit().kind(); }
const transaction_dispatch_execution_authority_body&
transaction_dispatch_execution_handoff::authority() const noexcept
{ return authority_; }
const construction_session*
transaction_dispatch_execution_handoff::construction() const noexcept
{ return std::get_if<construction_session>(&authority_); }
const transaction_check_session*
transaction_dispatch_execution_handoff::check() const noexcept
{ return std::get_if<transaction_check_session>(&authority_); }
const operation_dispatch_execution_authority*
transaction_dispatch_execution_handoff::operation() const noexcept
{ return std::get_if<operation_dispatch_execution_authority>(&authority_); }
const session_identity&
transaction_dispatch_execution_handoff::identity() const noexcept
{ return identity_; }

transaction_dispatch_recovery_handoff::
transaction_dispatch_recovery_handoff(
    transaction_run_restart_checkpoint checkpoint,
    transaction_dispatch_restart_assessment assessment,
    transaction_dispatch dispatch,
    transaction_dispatch_recovery_authority_body authority,
    session_identity identity)
    : checkpoint_(std::move(checkpoint)), assessment_(std::move(assessment)),
      dispatch_(std::move(dispatch)), authority_(std::move(authority)),
      identity_(std::move(identity))
{
}

const transaction_run_restart_checkpoint&
transaction_dispatch_recovery_handoff::checkpoint() const noexcept
{ return checkpoint_; }
const transaction_dispatch_restart_assessment&
transaction_dispatch_recovery_handoff::assessment() const noexcept
{ return assessment_; }
const transaction_dispatch&
transaction_dispatch_recovery_handoff::dispatch() const noexcept
{ return dispatch_; }
transaction_dispatch_restart_disposition
transaction_dispatch_recovery_handoff::disposition() const noexcept
{ return assessment_.disposition(); }
const transaction_dispatch_recovery_authority_body&
transaction_dispatch_recovery_handoff::authority() const noexcept
{ return authority_; }
bool transaction_dispatch_recovery_handoff::releases_reserved() const noexcept
{ return std::holds_alternative<std::monostate>(authority_); }
const construction_result*
transaction_dispatch_recovery_handoff::construction() const noexcept
{ return std::get_if<construction_result>(&authority_); }
const transaction_check_result*
transaction_dispatch_recovery_handoff::check() const noexcept
{ return std::get_if<transaction_check_result>(&authority_); }
const effect_restart_checkpoint*
transaction_dispatch_recovery_handoff::operation() const noexcept
{ return std::get_if<effect_restart_checkpoint>(&authority_); }
const session_identity&
transaction_dispatch_recovery_handoff::identity() const noexcept
{ return identity_; }

transaction_run_restart_checkpoint rehydrate_transaction_run(
    transaction_run_journal_record record,
    transaction_progress_rehydration_source& source)
{
  auto progress = source.rehydrate_progress(record);
  return transaction_run_restart_checkpoint::make(
      std::move(progress), std::move(record));
}

transaction_dispatch_execution_handoff
acquire_transaction_dispatch_execution_authority(
    transaction_run_journal_record record,
    transaction_run run,
    transaction_dispatch dispatch,
    transaction_dispatch_execution_authority_source& source)
{
  validate_record_run(record, run);
  const auto& retained = require_dispatch_record(run, dispatch);
  if (retained.state() != transaction_dispatch_state::reserved)
    invalid_authority(
        "fresh execution authority requires an exact reserved dispatch");

  transaction_dispatch_execution_authority_body authority =
      [&]() -> transaction_dispatch_execution_authority_body {
    switch (dispatch.unit().kind())
    {
      case transaction_unit_kind::construction:
      {
        auto session = source.construction(record, run, dispatch);
        (void)start_construction_dispatch(run, dispatch, session);
        return session;
      }
      case transaction_unit_kind::check:
      {
        auto session = source.check(record, run, dispatch);
        (void)start_check_dispatch(run, dispatch, session);
        return session;
      }
      case transaction_unit_kind::operation:
      {
        auto value = source.operation(record, run, dispatch);
        (void)start_operation_dispatch(
            run, dispatch, value.session, value.nonce);
        return value;
      }
    }
    invalid_authority("unknown transaction dispatch kind");
  }();

  auto identity = execution_identity(record, run, dispatch, authority);
  return transaction_dispatch_execution_handoff(
      std::move(record), std::move(run), std::move(dispatch),
      std::move(authority), std::move(identity));
}

transaction_dispatch_recovery_handoff
acquire_transaction_dispatch_recovery_authority(
    transaction_run_restart_checkpoint checkpoint,
    transaction_dispatch dispatch,
    transaction_dispatch_recovery_authority_source& source)
{
  const auto& selected = require_assessment(checkpoint, dispatch);
  transaction_dispatch_restart_assessment assessment = selected;

  transaction_dispatch_recovery_authority_body authority =
      [&]() -> transaction_dispatch_recovery_authority_body {
    switch (assessment.disposition())
    {
      case transaction_dispatch_restart_disposition::release_reserved:
        if (assessment.state() != transaction_dispatch_state::reserved ||
            assessment.external_evidence_required())
          invalid_authority(
              "reserved recovery authority contradicts restart assessment");
        return std::monostate{};
      case transaction_dispatch_restart_disposition::recover_construction:
      {
        auto result = source.construction(checkpoint, assessment, dispatch);
        validate_construction_recovery(assessment, result);
        return result;
      }
      case transaction_dispatch_restart_disposition::recover_check:
      {
        auto result = source.check(checkpoint, assessment, dispatch);
        validate_check_recovery(assessment, result);
        return result;
      }
      case transaction_dispatch_restart_disposition::inspect_effect_journal:
      {
        auto result = source.operation(checkpoint, assessment, dispatch);
        validate_operation_recovery(assessment, result);
        return result;
      }
    }
    invalid_authority("unknown transaction restart disposition");
  }();

  auto identity = recovery_identity(
      checkpoint, assessment, dispatch, authority);
  return transaction_dispatch_recovery_handoff(
      std::move(checkpoint), std::move(assessment), std::move(dispatch),
      std::move(authority), std::move(identity));
}

} // namespace pkgctl
