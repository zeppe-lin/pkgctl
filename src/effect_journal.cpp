// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_journal.h>

#include "effect_application_classification.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_record(const std::string& message)
{
  throw effect_journal_error(
      effect_journal_error_code::invalid_record, message);
}

[[noreturn]] void invalid_transition(const std::string& message)
{
  throw effect_journal_error(
      effect_journal_error_code::invalid_transition, message);
}

bool lowercase_hex(std::string_view value)
{
  if (value.size() != 64U)
    return false;
  return std::all_of(value.begin(), value.end(), [](const char digit) {
    return (digit >= '0' && digit <= '9') ||
           (digit >= 'a' && digit <= 'f');
  });
}

template<typename Identity>
void require_typed_identity(const std::optional<std::string>& value,
                            const char* label)
{
  if (!value)
    return;
  try
  {
    (void)Identity::parse(*value);
  }
  catch (const std::exception&)
  {
    invalid_record(std::string(label) + " is not a canonical identity");
  }
}

template<typename Identity>
void require_typed_identity(const std::string& value, const char* label)
{
  require_typed_identity<Identity>(
      std::optional<std::string>(value), label);
}

std::string bool_text(bool value) { return value ? "1" : "0"; }

std::vector<std::string> identity_fields(
    const session_identity& attempt,
    const session_identity& session,
    const effect_attempt_nonce& nonce,
    std::uint64_t sequence,
    const std::optional<session_identity>& previous,
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_stage stage,
    const std::optional<std::size_t>& active_index,
    const std::vector<effect_lifecycle_fact>& before,
    const std::optional<effect_application_fact>& application,
    const std::vector<effect_lifecycle_fact>& after,
    const std::optional<std::string>& transaction_evidence,
    const std::optional<std::string>& publication_request,
    const std::optional<effect_publication_fact>& publication,
    const std::optional<effectful_operation_outcome>& terminal_outcome,
    const std::optional<std::string>& reconciled_state)
{
  std::vector<std::string> fields{
      attempt.hex(), session.hex(), nonce.hex(), std::to_string(sequence),
      previous ? previous->hex() : std::string(),
      std::to_string(before_total), std::to_string(after_total),
      std::to_string(static_cast<unsigned int>(stage)),
      active_index ? std::to_string(*active_index) : std::string(),
      std::to_string(before.size())};
  for (const auto& fact : before)
  {
    fields.push_back(fact.result().hex());
    fields.push_back(bool_text(fact.succeeded()));
  }
  fields.push_back(application ? application->receipt() : std::string());
  fields.push_back(application
                       ? std::to_string(static_cast<unsigned int>(
                             application->outcome()))
                       : std::string());
  fields.push_back(application && application->journal()
                       ? *application->journal()
                       : std::string());
  fields.push_back(application && application->completed_evidence()
                       ? *application->completed_evidence()
                       : std::string());
  fields.push_back(std::to_string(after.size()));
  for (const auto& fact : after)
  {
    fields.push_back(fact.result().hex());
    fields.push_back(bool_text(fact.succeeded()));
  }
  fields.push_back(transaction_evidence.value_or(std::string()));
  fields.push_back(publication_request.value_or(std::string()));
  fields.push_back(publication ? publication->receipt() : std::string());
  fields.push_back(publication
                       ? std::to_string(static_cast<unsigned int>(
                             publication->outcome()))
                       : std::string());
  fields.push_back(publication && publication->resulting_snapshot()
                       ? *publication->resulting_snapshot()
                       : std::string());
  fields.push_back(terminal_outcome
                       ? std::to_string(static_cast<unsigned int>(
                             *terminal_outcome))
                       : std::string());
  fields.push_back(reconciled_state.value_or(std::string()));
  return fields;
}

std::uint64_t retained_transition_count(
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_stage stage,
    const std::vector<effect_lifecycle_fact>& before,
    const std::optional<effect_application_fact>& application,
    const std::vector<effect_lifecycle_fact>& after,
    const std::optional<std::string>& transaction_evidence,
    const std::optional<effect_publication_fact>& publication,
    const std::optional<effectful_operation_outcome>& terminal_outcome)
{
  const auto before_count = static_cast<std::uint64_t>(before.size());
  const auto after_count = static_cast<std::uint64_t>(after.size());
  const auto application_base =
      static_cast<std::uint64_t>(before_total) * 2U;
  const auto after_base = application_base + 2U;
  const auto publication_base =
      after_base + static_cast<std::uint64_t>(after_total) * 2U;

  switch (stage)
  {
    case effect_attempt_stage::admitted:
      return 0U;
    case effect_attempt_stage::before_lifecycle_intent:
      return before_count * 2U + 1U;
    case effect_attempt_stage::before_lifecycle_terminal:
      return before_count * 2U;
    case effect_attempt_stage::application_intent:
      return application_base + 1U;
    case effect_attempt_stage::application_terminal:
      return application_base + 2U;
    case effect_attempt_stage::after_lifecycle_intent:
      return after_base + after_count * 2U + 1U;
    case effect_attempt_stage::after_lifecycle_terminal:
      return after_base + after_count * 2U;
    case effect_attempt_stage::publication_intent:
      return publication_base + 1U;
    case effect_attempt_stage::publication_terminal:
      return publication_base + 2U;
    case effect_attempt_stage::terminal:
      break;
  }

  switch (*terminal_outcome)
  {
    case effectful_operation_outcome::lifecycle_failed_before_application:
      return before_count * 2U + 1U;
    case effectful_operation_outcome::application_not_completed:
      return application_base + 3U;
    case effectful_operation_outcome::lifecycle_failed_after_application:
      return after_base + after_count * 2U + 1U;
    case effectful_operation_outcome::state_publication_not_completed:
    case effectful_operation_outcome::state_publication_indeterminate:
      return publication_base + 3U;
    case effectful_operation_outcome::completed:
      return publication_base + (publication ? 3U : 2U);
    case effectful_operation_outcome::application_resolution_required:
      invalid_record(
          "application resolution requirement cannot be a terminal journal outcome");
    case effectful_operation_outcome::outer_lease_lost:
      if (publication)
        return publication_base + 3U;
      if (transaction_evidence)
        invalid_record(
            "lease-loss terminal record follows unresolved publication intent");
      if (!after.empty())
        return after_base + after_count * 2U + 1U;
      if (application)
        return application_base + 3U;
      if (!before.empty())
        return before_count * 2U + 1U;
      return 1U;
  }
  invalid_record("effect-attempt journal has an invalid terminal outcome");
}

void validate_record_shape(
    const session_identity& identity,
    const session_identity& attempt,
    const session_identity& session,
    const effect_attempt_nonce& nonce,
    std::uint64_t sequence,
    const std::optional<session_identity>& previous,
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_stage stage,
    const std::optional<std::size_t>& active_index,
    const std::vector<effect_lifecycle_fact>& before,
    const std::optional<effect_application_fact>& application,
    const std::vector<effect_lifecycle_fact>& after,
    const std::optional<std::string>& transaction_evidence,
    const std::optional<std::string>& publication_request,
    const std::optional<effect_publication_fact>& publication,
    const std::optional<effectful_operation_outcome>& terminal_outcome,
    const std::optional<std::string>& reconciled_state)
{
  if (before.size() > before_total || after.size() > after_total)
    invalid_record("lifecycle evidence exceeds the admitted session shape");
  if ((sequence == 0U) != !previous.has_value())
    invalid_record("journal sequence and predecessor presence disagree");
  if ((sequence == 0U) != (stage == effect_attempt_stage::admitted))
    invalid_record("only sequence zero may represent attempt admission");
  if (stage != effect_attempt_stage::terminal && terminal_outcome)
    invalid_record("nonterminal journal snapshot carries a terminal outcome");
  if (stage == effect_attempt_stage::terminal && !terminal_outcome)
    invalid_record("terminal journal snapshot lacks an outcome");
  if (reconciled_state &&
      (stage != effect_attempt_stage::terminal ||
       *terminal_outcome != effectful_operation_outcome::completed))
    invalid_record("reconciled state is valid only for completed terminal records");

  const bool intent =
      stage == effect_attempt_stage::before_lifecycle_intent ||
      stage == effect_attempt_stage::after_lifecycle_intent;
  if (intent != active_index.has_value())
    invalid_record("lifecycle intent index is absent or appears outside an intent");
  if (stage == effect_attempt_stage::before_lifecycle_intent &&
      *active_index != before.size())
    invalid_record("pre-lifecycle intent index differs from retained evidence");
  if (stage == effect_attempt_stage::after_lifecycle_intent &&
      *active_index != after.size())
    invalid_record("post-lifecycle intent index differs from retained evidence");

  if (stage == effect_attempt_stage::before_lifecycle_terminal && before.empty())
    invalid_record("pre-lifecycle terminal snapshot lacks evidence");
  if (stage == effect_attempt_stage::application_intent && application)
    invalid_record("application intent already carries terminal evidence");
  if (stage == effect_attempt_stage::application_terminal && !application)
    invalid_record("application terminal snapshot lacks evidence");
  if ((stage == effect_attempt_stage::publication_intent ||
       stage == effect_attempt_stage::publication_terminal) &&
      (!transaction_evidence || !publication_request))
    invalid_record("publication boundary lacks transaction or request identity");
  if (stage == effect_attempt_stage::publication_intent && publication)
    invalid_record("publication intent already carries terminal evidence");
  if (stage == effect_attempt_stage::publication_terminal && !publication)
    invalid_record("publication terminal snapshot lacks evidence");

  if (application)
  {
    require_typed_identity<pkgapply::application_receipt_identity>(
        application->receipt(), "application receipt identity");
    require_typed_identity<pkgapply::application_journal_identity>(
        application->journal(), "application journal identity");
    require_typed_identity<pkgapply::completed_application_evidence_identity>(
        application->completed_evidence(),
        "completed application evidence identity");
    if (application->outcome() ==
            pkgapply::application_attempt_outcome::completed &&
        !application->completed_evidence())
      invalid_record("completed application fact lacks completed evidence");
    if (application->outcome() !=
            pkgapply::application_attempt_outcome::completed &&
        application->completed_evidence())
      invalid_record("failed application fact carries completed evidence");
  }
  require_typed_identity<pkgstate::transaction_evidence_identity>(
      transaction_evidence, "transaction evidence identity");
  require_typed_identity<pkgstate::state_publication_request_identity>(
      publication_request, "publication request identity");
  if (publication)
  {
    require_typed_identity<pkgstate::state_publication_receipt_identity>(
        publication->receipt(), "publication receipt identity");
    require_typed_identity<pkgstate::installed_state_snapshot_identity>(
        publication->resulting_snapshot(), "resulting snapshot identity");
  }
  require_typed_identity<pkgstate::installed_state_snapshot_identity>(
      reconciled_state, "reconciled state identity");

  const auto facts_succeeded = [](const auto& facts) {
    return std::all_of(facts.begin(), facts.end(),
                       [](const auto& fact) { return fact.succeeded(); });
  };
  const auto prefix_succeeded = [](const auto& facts) {
    return facts.empty() ||
        std::all_of(facts.begin(), std::prev(facts.end()),
                    [](const auto& fact) { return fact.succeeded(); });
  };
  const bool before_complete =
      before.size() == before_total && facts_succeeded(before);
  const bool application_complete = application &&
      application->outcome() == pkgapply::application_attempt_outcome::completed;
  const bool after_complete =
      after.size() == after_total && facts_succeeded(after);
  const bool has_publication_authority =
      transaction_evidence.has_value() || publication_request.has_value() ||
      publication.has_value();

  if (application && !before_complete)
    invalid_record("application evidence precedes complete pre-lifecycle evidence");
  if (!after.empty() && !application_complete)
    invalid_record("post-lifecycle evidence precedes completed application evidence");
  if (has_publication_authority &&
      (!application_complete || !after_complete ||
       !transaction_evidence || !publication_request))
    invalid_record("publication authority precedes complete operation evidence");
  if (publication && !publication_request)
    invalid_record("publication receipt lacks a request identity");

  switch (stage)
  {
    case effect_attempt_stage::admitted:
      if (!before.empty() || application || !after.empty() ||
          has_publication_authority)
        invalid_record("admission snapshot carries effect evidence");
      break;
    case effect_attempt_stage::before_lifecycle_intent:
      if (!facts_succeeded(before) || before.size() >= before_total ||
          application || !after.empty() || has_publication_authority)
        invalid_record("pre-lifecycle intent has an invalid causal shape");
      break;
    case effect_attempt_stage::before_lifecycle_terminal:
      if (before.empty() || before.size() > before_total ||
          !prefix_succeeded(before) || application || !after.empty() ||
          has_publication_authority)
        invalid_record("pre-lifecycle terminal evidence has an invalid shape");
      break;
    case effect_attempt_stage::application_intent:
      if (!before_complete || application || !after.empty() ||
          has_publication_authority)
        invalid_record("application intent has an invalid causal shape");
      break;
    case effect_attempt_stage::application_terminal:
      if (!before_complete || !application || !after.empty() ||
          has_publication_authority)
        invalid_record("application terminal evidence has an invalid shape");
      break;
    case effect_attempt_stage::after_lifecycle_intent:
      if (!before_complete || !application_complete ||
          !facts_succeeded(after) || after.size() >= after_total ||
          has_publication_authority)
        invalid_record("post-lifecycle intent has an invalid causal shape");
      break;
    case effect_attempt_stage::after_lifecycle_terminal:
      if (!before_complete || !application_complete || after.empty() ||
          after.size() > after_total || !prefix_succeeded(after) ||
          has_publication_authority)
        invalid_record("post-lifecycle terminal evidence has an invalid shape");
      break;
    case effect_attempt_stage::publication_intent:
      if (!before_complete || !application_complete || !after_complete ||
          !transaction_evidence || !publication_request || publication)
        invalid_record("publication intent has an invalid causal shape");
      break;
    case effect_attempt_stage::publication_terminal:
      if (!before_complete || !application_complete || !after_complete ||
          !transaction_evidence || !publication_request || !publication)
        invalid_record("publication terminal evidence has an invalid shape");
      break;
    case effect_attempt_stage::terminal:
      switch (*terminal_outcome)
      {
        case effectful_operation_outcome::lifecycle_failed_before_application:
          if (before.empty() || before.back().succeeded() || application ||
              !after.empty() || has_publication_authority)
            invalid_record("pre-lifecycle failure lacks exact failed evidence");
          break;
        case effectful_operation_outcome::application_not_completed:
          if (!application || application_complete || !after.empty() ||
              has_publication_authority ||
              detail::classify_application_effect(application->outcome()) !=
                  detail::application_effect_classification::definitive_failure)
            invalid_record("application failure lacks exact definitive evidence");
          break;
        case effectful_operation_outcome::application_resolution_required:
          invalid_record(
              "application resolution requirement cannot be a terminal journal outcome");
        case effectful_operation_outcome::lifecycle_failed_after_application:
          if (!application_complete || after.empty() ||
              after.back().succeeded() || has_publication_authority)
            invalid_record("post-lifecycle failure lacks exact failed evidence");
          break;
        case effectful_operation_outcome::outer_lease_lost:
          if (active_index ||
              (publication_request && !transaction_evidence))
            invalid_record("lease-loss terminal record retains unresolved intent");
          break;
        case effectful_operation_outcome::state_publication_not_completed:
          if (!publication ||
              (publication->outcome() !=
                   pkgstate::state_publication_outcome::stale_expected_state &&
               publication->outcome() !=
                   pkgstate::state_publication_outcome::request_rejected &&
               publication->outcome() !=
                   pkgstate::state_publication_outcome::failed_before_publication))
            invalid_record("publication failure outcome lacks exact evidence");
          break;
        case effectful_operation_outcome::state_publication_indeterminate:
          if (!publication ||
              (publication->outcome() !=
                   pkgstate::state_publication_outcome::
                       published_durability_unconfirmed &&
               publication->outcome() !=
                   pkgstate::state_publication_outcome::indeterminate))
            invalid_record("indeterminate publication lacks exact evidence");
          break;
        case effectful_operation_outcome::completed:
        {
          const bool published = publication &&
              publication->outcome() ==
                  pkgstate::state_publication_outcome::published;
          const bool reconciled = reconciled_state.has_value() &&
              (!publication ||
               publication->outcome() ==
                   pkgstate::state_publication_outcome::
                       published_durability_unconfirmed ||
               publication->outcome() ==
                   pkgstate::state_publication_outcome::indeterminate);
          if (!published && !reconciled)
            invalid_record("completed attempt lacks published or reconciled state");
          break;
        }
      }
      break;
  }

  if (sequence != retained_transition_count(
          before_total, after_total, stage, before, application, after,
          transaction_evidence, publication, terminal_outcome))
    invalid_record(
        "effect-attempt journal sequence disagrees with retained history");

  const session_identity expected = make_session_identity(
      "pkgctl/effect-attempt-record/1",
      identity_fields(attempt, session, nonce, sequence, previous,
                      before_total, after_total, stage, active_index, before,
                      application, after, transaction_evidence,
                      publication_request, publication, terminal_outcome,
                      reconciled_state));
  if (identity != expected)
    invalid_record("journal record identity does not match its content");
}

bool same_lifecycle_fact(
    const effect_lifecycle_fact& lhs,
    const effect_lifecycle_fact& rhs) noexcept
{
  return lhs.result() == rhs.result() && lhs.succeeded() == rhs.succeeded();
}

bool same_lifecycle_facts(
    const std::vector<effect_lifecycle_fact>& lhs,
    const std::vector<effect_lifecycle_fact>& rhs) noexcept
{
  return lhs.size() == rhs.size() &&
      std::equal(lhs.begin(), lhs.end(), rhs.begin(), same_lifecycle_fact);
}

bool appended_lifecycle_fact(
    const std::vector<effect_lifecycle_fact>& previous,
    const std::vector<effect_lifecycle_fact>& next) noexcept
{
  return next.size() == previous.size() + 1U &&
      std::equal(previous.begin(), previous.end(), next.begin(),
                 same_lifecycle_fact);
}

bool same_application_fact(
    const std::optional<effect_application_fact>& lhs,
    const std::optional<effect_application_fact>& rhs) noexcept
{
  if (lhs.has_value() != rhs.has_value())
    return false;
  return !lhs ||
      (lhs->receipt() == rhs->receipt() &&
       lhs->outcome() == rhs->outcome() &&
       lhs->journal() == rhs->journal() &&
       lhs->completed_evidence() == rhs->completed_evidence());
}

bool same_publication_fact(
    const std::optional<effect_publication_fact>& lhs,
    const std::optional<effect_publication_fact>& rhs) noexcept
{
  if (lhs.has_value() != rhs.has_value())
    return false;
  return !lhs ||
      (lhs->receipt() == rhs->receipt() &&
       lhs->outcome() == rhs->outcome() &&
       lhs->resulting_snapshot() == rhs->resulting_snapshot());
}

bool same_effect_evidence(
    const effect_attempt_record& lhs,
    const effect_attempt_record& rhs) noexcept
{
  return same_lifecycle_facts(lhs.before(), rhs.before()) &&
      same_application_fact(lhs.application(), rhs.application()) &&
      same_lifecycle_facts(lhs.after(), rhs.after()) &&
      lhs.transaction_evidence() == rhs.transaction_evidence() &&
      lhs.publication_request() == rhs.publication_request() &&
      same_publication_fact(lhs.publication(), rhs.publication());
}

void require_same_effect_evidence(
    const effect_attempt_record& previous,
    const effect_attempt_record& next,
    const char* message)
{
  if (!same_effect_evidence(previous, next))
    invalid_transition(message);
}

void validate_effect_successor(
    const effect_attempt_record& previous,
    const effect_attempt_record& next)
{
  if (previous.stage() == effect_attempt_stage::terminal)
    invalid_transition("terminal controller attempt cannot advance");
  if (previous.sequence() == std::numeric_limits<std::uint64_t>::max() ||
      next.sequence() != previous.sequence() + 1U ||
      !next.previous() || *next.previous() != previous.identity())
    invalid_transition("controller journal sequence chain is invalid");
  if (next.attempt() != previous.attempt() ||
      next.session() != previous.session() ||
      next.nonce() != previous.nonce() ||
      next.before_total() != previous.before_total() ||
      next.after_total() != previous.after_total())
    invalid_transition("controller journal successor changes attempt authority");

  switch (next.stage())
  {
    case effect_attempt_stage::admitted:
      invalid_transition("attempt admission cannot be a successor");

    case effect_attempt_stage::before_lifecycle_intent:
    {
      const bool allowed =
          previous.stage() == effect_attempt_stage::admitted ||
          (previous.stage() == effect_attempt_stage::before_lifecycle_terminal &&
           !previous.before().empty() && previous.before().back().succeeded());
      if (!allowed || next.active_index() != previous.before().size())
        invalid_transition("invalid pre-lifecycle intent successor");
      require_same_effect_evidence(
          previous, next, "pre-lifecycle intent rewrites retained evidence");
      return;
    }

    case effect_attempt_stage::before_lifecycle_terminal:
      if (previous.stage() != effect_attempt_stage::before_lifecycle_intent ||
          !appended_lifecycle_fact(previous.before(), next.before()) ||
          !same_application_fact(previous.application(), next.application()) ||
          !same_lifecycle_facts(previous.after(), next.after()) ||
          previous.transaction_evidence() != next.transaction_evidence() ||
          previous.publication_request() != next.publication_request() ||
          !same_publication_fact(previous.publication(), next.publication()))
        invalid_transition(
            "pre-lifecycle terminal successor is not one exact evidence append");
      return;

    case effect_attempt_stage::application_intent:
    {
      const bool allowed =
          (previous.before_total() == 0U &&
           previous.stage() == effect_attempt_stage::admitted) ||
          previous.stage() == effect_attempt_stage::before_lifecycle_terminal;
      if (!allowed)
        invalid_transition("invalid application intent successor");
      require_same_effect_evidence(
          previous, next, "application intent rewrites retained evidence");
      return;
    }

    case effect_attempt_stage::application_terminal:
      if (previous.stage() != effect_attempt_stage::application_intent ||
          !same_lifecycle_facts(previous.before(), next.before()) ||
          previous.application() || !next.application() ||
          !same_lifecycle_facts(previous.after(), next.after()) ||
          previous.transaction_evidence() != next.transaction_evidence() ||
          previous.publication_request() != next.publication_request() ||
          !same_publication_fact(previous.publication(), next.publication()))
        invalid_transition(
            "application terminal successor does not add exact evidence");
      return;

    case effect_attempt_stage::after_lifecycle_intent:
    {
      const bool allowed =
          previous.stage() == effect_attempt_stage::application_terminal ||
          (previous.stage() == effect_attempt_stage::after_lifecycle_terminal &&
           !previous.after().empty() && previous.after().back().succeeded());
      if (!allowed || next.active_index() != previous.after().size())
        invalid_transition("invalid post-lifecycle intent successor");
      require_same_effect_evidence(
          previous, next, "post-lifecycle intent rewrites retained evidence");
      return;
    }

    case effect_attempt_stage::after_lifecycle_terminal:
      if (previous.stage() != effect_attempt_stage::after_lifecycle_intent ||
          !same_lifecycle_facts(previous.before(), next.before()) ||
          !same_application_fact(previous.application(), next.application()) ||
          !appended_lifecycle_fact(previous.after(), next.after()) ||
          previous.transaction_evidence() != next.transaction_evidence() ||
          previous.publication_request() != next.publication_request() ||
          !same_publication_fact(previous.publication(), next.publication()))
        invalid_transition(
            "post-lifecycle terminal successor is not one exact evidence append");
      return;

    case effect_attempt_stage::publication_intent:
    {
      const bool allowed =
          (previous.after_total() == 0U &&
           previous.stage() == effect_attempt_stage::application_terminal) ||
          previous.stage() == effect_attempt_stage::after_lifecycle_terminal;
      if (!allowed || previous.transaction_evidence() ||
          previous.publication_request() || previous.publication() ||
          !same_lifecycle_facts(previous.before(), next.before()) ||
          !same_application_fact(previous.application(), next.application()) ||
          !same_lifecycle_facts(previous.after(), next.after()) ||
          next.publication())
        invalid_transition("publication intent does not add exact authority");
      return;
    }

    case effect_attempt_stage::publication_terminal:
      if (previous.stage() != effect_attempt_stage::publication_intent ||
          !same_lifecycle_facts(previous.before(), next.before()) ||
          !same_application_fact(previous.application(), next.application()) ||
          !same_lifecycle_facts(previous.after(), next.after()) ||
          previous.transaction_evidence() != next.transaction_evidence() ||
          previous.publication_request() != next.publication_request() ||
          previous.publication() || !next.publication())
        invalid_transition(
            "publication terminal successor does not add exact evidence");
      return;

    case effect_attempt_stage::terminal:
    {
      require_same_effect_evidence(
          previous, next, "terminal successor rewrites retained evidence");
      switch (*next.terminal_outcome())
      {
        case effectful_operation_outcome::lifecycle_failed_before_application:
          if (previous.stage() !=
              effect_attempt_stage::before_lifecycle_terminal)
            invalid_transition(
                "pre-lifecycle failure has the wrong predecessor");
          return;
        case effectful_operation_outcome::application_not_completed:
          if (previous.stage() != effect_attempt_stage::application_terminal ||
              !previous.application() ||
              detail::classify_application_effect(
                  previous.application()->outcome()) !=
                  detail::application_effect_classification::definitive_failure)
            invalid_transition("application failure has the wrong predecessor");
          return;
        case effectful_operation_outcome::application_resolution_required:
          invalid_transition(
              "application resolution requirement cannot seal a terminal journal");
        case effectful_operation_outcome::lifecycle_failed_after_application:
          if (previous.stage() !=
              effect_attempt_stage::after_lifecycle_terminal)
            invalid_transition(
                "post-lifecycle failure has the wrong predecessor");
          return;
        case effectful_operation_outcome::outer_lease_lost:
          if (previous.stage() == effect_attempt_stage::before_lifecycle_intent ||
              previous.stage() == effect_attempt_stage::application_intent ||
              previous.stage() == effect_attempt_stage::after_lifecycle_intent ||
              previous.stage() == effect_attempt_stage::publication_intent)
            invalid_transition("lease loss cannot resolve an active intent");
          return;
        case effectful_operation_outcome::state_publication_not_completed:
        case effectful_operation_outcome::state_publication_indeterminate:
          if (previous.stage() != effect_attempt_stage::publication_terminal)
            invalid_transition("publication result has the wrong predecessor");
          return;
        case effectful_operation_outcome::completed:
          if (previous.stage() != effect_attempt_stage::publication_intent &&
              previous.stage() != effect_attempt_stage::publication_terminal)
            invalid_transition("completed attempt has the wrong predecessor");
          return;
      }
    }
  }
}

bool all_succeeded(const std::vector<effect_lifecycle_fact>& facts)
{
  return std::all_of(facts.begin(), facts.end(),
                     [](const auto& fact) { return fact.succeeded(); });
}

} // namespace

effect_journal_error::effect_journal_error(
    effect_journal_error_code code, std::string message, int system_error)
    : std::runtime_error(std::move(message)), code_(code),
      system_error_(system_error)
{
}
effect_journal_error_code effect_journal_error::code() const noexcept
{ return code_; }
int effect_journal_error::system_error() const noexcept { return system_error_; }

effect_attempt_nonce::effect_attempt_nonce(byte_array bytes)
    : bytes_(std::move(bytes))
{
}
effect_attempt_nonce effect_attempt_nonce::from_bytes(byte_array bytes)
{
  if (std::all_of(bytes.begin(), bytes.end(),
                  [](const auto value) { return value == 0U; }))
    throw effect_journal_error(effect_journal_error_code::invalid_nonce,
                               "controller attempt nonce cannot be all zero");
  return effect_attempt_nonce(std::move(bytes));
}
effect_attempt_nonce effect_attempt_nonce::from_hex(std::string value)
{
  if (!lowercase_hex(value))
    throw effect_journal_error(effect_journal_error_code::invalid_nonce,
                               "controller attempt nonce is not canonical hex");
  byte_array bytes{};
  const auto digit = [](const char value) -> std::uint8_t {
    return value <= '9' ? static_cast<std::uint8_t>(value - '0')
                        : static_cast<std::uint8_t>(value - 'a' + 10);
  };
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(
        (digit(value[index * 2U]) << 4U) | digit(value[index * 2U + 1U]));
  return from_bytes(bytes);
}
const effect_attempt_nonce::byte_array&
effect_attempt_nonce::bytes() const noexcept { return bytes_; }
std::string effect_attempt_nonce::hex() const
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes_.size() * 2U);
  for (const auto byte : bytes_)
  {
    result.push_back(digits[(byte >> 4U) & 0x0fU]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}
bool operator==(const effect_attempt_nonce& lhs,
                const effect_attempt_nonce& rhs) noexcept
{ return lhs.bytes_ == rhs.bytes_; }
bool operator!=(const effect_attempt_nonce& lhs,
                const effect_attempt_nonce& rhs) noexcept
{ return !(lhs == rhs); }

effect_lifecycle_fact::effect_lifecycle_fact(
    session_identity result, bool succeeded)
    : result_(std::move(result)), succeeded_(succeeded)
{
}
const session_identity& effect_lifecycle_fact::result() const noexcept
{ return result_; }
bool effect_lifecycle_fact::succeeded() const noexcept { return succeeded_; }

effect_application_fact::effect_application_fact(
    std::string receipt,
    pkgapply::application_attempt_outcome outcome,
    std::optional<std::string> journal,
    std::optional<std::string> completed_evidence)
    : receipt_(std::move(receipt)), outcome_(outcome),
      journal_(std::move(journal)),
      completed_evidence_(std::move(completed_evidence))
{
}
const std::string& effect_application_fact::receipt() const noexcept
{ return receipt_; }
pkgapply::application_attempt_outcome
effect_application_fact::outcome() const noexcept { return outcome_; }
const std::optional<std::string>& effect_application_fact::journal() const noexcept
{ return journal_; }
const std::optional<std::string>&
effect_application_fact::completed_evidence() const noexcept
{ return completed_evidence_; }

effect_publication_fact::effect_publication_fact(
    std::string receipt,
    pkgstate::state_publication_outcome outcome,
    std::optional<std::string> resulting_snapshot)
    : receipt_(std::move(receipt)), outcome_(outcome),
      resulting_snapshot_(std::move(resulting_snapshot))
{
}
const std::string& effect_publication_fact::receipt() const noexcept
{ return receipt_; }
pkgstate::state_publication_outcome
effect_publication_fact::outcome() const noexcept { return outcome_; }
const std::optional<std::string>&
effect_publication_fact::resulting_snapshot() const noexcept
{ return resulting_snapshot_; }

effect_attempt_record::effect_attempt_record(
    session_identity identity,
    session_identity attempt,
    session_identity session,
    effect_attempt_nonce nonce,
    std::uint64_t sequence,
    std::optional<session_identity> previous,
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_stage stage,
    std::optional<std::size_t> active_index,
    std::vector<effect_lifecycle_fact> before,
    std::optional<effect_application_fact> application,
    std::vector<effect_lifecycle_fact> after,
    std::optional<std::string> transaction_evidence,
    std::optional<std::string> publication_request,
    std::optional<effect_publication_fact> publication,
    std::optional<effectful_operation_outcome> terminal_outcome,
    std::optional<std::string> reconciled_state)
    : identity_(std::move(identity)), attempt_(std::move(attempt)),
      session_(std::move(session)), nonce_(std::move(nonce)),
      sequence_(sequence), previous_(std::move(previous)),
      before_total_(before_total), after_total_(after_total), stage_(stage),
      active_index_(active_index), before_(std::move(before)),
      application_(std::move(application)), after_(std::move(after)),
      transaction_evidence_(std::move(transaction_evidence)),
      publication_request_(std::move(publication_request)),
      publication_(std::move(publication)),
      terminal_outcome_(terminal_outcome),
      reconciled_state_(std::move(reconciled_state))
{
}

effect_attempt_record effect_attempt_record::restore(
    session_identity identity,
    session_identity attempt,
    session_identity session,
    effect_attempt_nonce nonce,
    std::uint64_t sequence,
    std::optional<session_identity> previous,
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_stage stage,
    std::optional<std::size_t> active_index,
    std::vector<effect_lifecycle_fact> before,
    std::optional<effect_application_fact> application,
    std::vector<effect_lifecycle_fact> after,
    std::optional<std::string> transaction_evidence,
    std::optional<std::string> publication_request,
    std::optional<effect_publication_fact> publication,
    std::optional<effectful_operation_outcome> terminal_outcome,
    std::optional<std::string> reconciled_state)
{
  validate_record_shape(identity, attempt, session, nonce, sequence, previous,
                        before_total, after_total, stage, active_index, before,
                        application, after, transaction_evidence,
                        publication_request, publication, terminal_outcome,
                        reconciled_state);
  return effect_attempt_record(
      std::move(identity), std::move(attempt), std::move(session),
      std::move(nonce), sequence, std::move(previous), before_total,
      after_total, stage, active_index, std::move(before),
      std::move(application), std::move(after),
      std::move(transaction_evidence), std::move(publication_request),
      std::move(publication), terminal_outcome, std::move(reconciled_state));
}

effect_attempt_record effect_attempt_record::admit(
    const session_identity& session,
    std::size_t before_total,
    std::size_t after_total,
    effect_attempt_nonce nonce)
{
  if (before_total > maximum_effect_lifecycle_count ||
      after_total > maximum_effect_lifecycle_count)
    invalid_record("lifecycle count exceeds journal encoding bounds");
  const session_identity attempt = make_session_identity(
      "pkgctl/effect-attempt/1", {session.hex(), nonce.hex()});
  const auto fields = identity_fields(
      attempt, session, nonce, 0U, std::nullopt, before_total, after_total,
      effect_attempt_stage::admitted, std::nullopt, {}, std::nullopt, {},
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
  return restore(
      make_session_identity("pkgctl/effect-attempt-record/1", fields),
      attempt, session, std::move(nonce), 0U, std::nullopt, before_total,
      after_total, effect_attempt_stage::admitted, std::nullopt, {},
      std::nullopt, {}, std::nullopt, std::nullopt, std::nullopt,
      std::nullopt, std::nullopt);
}

effect_attempt_record effect_attempt_record::successor(
    effect_attempt_stage stage,
    std::optional<std::size_t> active_index,
    std::vector<effect_lifecycle_fact> before,
    std::optional<effect_application_fact> application,
    std::vector<effect_lifecycle_fact> after,
    std::optional<std::string> transaction_evidence,
    std::optional<std::string> publication_request,
    std::optional<effect_publication_fact> publication,
    std::optional<effectful_operation_outcome> terminal_outcome,
    std::optional<std::string> reconciled_state) const
{
  if (stage_ == effect_attempt_stage::terminal)
    invalid_transition("terminal controller attempt cannot advance");
  if (sequence_ == std::numeric_limits<std::uint64_t>::max())
    invalid_transition("controller journal sequence is exhausted");
  const std::uint64_t next_sequence = sequence_ + 1U;
  const std::optional<session_identity> previous(identity_);
  const auto fields = identity_fields(
      attempt_, session_, nonce_, next_sequence, previous,
      before_total_, after_total_, stage, active_index, before, application,
      after, transaction_evidence, publication_request, publication,
      terminal_outcome, reconciled_state);
  auto result = restore(
      make_session_identity("pkgctl/effect-attempt-record/1", fields),
      attempt_, session_, nonce_, next_sequence, previous, before_total_,
      after_total_, stage, active_index, std::move(before),
      std::move(application), std::move(after),
      std::move(transaction_evidence), std::move(publication_request),
      std::move(publication), terminal_outcome, std::move(reconciled_state));
  result.validate_successor_of(*this);
  return result;
}

effect_attempt_record effect_attempt_record::begin_before(std::size_t index) const
{
  const bool allowed = stage_ == effect_attempt_stage::admitted ||
      (stage_ == effect_attempt_stage::before_lifecycle_terminal &&
       !before_.empty() && before_.back().succeeded());
  if (!allowed || index != before_.size() || index >= before_total_ ||
      application_ || !after_.empty())
    invalid_transition("invalid pre-lifecycle intent transition");
  return successor(effect_attempt_stage::before_lifecycle_intent, index,
                   before_, application_, after_, transaction_evidence_,
                   publication_request_, publication_, std::nullopt,
                   std::nullopt);
}

effect_attempt_record effect_attempt_record::complete_before(
    const pkgapply_exec::lifecycle_execution_result& result) const
{
  if (stage_ != effect_attempt_stage::before_lifecycle_intent ||
      !active_index_ || *active_index_ != before_.size())
    invalid_transition("pre-lifecycle terminal evidence lacks matching intent");
  auto before = before_;
  before.emplace_back(session_identity::from_hex(result.identity().hex()),
                      result.succeeded());
  return successor(effect_attempt_stage::before_lifecycle_terminal,
                   std::nullopt, std::move(before), application_, after_,
                   transaction_evidence_, publication_request_, publication_,
                   std::nullopt, std::nullopt);
}

effect_attempt_record effect_attempt_record::begin_application() const
{
  const bool allowed =
      (before_total_ == 0U && stage_ == effect_attempt_stage::admitted) ||
      (stage_ == effect_attempt_stage::before_lifecycle_terminal &&
       before_.size() == before_total_ && all_succeeded(before_));
  if (!allowed || application_ || !after_.empty())
    invalid_transition("application intent does not follow complete pre-lifecycle evidence");
  return successor(effect_attempt_stage::application_intent, std::nullopt,
                   before_, std::nullopt, after_, std::nullopt, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt);
}

effect_attempt_record effect_attempt_record::complete_application(
    const pkgapply::application_receipt& receipt) const
{
  if (stage_ != effect_attempt_stage::application_intent || application_)
    invalid_transition("application terminal evidence lacks matching intent");
  std::optional<std::string> journal;
  if (receipt.journal())
    journal = receipt.journal()->string();
  std::optional<std::string> completed;
  if (receipt.completed_evidence())
    completed = receipt.completed_evidence()->identity().string();
  effect_application_fact application(
      receipt.identity().string(), receipt.outcome(), std::move(journal),
      std::move(completed));
  return successor(effect_attempt_stage::application_terminal, std::nullopt,
                   before_, std::move(application), after_, std::nullopt,
                   std::nullopt, std::nullopt, std::nullopt, std::nullopt);
}

effect_attempt_record effect_attempt_record::begin_after(std::size_t index) const
{
  const bool application_complete = application_ &&
      application_->outcome() == pkgapply::application_attempt_outcome::completed;
  const bool allowed =
      (stage_ == effect_attempt_stage::application_terminal &&
       after_.empty()) ||
      (stage_ == effect_attempt_stage::after_lifecycle_terminal &&
       !after_.empty() && after_.back().succeeded());
  if (!application_complete || !allowed || index != after_.size() ||
      index >= after_total_)
    invalid_transition("invalid post-lifecycle intent transition");
  return successor(effect_attempt_stage::after_lifecycle_intent, index,
                   before_, application_, after_, transaction_evidence_,
                   publication_request_, publication_, std::nullopt,
                   std::nullopt);
}

effect_attempt_record effect_attempt_record::complete_after(
    const pkgapply_exec::lifecycle_execution_result& result) const
{
  if (stage_ != effect_attempt_stage::after_lifecycle_intent ||
      !active_index_ || *active_index_ != after_.size())
    invalid_transition("post-lifecycle terminal evidence lacks matching intent");
  auto after = after_;
  after.emplace_back(session_identity::from_hex(result.identity().hex()),
                     result.succeeded());
  return successor(effect_attempt_stage::after_lifecycle_terminal,
                   std::nullopt, before_, application_, std::move(after),
                   transaction_evidence_, publication_request_, publication_,
                   std::nullopt, std::nullopt);
}

effect_attempt_record effect_attempt_record::begin_publication(
    const pkgstate::transaction_evidence_identity& transaction,
    const pkgstate::state_publication_request& request) const
{
  const bool application_complete = application_ &&
      application_->outcome() == pkgapply::application_attempt_outcome::completed;
  const bool allowed =
      (after_total_ == 0U &&
       stage_ == effect_attempt_stage::application_terminal) ||
      (stage_ == effect_attempt_stage::after_lifecycle_terminal &&
       after_.size() == after_total_ && all_succeeded(after_));
  if (!application_complete || !allowed || transaction_evidence_ ||
      publication_request_ || publication_)
    invalid_transition("publication intent does not follow complete effect evidence");
  return successor(effect_attempt_stage::publication_intent, std::nullopt,
                   before_, application_, after_, transaction.string(),
                   request.identity().string(), std::nullopt, std::nullopt,
                   std::nullopt);
}

effect_attempt_record effect_attempt_record::complete_publication(
    const pkgstate::state_publication_receipt& receipt) const
{
  if (stage_ != effect_attempt_stage::publication_intent || publication_ ||
      !publication_request_ || receipt.request().string() != *publication_request_)
    invalid_transition("publication terminal evidence lacks matching intent");
  std::optional<std::string> resulting;
  if (receipt.resulting_snapshot())
    resulting = receipt.resulting_snapshot()->string();
  effect_publication_fact publication(
      receipt.identity().string(), receipt.outcome(), std::move(resulting));
  return successor(effect_attempt_stage::publication_terminal, std::nullopt,
                   before_, application_, after_, transaction_evidence_,
                   publication_request_, std::move(publication), std::nullopt,
                   std::nullopt);
}

effect_attempt_record effect_attempt_record::seal_terminal(
    effectful_operation_outcome outcome,
    std::optional<pkgstate::installed_state_snapshot_identity> reconciled_state) const
{
  const bool unresolved_intent =
      stage_ == effect_attempt_stage::before_lifecycle_intent ||
      stage_ == effect_attempt_stage::application_intent ||
      stage_ == effect_attempt_stage::after_lifecycle_intent ||
      stage_ == effect_attempt_stage::publication_intent;
  if (outcome == effectful_operation_outcome::outer_lease_lost &&
      unresolved_intent)
    invalid_transition(
        "lease loss cannot resolve an effect with unknown terminal evidence");
  if (outcome == effectful_operation_outcome::application_resolution_required)
    invalid_transition(
        "application resolution requirement cannot seal a terminal journal");
  if (outcome ==
          effectful_operation_outcome::lifecycle_failed_before_application &&
      stage_ != effect_attempt_stage::before_lifecycle_terminal)
    invalid_transition("pre-lifecycle failure does not follow terminal evidence");
  if (outcome == effectful_operation_outcome::application_not_completed &&
      stage_ != effect_attempt_stage::application_terminal)
    invalid_transition("application failure does not follow terminal evidence");
  if (outcome ==
          effectful_operation_outcome::lifecycle_failed_after_application &&
      stage_ != effect_attempt_stage::after_lifecycle_terminal)
    invalid_transition("post-lifecycle failure does not follow terminal evidence");
  if ((outcome ==
           effectful_operation_outcome::state_publication_not_completed ||
       outcome ==
           effectful_operation_outcome::state_publication_indeterminate) &&
      stage_ != effect_attempt_stage::publication_terminal)
    invalid_transition("publication outcome does not follow terminal evidence");
  if (outcome == effectful_operation_outcome::completed &&
      stage_ != effect_attempt_stage::publication_terminal &&
      stage_ != effect_attempt_stage::publication_intent)
    invalid_transition("completed attempt does not follow publication evidence");

  std::optional<std::string> reconciled;
  if (reconciled_state)
    reconciled = reconciled_state->string();
  if (outcome == effectful_operation_outcome::completed)
  {
    const bool published = publication_ &&
        publication_->outcome() == pkgstate::state_publication_outcome::published;
    const bool reconciled_publication = reconciled &&
        (stage_ == effect_attempt_stage::publication_intent ||
         (stage_ == effect_attempt_stage::publication_terminal &&
          publication_ &&
          (publication_->outcome() ==
               pkgstate::state_publication_outcome::published_durability_unconfirmed ||
           publication_->outcome() ==
               pkgstate::state_publication_outcome::indeterminate)));
    if (!published && !reconciled_publication)
      invalid_transition("completed attempt lacks published or reconciled state");
  }
  if (outcome == effectful_operation_outcome::lifecycle_failed_before_application &&
      (before_.empty() || before_.back().succeeded()))
    invalid_transition("pre-lifecycle failure outcome lacks failed evidence");
  if (outcome == effectful_operation_outcome::application_not_completed &&
      (!application_ ||
       detail::classify_application_effect(application_->outcome()) !=
           detail::application_effect_classification::definitive_failure))
    invalid_transition("application failure outcome lacks definitive evidence");
  if (outcome == effectful_operation_outcome::lifecycle_failed_after_application &&
      (after_.empty() || after_.back().succeeded()))
    invalid_transition("post-lifecycle failure outcome lacks failed evidence");
  return successor(effect_attempt_stage::terminal, std::nullopt, before_,
                   application_, after_, transaction_evidence_,
                   publication_request_, publication_, outcome,
                   std::move(reconciled));
}

void effect_attempt_record::validate_successor_of(
    const effect_attempt_record& previous) const
{
  validate_effect_successor(previous, *this);
}

std::uint16_t effect_attempt_record::schema_version() const noexcept
{ return schema_version_; }
const session_identity& effect_attempt_record::identity() const noexcept
{ return identity_; }
const session_identity& effect_attempt_record::attempt() const noexcept
{ return attempt_; }
const session_identity& effect_attempt_record::session() const noexcept
{ return session_; }
const effect_attempt_nonce& effect_attempt_record::nonce() const noexcept
{ return nonce_; }
std::uint64_t effect_attempt_record::sequence() const noexcept
{ return sequence_; }
const std::optional<session_identity>& effect_attempt_record::previous() const noexcept
{ return previous_; }
std::size_t effect_attempt_record::before_total() const noexcept
{ return before_total_; }
std::size_t effect_attempt_record::after_total() const noexcept
{ return after_total_; }
effect_attempt_stage effect_attempt_record::stage() const noexcept
{ return stage_; }
const std::optional<std::size_t>& effect_attempt_record::active_index() const noexcept
{ return active_index_; }
const std::vector<effect_lifecycle_fact>& effect_attempt_record::before() const noexcept
{ return before_; }
const std::optional<effect_application_fact>&
effect_attempt_record::application() const noexcept { return application_; }
const std::vector<effect_lifecycle_fact>& effect_attempt_record::after() const noexcept
{ return after_; }
const std::optional<std::string>&
effect_attempt_record::transaction_evidence() const noexcept
{ return transaction_evidence_; }
const std::optional<std::string>&
effect_attempt_record::publication_request() const noexcept
{ return publication_request_; }
const std::optional<effect_publication_fact>&
effect_attempt_record::publication() const noexcept { return publication_; }
const std::optional<effectful_operation_outcome>&
effect_attempt_record::terminal_outcome() const noexcept
{ return terminal_outcome_; }
const std::optional<std::string>& effect_attempt_record::reconciled_state() const noexcept
{ return reconciled_state_; }

} // namespace pkgctl
