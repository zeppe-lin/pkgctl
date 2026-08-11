// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_restart.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_checkpoint(const std::string& message)
{
  throw error(error_code::invalid_effect_session, message);
}


void validate_application_receipt(
    const effectful_operation_session& session,
    const pkgapply::application_receipt& receipt)
{
  const auto& request = session.request().application();
  if (receipt.request() != request.identity() ||
      receipt.plan() != request.plan() ||
      receipt.kind() != request.kind() ||
      receipt.target() != request.target().identity() ||
      receipt.control() != request.control().identity())
    invalid_checkpoint(
        "application receipt belongs to another effect authority universe");
  if (receipt.outcome() == pkgapply::application_attempt_outcome::completed &&
      !receipt.completed_evidence())
    invalid_checkpoint(
        "completed application checkpoint lacks completed evidence");
}

void validate_application_journal(
    const effectful_operation_session& session,
    const pkgapply::application_journal_record& journal)
{
  const auto& request = session.request().application();
  const auto& header = journal.header();
  if (header.request() != request.identity() ||
      header.plan() != request.plan() ||
      header.kind() != request.kind() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity())
    invalid_checkpoint(
        "application journal belongs to another effect authority universe");
}

void validate_ahead_application_receipt(
    const effectful_operation_session& session,
    const pkgapply::application_receipt& receipt,
    const pkgapply::application_journal_record& journal)
{
  validate_application_receipt(session, receipt);

  const auto assessment = pkgapply::assess_application_restart(journal);
  if (assessment.disposition() !=
      pkgapply::application_restart_disposition::terminal)
    invalid_checkpoint(
        "application receipt is ahead of a nonterminal application journal");
  if (!journal.receipt() || *journal.receipt() != receipt.identity())
    invalid_checkpoint(
        "ahead application receipt differs from terminal application journal");
  if (!receipt.journal() ||
      *receipt.journal() != journal.header().identity())
    invalid_checkpoint(
        "ahead application receipt names another durable application journal");

  const auto completed = receipt.completed_evidence()
      ? std::optional<pkgapply::completed_application_evidence_identity>(
            receipt.completed_evidence()->identity())
      : std::nullopt;
  if (completed != journal.completed_evidence())
    invalid_checkpoint(
        "ahead application receipt differs from terminal completed evidence");
}

} // namespace

effect_restart_assessment::effect_restart_assessment(
    session_identity attempt,
    session_identity record,
    effect_attempt_stage stage,
    effect_restart_disposition disposition)
    : attempt_(std::move(attempt)), record_(std::move(record)), stage_(stage),
      disposition_(disposition)
{
}
const session_identity& effect_restart_assessment::attempt() const noexcept
{ return attempt_; }
const session_identity& effect_restart_assessment::record() const noexcept
{ return record_; }
effect_attempt_stage effect_restart_assessment::stage() const noexcept
{ return stage_; }
effect_restart_disposition effect_restart_assessment::disposition() const noexcept
{ return disposition_; }
bool effect_restart_assessment::automatically_continuable() const noexcept
{
  return disposition_ != effect_restart_disposition::external_resolution_required;
}

effect_restart_assessment assess_effect_restart(
    const effect_attempt_record& record)
{
  effect_restart_disposition disposition =
      effect_restart_disposition::external_resolution_required;
  switch (record.stage())
  {
    case effect_attempt_stage::admitted:
      disposition = record.before_total() == 0U
          ? effect_restart_disposition::start_application
          : effect_restart_disposition::continue_before_lifecycle;
      break;
    case effect_attempt_stage::before_lifecycle_intent:
      break;
    case effect_attempt_stage::before_lifecycle_terminal:
      if (!record.before().back().succeeded())
        disposition = effect_restart_disposition::seal_terminal;
      else if (record.before().size() < record.before_total())
        disposition = effect_restart_disposition::continue_before_lifecycle;
      else
        disposition = effect_restart_disposition::start_application;
      break;
    case effect_attempt_stage::application_intent:
      disposition = effect_restart_disposition::resume_application;
      break;
    case effect_attempt_stage::application_terminal:
      if (record.application()->outcome() !=
          pkgapply::application_attempt_outcome::completed)
        disposition = effect_restart_disposition::seal_terminal;
      else if (record.after_total() == 0U)
        disposition = effect_restart_disposition::start_publication;
      else
        disposition = effect_restart_disposition::continue_after_application;
      break;
    case effect_attempt_stage::after_lifecycle_intent:
      break;
    case effect_attempt_stage::after_lifecycle_terminal:
      if (!record.after().back().succeeded())
        disposition = effect_restart_disposition::seal_terminal;
      else if (record.after().size() < record.after_total())
        disposition = effect_restart_disposition::continue_after_lifecycle;
      else
        disposition = effect_restart_disposition::start_publication;
      break;
    case effect_attempt_stage::publication_intent:
      disposition = effect_restart_disposition::reconcile_publication;
      break;
    case effect_attempt_stage::publication_terminal:
      if (record.publication()->outcome() ==
              pkgstate::state_publication_outcome::published_durability_unconfirmed ||
          record.publication()->outcome() ==
              pkgstate::state_publication_outcome::indeterminate)
        disposition = effect_restart_disposition::reconcile_publication;
      else
        disposition = effect_restart_disposition::seal_terminal;
      break;
    case effect_attempt_stage::terminal:
      disposition = effect_restart_disposition::terminal;
      break;
  }
  return effect_restart_assessment(
      record.attempt(), record.identity(), record.stage(), disposition);
}

bool effect_restart_requires_continuation_driver(
    const effect_restart_checkpoint& checkpoint)
{
  const auto assessment = assess_effect_restart(checkpoint.record());
  switch (assessment.disposition())
  {
    case effect_restart_disposition::external_resolution_required:
    case effect_restart_disposition::reconcile_publication:
    case effect_restart_disposition::seal_terminal:
    case effect_restart_disposition::terminal:
      return false;
    case effect_restart_disposition::resume_application:
      return checkpoint.application_journal().has_value();
    case effect_restart_disposition::continue_before_lifecycle:
    case effect_restart_disposition::start_application:
    case effect_restart_disposition::continue_after_application:
    case effect_restart_disposition::continue_after_lifecycle:
    case effect_restart_disposition::start_publication:
      return true;
  }
  invalid_checkpoint("unknown effect restart disposition");
}

bool effect_restart_requires_publication_driver(
    const effect_restart_checkpoint& checkpoint)
{
  return assess_effect_restart(checkpoint.record()).disposition() ==
      effect_restart_disposition::reconcile_publication;
}

effect_restart_checkpoint::effect_restart_checkpoint(
    effectful_operation_session session,
    effect_attempt_record record,
    std::vector<pkgapply_exec::lifecycle_execution_result> before,
    std::optional<pkgapply::application_receipt> application,
    std::vector<pkgapply_exec::lifecycle_execution_result> after,
    std::optional<pkgstate::state_publication_request> publication_request,
    std::optional<pkgstate::state_publication_receipt> publication_receipt,
    std::optional<pkgapply::application_journal_record> application_journal)
    : session_(std::move(session)), record_(std::move(record)),
      before_(std::move(before)), application_(std::move(application)),
      after_(std::move(after)),
      publication_request_(std::move(publication_request)),
      publication_receipt_(std::move(publication_receipt)),
      application_journal_(std::move(application_journal))
{
}

effect_restart_checkpoint effect_restart_checkpoint::make(
    effectful_operation_session session,
    effect_attempt_record record,
    std::vector<pkgapply_exec::lifecycle_execution_result> before,
    std::optional<pkgapply::application_receipt> application,
    std::vector<pkgapply_exec::lifecycle_execution_result> after,
    std::optional<pkgstate::state_publication_request> publication_request,
    std::optional<pkgstate::state_publication_receipt> publication_receipt,
    std::optional<pkgapply::application_journal_record> application_journal)
{
  if (record.session() != session.identity() ||
      record.before_total() != session.before().size() ||
      record.after_total() != session.after().size())
    invalid_checkpoint("controller journal belongs to another effect session");

  if (before.size() != record.before().size())
    invalid_checkpoint("pre-lifecycle checkpoint evidence is incomplete");
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    if (before[index].identity().hex() != record.before()[index].result().hex() ||
        before[index].succeeded() != record.before()[index].succeeded() ||
        before[index].node().identity() != session.before()[index].node().identity())
      invalid_checkpoint("pre-lifecycle checkpoint evidence differs from journal");
  }

  const bool ahead_application =
      application.has_value() && !record.application().has_value() &&
      record.stage() == effect_attempt_stage::application_intent &&
      application_journal.has_value();
  if (application.has_value() != record.application().has_value() &&
      !ahead_application)
    invalid_checkpoint("application checkpoint presence differs from journal");
  if (application)
  {
    if (ahead_application)
    {
      validate_application_journal(session, *application_journal);
      validate_ahead_application_receipt(
          session, *application, *application_journal);
    }
    else
    {
      validate_application_receipt(session, *application);
      const auto& fact = *record.application();
      const std::string journal_identity = application->journal()
          ? application->journal()->string() : std::string();
      const std::string completed_identity = application->completed_evidence()
          ? application->completed_evidence()->identity().string() : std::string();
      if (application->identity().string() != fact.receipt() ||
          application->outcome() != fact.outcome() ||
          journal_identity != fact.journal().value_or(std::string()) ||
          completed_identity != fact.completed_evidence().value_or(std::string()))
        invalid_checkpoint("application checkpoint evidence differs from journal");
    }
  }

  if (after.size() != record.after().size())
    invalid_checkpoint("post-lifecycle checkpoint evidence is incomplete");
  for (std::size_t index = 0; index < after.size(); ++index)
  {
    if (after[index].identity().hex() != record.after()[index].result().hex() ||
        after[index].succeeded() != record.after()[index].succeeded() ||
        after[index].node().identity() != session.after()[index].node().identity())
      invalid_checkpoint("post-lifecycle checkpoint evidence differs from journal");
  }

  if (publication_request.has_value() != record.publication_request().has_value())
    invalid_checkpoint("publication request checkpoint differs from journal");
  if (publication_request &&
      publication_request->identity().string() != *record.publication_request())
    invalid_checkpoint("publication request identity differs from journal");
  if (publication_receipt.has_value() != record.publication().has_value())
    invalid_checkpoint("publication receipt checkpoint differs from journal");
  if (publication_receipt)
  {
    if (publication_receipt->identity().string() !=
            record.publication()->receipt() ||
        publication_receipt->outcome() != record.publication()->outcome())
      invalid_checkpoint("publication receipt evidence differs from journal");
  }

  if (application_journal)
  {
    if (record.stage() != effect_attempt_stage::application_intent)
      invalid_checkpoint(
          "application restart journal appears outside application intent");
    validate_application_journal(session, *application_journal);
    if (record.application() && record.application()->journal() &&
        application_journal->header().identity().string() !=
            *record.application()->journal())
      invalid_checkpoint("application journal identity differs from receipt evidence");
  }

  return effect_restart_checkpoint(
      std::move(session), std::move(record), std::move(before),
      std::move(application), std::move(after),
      std::move(publication_request), std::move(publication_receipt),
      std::move(application_journal));
}

const effectful_operation_session&
effect_restart_checkpoint::session() const noexcept { return session_; }
const effect_attempt_record& effect_restart_checkpoint::record() const noexcept
{ return record_; }
const std::vector<pkgapply_exec::lifecycle_execution_result>&
effect_restart_checkpoint::before() const noexcept { return before_; }
const std::optional<pkgapply::application_receipt>&
effect_restart_checkpoint::application() const noexcept { return application_; }
const std::vector<pkgapply_exec::lifecycle_execution_result>&
effect_restart_checkpoint::after() const noexcept { return after_; }
const std::optional<pkgstate::state_publication_request>&
effect_restart_checkpoint::publication_request() const noexcept
{ return publication_request_; }
const std::optional<pkgstate::state_publication_receipt>&
effect_restart_checkpoint::publication_receipt() const noexcept
{ return publication_receipt_; }
const std::optional<pkgapply::application_journal_record>&
effect_restart_checkpoint::application_journal() const noexcept
{ return application_journal_; }

effect_restart_result::effect_restart_result(
    effect_restart_disposition disposition,
    effect_attempt_record journal,
    std::optional<effectful_operation_result> operation)
    : disposition_(disposition), journal_(std::move(journal)),
      operation_(std::move(operation))
{
}
effect_restart_disposition effect_restart_result::disposition() const noexcept
{ return disposition_; }
const effect_attempt_record& effect_restart_result::journal() const noexcept
{ return journal_; }
const std::optional<effectful_operation_result>&
effect_restart_result::operation() const noexcept { return operation_; }
bool effect_restart_result::terminal() const noexcept
{ return journal_.stage() == effect_attempt_stage::terminal; }
bool effect_restart_result::external_resolution_required() const noexcept
{
  return disposition_ == effect_restart_disposition::external_resolution_required;
}

} // namespace pkgctl
