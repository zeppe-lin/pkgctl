// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/report.h>

#include <sstream>
#include <string>
#include <string_view>

namespace pkgctl {
namespace {

std::string scope_text(const pkgsource::requirement_scope& scope)
{
  std::string value(pkgsource::to_string(scope.kind()));
  if (scope.action())
  {
    value.push_back(':');
    value.append(pkgsource::to_string(*scope.action()));
  }
  return value;
}


std::string_view unit_kind_text(transaction_unit_kind kind)
{
  switch (kind)
  {
    case transaction_unit_kind::construction: return "construction";
    case transaction_unit_kind::check: return "check";
    case transaction_unit_kind::operation: return "operation";
  }
  return "unknown";
}

std::string_view dispatch_state_text(transaction_dispatch_state state)
{
  switch (state)
  {
    case transaction_dispatch_state::reserved: return "reserved";
    case transaction_dispatch_state::started: return "started";
    case transaction_dispatch_state::completed: return "completed";
    case transaction_dispatch_state::released_unstarted:
      return "released-unstarted";
  }
  return "unknown";
}

std::string_view restart_disposition_text(
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
  return "unknown";
}

std::string_view inspection_disposition_text(
    transaction_run_inspection_disposition disposition)
{
  switch (disposition)
  {
    case transaction_run_inspection_disposition::completed:
      return "completed";
    case transaction_run_inspection_disposition::stopped_after_failure:
      return "stopped-after-failure";
    case transaction_run_inspection_disposition::active:
      return "active";
    case transaction_run_inspection_disposition::quiescent_incomplete:
      return "quiescent-incomplete";
  }
  return "unknown";
}

std::string_view effect_stage_text(effect_attempt_stage stage)
{
  switch (stage)
  {
    case effect_attempt_stage::admitted: return "admitted";
    case effect_attempt_stage::before_lifecycle_intent:
      return "before-lifecycle-intent";
    case effect_attempt_stage::before_lifecycle_terminal:
      return "before-lifecycle-terminal";
    case effect_attempt_stage::application_intent:
      return "application-intent";
    case effect_attempt_stage::application_terminal:
      return "application-terminal";
    case effect_attempt_stage::after_lifecycle_intent:
      return "after-lifecycle-intent";
    case effect_attempt_stage::after_lifecycle_terminal:
      return "after-lifecycle-terminal";
    case effect_attempt_stage::publication_intent:
      return "publication-intent";
    case effect_attempt_stage::publication_terminal:
      return "publication-terminal";
    case effect_attempt_stage::terminal: return "terminal";
  }
  return "unknown";
}

std::string_view effect_restart_disposition_text(
    effect_restart_disposition disposition)
{
  switch (disposition)
  {
    case effect_restart_disposition::continue_before_lifecycle:
      return "continue-before-lifecycle";
    case effect_restart_disposition::start_application:
      return "start-application";
    case effect_restart_disposition::resume_application:
      return "resume-application";
    case effect_restart_disposition::continue_after_application:
      return "continue-after-application";
    case effect_restart_disposition::continue_after_lifecycle:
      return "continue-after-lifecycle";
    case effect_restart_disposition::start_publication:
      return "start-publication";
    case effect_restart_disposition::reconcile_publication:
      return "reconcile-publication";
    case effect_restart_disposition::seal_terminal:
      return "seal-terminal";
    case effect_restart_disposition::terminal: return "terminal";
    case effect_restart_disposition::external_resolution_required:
      return "external-resolution-required";
  }
  return "unknown";
}

std::string_view application_outcome_text(
    pkgapply::application_attempt_outcome outcome)
{
  switch (outcome)
  {
    case pkgapply::application_attempt_outcome::precondition_refused:
      return "precondition-refused";
    case pkgapply::application_attempt_outcome::failed_before_target_mutation:
      return "failed-before-target-mutation";
    case pkgapply::application_attempt_outcome::completed:
      return "completed";
    case pkgapply::application_attempt_outcome::failed_fully_recovered:
      return "failed-fully-recovered";
    case pkgapply::application_attempt_outcome::failed_with_partial_effects:
      return "failed-with-partial-effects";
    case pkgapply::application_attempt_outcome::
        effects_visible_durability_unconfirmed:
      return "effects-visible-durability-unconfirmed";
    case pkgapply::application_attempt_outcome::indeterminate:
      return "indeterminate";
  }
  return "unknown";
}

std::string_view publication_outcome_text(
    pkgstate::state_publication_outcome outcome)
{
  switch (outcome)
  {
    case pkgstate::state_publication_outcome::published:
      return "published";
    case pkgstate::state_publication_outcome::stale_expected_state:
      return "stale-expected-state";
    case pkgstate::state_publication_outcome::request_rejected:
      return "request-rejected";
    case pkgstate::state_publication_outcome::failed_before_publication:
      return "failed-before-publication";
    case pkgstate::state_publication_outcome::
        published_durability_unconfirmed:
      return "published-durability-unconfirmed";
    case pkgstate::state_publication_outcome::indeterminate:
      return "indeterminate";
  }
  return "unknown";
}

std::string_view effect_outcome_text(effectful_operation_outcome outcome)
{
  switch (outcome)
  {
    case effectful_operation_outcome::lifecycle_failed_before_application:
      return "lifecycle-failed-before-application";
    case effectful_operation_outcome::application_not_completed:
      return "application-not-completed";
    case effectful_operation_outcome::lifecycle_failed_after_application:
      return "lifecycle-failed-after-application";
    case effectful_operation_outcome::outer_lease_lost:
      return "outer-lease-lost";
    case effectful_operation_outcome::state_publication_not_completed:
      return "state-publication-not-completed";
    case effectful_operation_outcome::state_publication_indeterminate:
      return "state-publication-indeterminate";
    case effectful_operation_outcome::completed: return "completed";
  }
  return "unknown";
}

const char* boolean_text(bool value) noexcept
{
  return value ? "true" : "false";
}

void render_catalog_body(std::ostringstream& out,
                         const catalog_session& session)
{
  const pkgcatalog::catalog_snapshot& catalog = session.catalog();
  out << "catalog.identity=" << catalog.identity().hex() << '\n'
      << "catalog.policy=" << pkgcatalog::to_string(catalog.policy()) << '\n'
      << "catalog.profiles=" << catalog.profiles().profiles().size() << '\n'
      << "catalog.collections=" << catalog.collections().size() << '\n'
      << "catalog.candidates=" << catalog.candidates().size() << '\n';

  for (std::size_t index = 0; index < catalog.collections().size(); ++index)
  {
    const auto& value = catalog.collections()[index];
    out << "collection." << index << ".precedence=" << value.precedence() << '\n'
        << "collection." << index << ".name="
        << value.collection().name().name() << '\n'
        << "collection." << index << ".identity="
        << value.collection().identity().hex() << '\n'
        << "collection." << index << ".revision="
        << value.collection().revision_identity().hex() << '\n';
  }

  for (std::size_t index = 0; index < catalog.candidates().size(); ++index)
  {
    const auto& value = catalog.candidates()[index];
    out << "candidate." << index << ".identity=" << value.identity().hex() << '\n'
        << "candidate." << index << ".package=" << value.package().name() << '\n'
        << "candidate." << index << ".release="
        << value.release().version_release() << '\n'
        << "candidate." << index << ".collection="
        << value.collection().name() << '\n'
        << "candidate." << index << ".precedence="
        << value.precedence() << '\n'
        << "candidate." << index << ".status="
        << pkgcatalog::to_string(value.status()) << '\n';
    if (value.shadowed_by())
      out << "candidate." << index << ".shadowed-by="
          << value.shadowed_by()->hex() << '\n';
  }
}

void render_resolution_body(std::ostringstream& out,
                            const resolution_session& session)
{
  const pkgresolve::resolution_result& result = session.resolution();
  out << "state.target-binding="
      << session.installed().target_binding().identity().string() << '\n'
      << "state.snapshot=" << session.installed().identity().string() << '\n'
      << "state.packages=" << session.installed().packages().size() << '\n'
      << "resolution.request=" << result.request().identity().hex() << '\n'
      << "resolution.result=" << result.identity().hex() << '\n'
      << "resolution.preference="
      << pkgresolve::to_string(result.request().policy().preference()) << '\n'
      << "resolution.build-architecture="
      << result.request().architectures().build().name() << '\n'
      << "resolution.target-architecture="
      << result.request().architectures().target().name() << '\n'
      << "resolution.selections=" << result.selections().size() << '\n'
      << "resolution.edges=" << result.edges().size() << '\n'
      << "resolution.goals=" << result.goals().size() << '\n';

  for (std::size_t index = 0; index < result.selections().size(); ++index)
  {
    const auto& value = result.selections()[index];
    out << "selection." << index << ".identity=" << value.identity().hex() << '\n'
        << "selection." << index << ".package=" << value.package().name() << '\n'
        << "selection." << index << ".release="
        << value.release().version_release() << '\n'
        << "selection." << index << ".environment="
        << pkgresolve::to_string(value.environment()) << '\n'
        << "selection." << index << ".authority="
        << pkgresolve::to_string(value.authority_kind()) << '\n'
        << "selection." << index << ".source="
        << value.source_snapshot().hex() << '\n';
  }

  for (std::size_t index = 0; index < result.edges().size(); ++index)
  {
    const auto& value = result.edges()[index];
    out << "requirement-edge." << index << ".identity="
        << value.identity().hex() << '\n'
        << "requirement-edge." << index << ".issuer="
        << value.issuer().hex() << '\n'
        << "requirement-edge." << index << ".required="
        << value.required().hex() << '\n'
        << "requirement-edge." << index << ".scope="
        << scope_text(value.scope()) << '\n'
        << "requirement-edge." << index << ".environment="
        << pkgresolve::to_string(value.environment()) << '\n'
        << "requirement-edge." << index << ".authority="
        << pkgresolve::to_string(value.witness().kind()) << '\n';
  }

  for (std::size_t index = 0; index < result.goals().size(); ++index)
  {
    const auto& value = result.goals()[index];
    out << "goal." << index << ".identity=" << value.identity().hex() << '\n'
        << "goal." << index << ".scope="
        << scope_text(value.goal().scope()) << '\n'
        << "goal." << index << ".subject="
        << value.goal().subject().text() << '\n'
        << "goal." << index << ".members=" << value.members().size() << '\n'
        << "goal." << index << ".selections="
        << value.selections().size() << '\n'
        << "goal." << index << ".edges=" << value.edges().size() << '\n';
  }
}

} // namespace

std::string render_report(const catalog_session& session)
{
  std::ostringstream out;
  out << "session.kind=catalog\n"
      << "session.identity=" << session.identity().hex() << '\n';
  render_catalog_body(out, session);
  return out.str();
}

std::string render_report(const resolution_session& session)
{
  std::ostringstream out;
  out << "session.kind=resolution\n"
      << "session.identity=" << session.identity().hex() << '\n'
      << "catalog.session=" << session.catalog().identity().hex() << '\n'
      << "catalog.identity=" << session.catalog().catalog().identity().hex()
      << '\n';
  render_resolution_body(out, session);
  return out.str();
}

std::string render_report(const transaction_session& session)
{
  std::ostringstream out;
  const auto& program = session.program();
  out << "session.kind=transaction\n"
      << "session.identity=" << session.identity().hex() << '\n'
      << "resolution.session=" << session.resolution().identity().hex() << '\n'
      << "catalog.identity="
      << session.resolution().catalog().catalog().identity().hex() << '\n'
      << "state.snapshot="
      << session.resolution().installed().identity().string() << '\n'
      << "resolution.result="
      << session.resolution().resolution().identity().hex() << '\n'
      << "transaction.request=" << program.request().identity().hex() << '\n'
      << "transaction.program=" << program.identity().hex() << '\n'
      << "transaction.convergence="
      << pkgtransaction::to_string(program.request().policy().mode()) << '\n'
      << "transaction.nodes=" << program.nodes().size() << '\n'
      << "transaction.edges=" << program.edges().size() << '\n'
      << "transaction.runtime-cohorts=" << program.runtime_cohorts().size()
      << '\n';

  for (std::size_t index = 0; index < program.nodes().size(); ++index)
  {
    const auto& value = program.nodes()[index];
    out << "node." << index << ".identity=" << value.identity().hex() << '\n'
        << "node." << index << ".package=" << value.package().name() << '\n'
        << "node." << index << ".action="
        << pkgtransaction::to_string(value.action()) << '\n'
        << "node." << index << ".environment="
        << pkgresolve::to_string(value.environment()) << '\n'
        << "node." << index << ".reasons=" << value.reasons().size() << '\n';
    if (value.lifecycle())
      out << "node." << index << ".lifecycle="
          << pkgsource::to_string(*value.lifecycle()) << '\n';
  }

  for (std::size_t index = 0; index < program.edges().size(); ++index)
  {
    const auto& value = program.edges()[index];
    out << "transaction-edge." << index << ".identity="
        << value.identity().hex() << '\n'
        << "transaction-edge." << index << ".kind="
        << pkgtransaction::to_string(value.kind()) << '\n'
        << "transaction-edge." << index << ".before="
        << value.before().hex() << '\n'
        << "transaction-edge." << index << ".after="
        << value.after().hex() << '\n';
    if (value.scope())
      out << "transaction-edge." << index << ".scope="
          << scope_text(*value.scope()) << '\n';
    if (value.requirement_witness())
      out << "transaction-edge." << index << ".witness="
          << value.requirement_witness()->hex() << '\n';
    if (value.phase_order())
      out << "transaction-edge." << index << ".phase="
          << pkgtransaction::to_string(*value.phase_order()) << '\n';
  }

  for (std::size_t index = 0; index < program.runtime_cohorts().size(); ++index)
  {
    const auto& value = program.runtime_cohorts()[index];
    out << "runtime-cohort." << index << ".identity="
        << value.identity().hex() << '\n'
        << "runtime-cohort." << index << ".members="
        << value.members().size() << '\n'
        << "runtime-cohort." << index << ".witnesses="
        << value.witnesses().size() << '\n';
  }
  return out.str();
}

std::string render_report(const effect_attempt_inspection& inspection)
{
  std::ostringstream out;
  const auto& record = inspection.record();
  const auto& assessment = inspection.assessment();

  out << "session.kind=effect-attempt\n"
      << "effect.attempt=" << record.attempt().hex() << '\n'
      << "effect.record=" << record.identity().hex() << '\n'
      << "effect.session=" << record.session().hex() << '\n'
      << "effect.nonce=" << record.nonce().hex() << '\n'
      << "effect.sequence=" << record.sequence() << '\n';
  if (record.previous())
    out << "effect.previous=" << record.previous()->hex() << '\n';
  out << "effect.stage=" << effect_stage_text(record.stage()) << '\n'
      << "effect.disposition="
      << effect_restart_disposition_text(assessment.disposition()) << '\n'
      << "effect.terminal=" << boolean_text(inspection.terminal()) << '\n'
      << "effect.automatically-continuable="
      << boolean_text(inspection.automatically_continuable()) << '\n'
      << "effect.external-resolution-required="
      << boolean_text(inspection.external_resolution_required()) << '\n'
      << "effect.before-total=" << record.before_total() << '\n'
      << "effect.before-completed=" << record.before().size() << '\n'
      << "effect.after-total=" << record.after_total() << '\n'
      << "effect.after-completed=" << record.after().size() << '\n';
  if (record.active_index())
    out << "effect.active-index=" << *record.active_index() << '\n';

  for (std::size_t index = 0U; index < record.before().size(); ++index)
  {
    const auto prefix = "before." + std::to_string(index) + ".";
    out << prefix << "result=" << record.before()[index].result().hex() << '\n'
        << prefix << "succeeded="
        << boolean_text(record.before()[index].succeeded()) << '\n';
  }

  if (record.application())
  {
    const auto& application = *record.application();
    out << "effect.application-receipt=" << application.receipt() << '\n'
        << "effect.application-outcome="
        << application_outcome_text(application.outcome()) << '\n';
    if (application.journal())
      out << "effect.application-journal=" << *application.journal() << '\n';
    if (application.completed_evidence())
      out << "effect.application-completed-evidence="
          << *application.completed_evidence() << '\n';
  }

  for (std::size_t index = 0U; index < record.after().size(); ++index)
  {
    const auto prefix = "after." + std::to_string(index) + ".";
    out << prefix << "result=" << record.after()[index].result().hex() << '\n'
        << prefix << "succeeded="
        << boolean_text(record.after()[index].succeeded()) << '\n';
  }

  if (record.transaction_evidence())
    out << "effect.transaction-evidence=" << *record.transaction_evidence()
        << '\n';
  if (record.publication_request())
    out << "effect.publication-request=" << *record.publication_request()
        << '\n';
  if (record.publication())
  {
    const auto& publication = *record.publication();
    out << "effect.publication-receipt=" << publication.receipt() << '\n'
        << "effect.publication-outcome="
        << publication_outcome_text(publication.outcome()) << '\n';
    if (publication.resulting_snapshot())
      out << "effect.publication-resulting-snapshot="
          << *publication.resulting_snapshot() << '\n';
  }
  if (record.terminal_outcome())
    out << "effect.terminal-outcome="
        << effect_outcome_text(*record.terminal_outcome()) << '\n';
  if (record.reconciled_state())
    out << "effect.reconciled-state=" << *record.reconciled_state() << '\n';

  return out.str();
}

std::string render_report(const transaction_run_inspection& inspection)
{
  std::ostringstream out;
  const auto& record = inspection.record();
  const auto& assessment = inspection.assessment();

  out << "session.kind=transaction-run\n"
      << "run.journal=" << record.journal().hex() << '\n'
      << "run.record=" << record.identity().hex() << '\n'
      << "run.sequence=" << record.sequence() << '\n'
      << "run.transaction=" << record.transaction().hex() << '\n'
      << "run.nonce=" << record.nonce().hex() << '\n'
      << "run.identity=" << record.run().hex() << '\n'
      << "run.progress=" << record.progress().hex() << '\n'
      << "run.current-state=" << record.current_state().string() << '\n'
      << "run.policy=" << record.policy().identity().hex() << '\n'
      << "run.policy.construction-capacity="
      << record.policy().construction_capacity() << '\n'
      << "run.policy.check-capacity="
      << record.policy().check_capacity() << '\n'
      << "run.policy.operation-capacity="
      << record.policy().operation_capacity() << '\n'
      << "run.complete=" << boolean_text(record.complete()) << '\n'
      << "run.failed=" << boolean_text(record.failed()) << '\n'
      << "run.stopped=" << boolean_text(record.stopped()) << '\n'
      << "run.disposition="
      << inspection_disposition_text(inspection.disposition()) << '\n'
      << "run.dispatches=" << record.dispatches().size() << '\n'
      << "run.active=" << assessment.active().size() << '\n'
      << "run.external-evidence-required="
      << boolean_text(inspection.external_evidence_required()) << '\n';
  if (record.previous())
    out << "run.previous=" << record.previous()->hex() << '\n';

  for (std::size_t index = 0U; index < record.dispatches().size(); ++index)
  {
    const auto& retained = record.dispatches()[index];
    const auto& dispatch = retained.dispatch();
    const auto prefix = "dispatch." + std::to_string(index) + ".";
    out << prefix << "record=" << retained.identity().hex() << '\n'
        << prefix << "identity=" << dispatch.identity().hex() << '\n'
        << prefix << "kind=" << unit_kind_text(dispatch.unit().kind()) << '\n'
        << prefix << "state=" << dispatch_state_text(retained.state()) << '\n'
        << prefix << "unit=" << dispatch.unit().identity().hex() << '\n'
        << prefix << "primary-node="
        << dispatch.unit().primary_node().hex() << '\n'
        << prefix << "members=" << dispatch.unit().members().size() << '\n'
        << prefix << "nonce=" << dispatch.nonce().hex() << '\n'
        << prefix << "reserved-progress="
        << dispatch.reserved_from_progress().hex() << '\n'
        << prefix << "reserved-state="
        << dispatch.reserved_state().string() << '\n'
        << prefix << "dependencies=" << dispatch.dependencies().size() << '\n'
        << prefix << "observations=" << retained.observations().size() << '\n';
    if (retained.attempt_session())
      out << prefix << "attempt-session="
          << retained.attempt_session()->hex() << '\n';
    if (retained.effect_attempt())
      out << prefix << "effect-attempt="
          << retained.effect_attempt()->hex() << '\n';
    if (retained.terminal_evidence())
      out << prefix << "terminal-evidence="
          << retained.terminal_evidence()->hex() << '\n';
  }

  for (std::size_t index = 0U; index < assessment.active().size(); ++index)
  {
    const auto& active = assessment.active()[index];
    const auto prefix = "active." + std::to_string(index) + ".";
    out << prefix << "dispatch=" << active.dispatch().hex() << '\n'
        << prefix << "kind=" << unit_kind_text(active.kind()) << '\n'
        << prefix << "state=" << dispatch_state_text(active.state()) << '\n'
        << prefix << "disposition="
        << restart_disposition_text(active.disposition()) << '\n'
        << prefix << "external-evidence-required="
        << boolean_text(active.external_evidence_required()) << '\n'
        << prefix << "observations=" << active.observations().size() << '\n';
    if (active.attempt_session())
      out << prefix << "attempt-session="
          << active.attempt_session()->hex() << '\n';
    if (active.effect_attempt())
      out << prefix << "effect-attempt="
          << active.effect_attempt()->hex() << '\n';
  }

  return out.str();
}

} // namespace pkgctl
