// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_recovery.h>

#include "run_recovery_detail.h"

#include <pkgctl/error.h>
#include <pkgctl/identity.h>

#include <exception>
#include <string>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void evidence_missing(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::evidence_missing,
      message);
}

[[noreturn]] void context_mismatch(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::recovery_context_mismatch,
      message);
}

[[noreturn]] void decode_failed(
    const std::string& role,
    const std::exception& problem)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::recovery_decode_failed,
      role + " recovery decode failed: " + problem.what());
}

const session_identity& require_attempt(
    const transaction_dispatch_restart_assessment& assessment)
{
  if (!assessment.attempt_session())
    context_mismatch("restart assessment lacks an attempt session");
  return *assessment.attempt_session();
}

template<typename Evidence>
void validate_evidence_binding(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch,
    const Evidence& evidence)
{
  if (evidence.journal() != checkpoint.record().journal() ||
      evidence.transaction() != checkpoint.record().transaction() ||
      evidence.dispatch() != dispatch.identity() ||
      evidence.node() != dispatch.unit().primary_node() ||
      evidence.attempt_session() != require_attempt(assessment))
  {
    context_mismatch(
        "stored dispatch evidence belongs to another durable attempt");
  }
}

void validate_construction_context(
    const construction_dispatch_evidence_record& evidence,
    const construction_dispatch_recovery_context& context)
{
  const auto& session = context.session;
  const auto& request = session.request();
  if (session.identity() != evidence.attempt_session() ||
      request.identity() != evidence.controller_request() ||
      request.transaction().identity() != evidence.transaction() ||
      request.build_node() != evidence.node() ||
      request.build().identity() != evidence.build_request() ||
      context.materialization.identity() != evidence.materialization() ||
      context.materialization.source().identity() != request.source().identity() ||
      context.execution_request.identity() != evidence.execution_request() ||
      context.backend.identity() != evidence.backend())
  {
    context_mismatch(
        "construction recovery context differs from stored evidence");
  }
}

void validate_check_context(
    const check_dispatch_evidence_record& evidence,
    const check_dispatch_recovery_context& context)
{
  const auto& session = context.session;
  const auto& request = session.request();
  if (session.identity() != evidence.attempt_session() ||
      request.identity() != evidence.controller_request() ||
      request.transaction().identity() != evidence.transaction() ||
      request.check_node() != evidence.node() ||
      request.construction().identity() != evidence.construction() ||
      request.check().identity() != evidence.check_request() ||
      session.execution_request() != evidence.execution_request() ||
      context.execution_request.identity() != evidence.execution_request() ||
      context.backend.identity() != evidence.backend())
  {
    context_mismatch("check recovery context differs from stored evidence");
  }
}

void validate_decoded_construction(
    const construction_dispatch_evidence_record& evidence,
    const pkgbuild_exec::build_execution_result& result)
{
  if (result.execution().identity() != evidence.execution() ||
      result.execution().request().identity() != evidence.execution_request() ||
      result.execution().backend().identity() != evidence.backend() ||
      result.build().identity() != evidence.build() ||
      result.build().request().identity() != evidence.build_request())
  {
    context_mismatch(
        "decoded construction evidence differs from its durable record");
  }
}

void validate_decoded_check(
    const check_dispatch_evidence_record& evidence,
    const pkgcheck_exec::check_execution_result& result)
{
  if (result.execution().identity() != evidence.execution() ||
      result.execution().request().identity() != evidence.execution_request() ||
      result.execution().backend().identity() != evidence.backend() ||
      result.check().identity() != evidence.check() ||
      result.check().request().identity() != evidence.check_request())
  {
    context_mismatch(
        "decoded check evidence differs from its durable record");
  }
}

} // namespace

struct detail_run_recovery_access final {
  static construction_result construction(
      construction_dispatch_recovery_context context,
      pkgbuild_exec::build_execution_result build,
      const session_identity& expected_identity)
  {
    const auto& request = context.session.request();
    if (context.materialization.source().identity() !=
            request.source().identity() ||
        build.build().request().identity() != request.build().identity())
    {
      context_mismatch(
          "recovered construction result contradicts its controller session");
    }

    const bool succeeded =
        build.build().outcome() == pkgbuild::build_outcome::succeeded;
    if (succeeded &&
        (build.sealing_failure().has_value() ||
         !build.image_authority().has_value()))
    {
      context_mismatch(
          "recovered successful construction lacks artifact authority");
    }
    if (!succeeded && build.image_authority().has_value())
      context_mismatch(
          "recovered failed construction carries artifact authority");

    const auto outcome = succeeded
        ? construction_outcome::completed
        : construction_outcome::build_failed;
    auto identity = make_session_identity(
        "pkgctl/construction-result/1",
        {
            context.session.identity().hex(),
            context.materialization.identity().hex(),
            build.execution().identity().hex(),
            build.build().identity().hex(),
            std::to_string(static_cast<unsigned int>(outcome)),
        });
    if (identity != expected_identity)
      context_mismatch(
          "recovered construction result identity differs from durable evidence");

    return construction_result(
        std::move(context.session), std::move(context.materialization),
        std::move(build), outcome, std::move(identity));
  }

  static transaction_check_result check(
      check_dispatch_recovery_context context,
      pkgcheck_exec::check_execution_result execution,
      const session_identity& expected_identity)
  {
    if (execution.check().request().identity() !=
            context.session.request().check().identity() ||
        execution.execution().request().identity() !=
            context.session.execution_request() ||
        execution.execution().request().purpose().kind() !=
            pkgexec::execution_purpose_kind::check)
    {
      context_mismatch(
          "recovered check result contradicts its controller session");
    }

    auto identity = make_session_identity(
        "pkgctl/transaction-check-result/1",
        {
            context.session.identity().hex(),
            execution.execution().identity().hex(),
            execution.check().identity().hex(),
            std::to_string(static_cast<unsigned int>(
                execution.check().outcome())),
        });
    if (identity != expected_identity)
      context_mismatch(
          "recovered check result identity differs from durable evidence");

    return transaction_check_result(
        std::move(context.session), std::move(execution),
        std::move(identity));
  }
};

construction_result
detail::rehydrate_construction_dispatch_evidence(
    const construction_dispatch_evidence_record& evidence,
    construction_dispatch_recovery_context context)
{
  validate_construction_context(evidence, context);

  try
  {
    auto decoded = pkgbuild_exec::decode_build_execution_result(
        evidence.encoding(), context.session.request().build(),
        context.execution_request, context.backend);
    validate_decoded_construction(evidence, decoded);
    return detail_run_recovery_access::construction(
        std::move(context), std::move(decoded), evidence.result());
  }
  catch (const transaction_run_evidence_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    decode_failed("construction", problem);
  }
}

transaction_check_result
detail::rehydrate_check_dispatch_evidence(
    const check_dispatch_evidence_record& evidence,
    check_dispatch_recovery_context context)
{
  validate_check_context(evidence, context);

  try
  {
    auto decoded = pkgcheck_exec::decode_check_execution_result(
        evidence.encoding(), context.session.request().check(),
        context.execution_request, context.backend);
    validate_decoded_check(evidence, decoded);
    return detail_run_recovery_access::check(
        std::move(context), std::move(decoded), evidence.result());
  }
  catch (const transaction_run_evidence_error&)
  {
    throw;
  }
  catch (const std::exception& problem)
  {
    decode_failed("check", problem);
  }
}

native_transaction_dispatch_recovery_context_source::
native_transaction_dispatch_recovery_context_source(
    transaction_dispatch_session_source& sessions,
    pkgexec::execution_backend& construction_backend,
    pkgexec::execution_backend& check_backend,
    transaction_operation_recovery_authority_source& operations)
    : sessions_(sessions), construction_backend_(construction_backend),
      check_backend_(check_backend), operations_(operations)
{
}

construction_dispatch_recovery_context
native_transaction_dispatch_recovery_context_source::construction(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch,
    const construction_dispatch_evidence_record& evidence)
{
  const auto& attempt = require_attempt(assessment);
  auto session = sessions_.construction(
      checkpoint.record(), checkpoint.run(), dispatch);
  auto backend = construction_backend_.capabilities();
  const auto& request = session.request();
  if (session.identity() != evidence.attempt_session() ||
      session.identity() != attempt ||
      request.identity() != evidence.controller_request() ||
      request.transaction().identity() != evidence.transaction() ||
      request.build_node() != evidence.node() ||
      request.build().identity() != evidence.build_request() ||
      backend.identity() != evidence.backend())
  {
    context_mismatch(
        "native construction context differs from retained evidence");
  }

  auto materialization = pkgfetch::materialize(
      pkgfetch::materialization_request::seal(
          session.request().source(), session.paths().local_source_root,
          session.paths().content_store_root,
          session.request().acquisition_policy()));
  if (materialization.identity() != evidence.materialization())
    context_mismatch(
        "native construction materialization differs from retained evidence");

  auto admitted = pkgbuild_exec::admitted_build_session::admit(
      request.build(), materialization, session.package_inputs(),
      session.paths().build, session.execution_identity(),
      session.compression());
  auto execution_request =
      pkgbuild_exec::seal_execution_request(admitted);
  if (execution_request.identity() != evidence.execution_request())
    context_mismatch(
        "native construction request differs from retained evidence");
  return {
      std::move(session), std::move(materialization),
      std::move(execution_request), std::move(backend),
  };
}

check_dispatch_recovery_context
native_transaction_dispatch_recovery_context_source::check(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch,
    const check_dispatch_evidence_record& evidence)
{
  const auto& attempt = require_attempt(assessment);
  auto session = sessions_.check(
      checkpoint.record(), checkpoint.run(), dispatch);
  auto backend = check_backend_.capabilities();
  const auto& request = session.request();
  if (session.identity() != evidence.attempt_session() ||
      session.identity() != attempt ||
      request.identity() != evidence.controller_request() ||
      request.transaction().identity() != evidence.transaction() ||
      request.check_node() != evidence.node() ||
      request.construction().identity() != evidence.construction() ||
      request.check().identity() != evidence.check_request() ||
      backend.identity() != evidence.backend())
  {
    context_mismatch("native check context differs from retained evidence");
  }

  auto execution_request = pkgcheck_exec::seal_execution_request(
      session.execution_session());
  if (execution_request.identity() != evidence.execution_request())
    context_mismatch("native check request differs from retained evidence");
  return {
      std::move(session), std::move(execution_request), std::move(backend),
  };
}

effect_restart_checkpoint
native_transaction_dispatch_recovery_context_source::operation(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch)
{
  return operations_.operation(checkpoint, assessment, dispatch);
}

stored_transaction_dispatch_recovery_authority_source::
stored_transaction_dispatch_recovery_authority_source(
    transaction_run_evidence_store& evidence,
    transaction_dispatch_recovery_context_source& context)
    : evidence_(evidence), context_(context)
{
}

construction_result
stored_transaction_dispatch_recovery_authority_source::construction(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch)
{
  const auto& attempt = require_attempt(assessment);
  auto evidence = evidence_.load_construction(
      checkpoint.record().journal(), dispatch.identity(), attempt);
  if (!evidence)
    evidence_missing(
        "started construction dispatch lacks durable execution evidence");
  validate_evidence_binding(checkpoint, assessment, dispatch, *evidence);

  auto context = context_.construction(
      checkpoint, assessment, dispatch, *evidence);
  return detail::rehydrate_construction_dispatch_evidence(
      *evidence, std::move(context));
}

transaction_check_result
stored_transaction_dispatch_recovery_authority_source::check(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch)
{
  const auto& attempt = require_attempt(assessment);
  auto evidence = evidence_.load_check(
      checkpoint.record().journal(), dispatch.identity(), attempt);
  if (!evidence)
    evidence_missing("started check dispatch lacks durable execution evidence");
  validate_evidence_binding(checkpoint, assessment, dispatch, *evidence);

  auto context = context_.check(
      checkpoint, assessment, dispatch, *evidence);
  return detail::rehydrate_check_dispatch_evidence(
      *evidence, std::move(context));
}

effect_restart_checkpoint
stored_transaction_dispatch_recovery_authority_source::operation(
    const transaction_run_restart_checkpoint& checkpoint,
    const transaction_dispatch_restart_assessment& assessment,
    const transaction_dispatch& dispatch)
{
  return context_.operation(checkpoint, assessment, dispatch);
}

} // namespace pkgctl
