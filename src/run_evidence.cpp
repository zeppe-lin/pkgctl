// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_evidence.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/evp.h>

namespace pkgctl {
namespace {

using context_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

[[noreturn]] void invalid_record(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::invalid_record, message);
}

std::string encoding_digest(const std::vector<std::uint8_t>& encoding)
{
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (!encoding.empty() &&
       EVP_DigestUpdate(context.get(), encoding.data(), encoding.size()) != 1))
  {
    invalid_record("cannot hash transaction-run evidence encoding");
  }

  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0U;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 ||
      size != 32U)
  {
    invalid_record("cannot finalize transaction-run evidence encoding digest");
  }

  static constexpr char digits[] = "0123456789abcdef";
  std::string result(64U, '0');
  for (std::size_t index = 0U; index < size; ++index)
  {
    result[index * 2U] = digits[(digest[index] >> 4U) & 0x0fU];
    result[index * 2U + 1U] = digits[digest[index] & 0x0fU];
  }
  return result;
}

const transaction_dispatch_record& require_dispatch_state(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch,
    transaction_unit_kind expected_kind,
    transaction_dispatch_state expected_state)
{
  const auto found = std::find_if(
      record.dispatches().begin(), record.dispatches().end(),
      [&](const transaction_dispatch_record& candidate) {
        return candidate.dispatch().identity() == dispatch.identity();
      });
  if (found == record.dispatches().end() ||
      found->dispatch().unit().identity() != dispatch.unit().identity())
  {
    invalid_record("durable run record does not retain the selected dispatch");
  }
  if (found->state() != expected_state)
    invalid_record("transaction-run evidence sees the wrong dispatch state");
  if (dispatch.unit().kind() != expected_kind)
    invalid_record("transaction-run evidence names the wrong dispatch kind");
  return *found;
}

const transaction_dispatch_record& require_started_dispatch(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch,
    transaction_unit_kind expected_kind)
{
  const auto& retained = require_dispatch_state(
      record, dispatch, expected_kind, transaction_dispatch_state::started);
  if (!retained.attempt_session())
    invalid_record("transaction-run evidence requires a started attempt");
  return retained;
}

session_identity construction_attempt_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& controller_request,
    const construction_session_encoding& session_encoding)
{
  return make_session_identity(
      "pkgctl/construction-dispatch-attempt/1",
      {
          journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
          attempt_session.hex(), controller_request.hex(),
          encoding_digest(session_encoding),
      });
}

session_identity check_attempt_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& controller_request,
    const session_identity& construction,
    const pkgcheck::check_request_identity& check_request,
    const check_session_encoding& session_encoding)
{
  return make_session_identity(
      "pkgctl/check-dispatch-attempt/1",
      {
          journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
          attempt_session.hex(), controller_request.hex(), construction.hex(),
          check_request.hex(), encoding_digest(session_encoding),
      });
}

session_identity construction_record_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& result,
    const session_identity& controller_request,
    const construction_session_encoding& session_encoding,
    const pkgfetch::materialization_identity& materialization,
    const pkgfetch::source_materialization_encoding& materialization_encoding,
    const pkgbuild::build_request_identity& build_request,
    const pkgexec::execution_request_identity& execution_request,
    const pkgexec::backend_capability_profile_identity& backend,
    const pkgexec::backend_capability_profile_encoding& backend_encoding,
    const pkgexec::execution_evidence_identity& execution,
    const pkgbuild::build_result_identity& build,
    const pkgbuild_exec::build_execution_result_encoding& encoding)
{
  return make_session_identity(
      "pkgctl/construction-dispatch-evidence/1",
      {
          journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
          attempt_session.hex(), result.hex(), controller_request.hex(),
          encoding_digest(session_encoding), materialization.hex(),
          encoding_digest(materialization_encoding),
          build_request.hex(), execution_request.hex(), backend.hex(),
          encoding_digest(backend_encoding), execution.hex(), build.hex(),
          encoding_digest(encoding),
      });
}

session_identity check_record_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& result,
    const session_identity& controller_request,
    const session_identity& construction,
    const check_session_encoding& session_encoding,
    const pkgcheck::check_request_identity& check_request,
    const pkgexec::execution_request_identity& execution_request,
    const pkgexec::backend_capability_profile_identity& backend,
    const pkgexec::backend_capability_profile_encoding& backend_encoding,
    const pkgexec::execution_evidence_identity& execution,
    const pkgcheck::check_result_identity& check,
    const pkgcheck_exec::check_execution_result_encoding& encoding)
{
  return make_session_identity(
      "pkgctl/check-dispatch-evidence/1",
      {
          journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
          attempt_session.hex(), result.hex(), controller_request.hex(),
          construction.hex(), encoding_digest(session_encoding),
          check_request.hex(), execution_request.hex(),
          backend.hex(), encoding_digest(backend_encoding), execution.hex(),
          check.hex(), encoding_digest(encoding),
      });
}

} // namespace

transaction_run_evidence_error::transaction_run_evidence_error(
    transaction_run_evidence_error_code code,
    std::string message,
    int system_error)
    : std::runtime_error(std::move(message)), code_(code),
      system_error_(system_error)
{
}

transaction_run_evidence_error_code
transaction_run_evidence_error::code() const noexcept
{
  return code_;
}

int transaction_run_evidence_error::system_error() const noexcept
{
  return system_error_;
}

construction_dispatch_attempt_record::construction_dispatch_attempt_record(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    session_identity dispatch,
    pkgtransaction::transaction_node_identity node,
    session_identity attempt_session,
    session_identity controller_request,
    construction_session_encoding session_encoding)
    : identity_(std::move(identity)), journal_(std::move(journal)),
      transaction_(std::move(transaction)), dispatch_(std::move(dispatch)),
      node_(std::move(node)), attempt_session_(std::move(attempt_session)),
      controller_request_(std::move(controller_request)),
      session_encoding_(std::move(session_encoding))
{
}

construction_dispatch_attempt_record
construction_dispatch_attempt_record::admit(
    const transaction_run_journal_record& reserved_record,
    const transaction_dispatch& dispatch,
    const construction_session& session)
{
  (void)require_dispatch_state(
      reserved_record, dispatch, transaction_unit_kind::construction,
      transaction_dispatch_state::reserved);
  const auto& request = session.request();
  if (reserved_record.transaction() != request.transaction().identity() ||
      dispatch.unit().primary_node() != request.build_node())
  {
    invalid_record("construction attempt belongs to another transaction node");
  }
  auto encoding = encode_construction_session(session);
  auto identity = construction_attempt_identity(
      reserved_record.journal(), reserved_record.transaction(),
      dispatch.identity(), dispatch.unit().primary_node(), session.identity(),
      request.identity(), encoding);
  return construction_dispatch_attempt_record(
      std::move(identity), reserved_record.journal(),
      reserved_record.transaction(), dispatch.identity(),
      dispatch.unit().primary_node(), session.identity(), request.identity(),
      std::move(encoding));
}

std::uint16_t construction_dispatch_attempt_record::schema_version() const noexcept
{ return schema_version_; }
const session_identity& construction_dispatch_attempt_record::identity() const noexcept
{ return identity_; }
const session_identity& construction_dispatch_attempt_record::journal() const noexcept
{ return journal_; }
const session_identity& construction_dispatch_attempt_record::transaction() const noexcept
{ return transaction_; }
const session_identity& construction_dispatch_attempt_record::dispatch() const noexcept
{ return dispatch_; }
const pkgtransaction::transaction_node_identity&
construction_dispatch_attempt_record::node() const noexcept
{ return node_; }
const session_identity&
construction_dispatch_attempt_record::attempt_session() const noexcept
{ return attempt_session_; }
const session_identity&
construction_dispatch_attempt_record::controller_request() const noexcept
{ return controller_request_; }
const construction_session_encoding&
construction_dispatch_attempt_record::session_encoding() const noexcept
{ return session_encoding_; }

check_dispatch_attempt_record::check_dispatch_attempt_record(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    session_identity dispatch,
    pkgtransaction::transaction_node_identity node,
    session_identity attempt_session,
    session_identity controller_request,
    session_identity construction,
    pkgcheck::check_request_identity check_request,
    check_session_encoding session_encoding)
    : identity_(std::move(identity)), journal_(std::move(journal)),
      transaction_(std::move(transaction)), dispatch_(std::move(dispatch)),
      node_(std::move(node)), attempt_session_(std::move(attempt_session)),
      controller_request_(std::move(controller_request)),
      construction_(std::move(construction)),
      check_request_(std::move(check_request)),
      session_encoding_(std::move(session_encoding))
{
}

check_dispatch_attempt_record check_dispatch_attempt_record::admit(
    const transaction_run_journal_record& reserved_record,
    const transaction_dispatch& dispatch,
    const transaction_check_session& session)
{
  (void)require_dispatch_state(
      reserved_record, dispatch, transaction_unit_kind::check,
      transaction_dispatch_state::reserved);
  const auto& request = session.request();
  if (reserved_record.transaction() != request.transaction().identity() ||
      dispatch.unit().primary_node() != request.check_node())
  {
    invalid_record("check attempt belongs to another transaction node");
  }
  auto encoding = encode_check_session(session);
  auto identity = check_attempt_identity(
      reserved_record.journal(), reserved_record.transaction(),
      dispatch.identity(), dispatch.unit().primary_node(), session.identity(),
      request.identity(), request.construction().identity(),
      request.check().identity(), encoding);
  return check_dispatch_attempt_record(
      std::move(identity), reserved_record.journal(),
      reserved_record.transaction(), dispatch.identity(),
      dispatch.unit().primary_node(), session.identity(), request.identity(),
      request.construction().identity(), request.check().identity(),
      std::move(encoding));
}

std::uint16_t check_dispatch_attempt_record::schema_version() const noexcept
{ return schema_version_; }
const session_identity& check_dispatch_attempt_record::identity() const noexcept
{ return identity_; }
const session_identity& check_dispatch_attempt_record::journal() const noexcept
{ return journal_; }
const session_identity& check_dispatch_attempt_record::transaction() const noexcept
{ return transaction_; }
const session_identity& check_dispatch_attempt_record::dispatch() const noexcept
{ return dispatch_; }
const pkgtransaction::transaction_node_identity&
check_dispatch_attempt_record::node() const noexcept
{ return node_; }
const session_identity& check_dispatch_attempt_record::attempt_session() const noexcept
{ return attempt_session_; }
const session_identity& check_dispatch_attempt_record::controller_request() const noexcept
{ return controller_request_; }
const session_identity& check_dispatch_attempt_record::construction() const noexcept
{ return construction_; }
const pkgcheck::check_request_identity&
check_dispatch_attempt_record::check_request() const noexcept
{ return check_request_; }
const check_session_encoding& check_dispatch_attempt_record::session_encoding() const noexcept
{ return session_encoding_; }

construction_dispatch_evidence_record::construction_dispatch_evidence_record(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    session_identity dispatch,
    pkgtransaction::transaction_node_identity node,
    session_identity attempt_session,
    session_identity result,
    session_identity controller_request,
    construction_session_encoding session_encoding,
    pkgfetch::materialization_identity materialization,
    pkgfetch::source_materialization_encoding materialization_encoding,
    pkgbuild::build_request_identity build_request,
    pkgexec::execution_request_identity execution_request,
    pkgexec::backend_capability_profile_identity backend,
    pkgexec::backend_capability_profile_encoding backend_encoding,
    pkgexec::execution_evidence_identity execution,
    pkgbuild::build_result_identity build,
    pkgbuild_exec::build_execution_result_encoding encoding)
    : identity_(std::move(identity)), journal_(std::move(journal)),
      transaction_(std::move(transaction)), dispatch_(std::move(dispatch)),
      node_(std::move(node)), attempt_session_(std::move(attempt_session)),
      result_(std::move(result)),
      controller_request_(std::move(controller_request)),
      session_encoding_(std::move(session_encoding)),
      materialization_(std::move(materialization)),
      materialization_encoding_(std::move(materialization_encoding)),
      build_request_(std::move(build_request)),
      execution_request_(std::move(execution_request)),
      backend_(std::move(backend)),
      backend_encoding_(std::move(backend_encoding)),
      execution_(std::move(execution)),
      build_(std::move(build)), encoding_(std::move(encoding))
{
}

construction_dispatch_evidence_record
construction_dispatch_evidence_record::admit(
    const transaction_run_journal_record& started_record,
    const transaction_dispatch& dispatch,
    const construction_result& result)
{
  const auto& retained = require_started_dispatch(
      started_record, dispatch, transaction_unit_kind::construction);
  const auto& session = result.session();
  const auto& request = session.request();
  const auto& build = result.build();

  if (*retained.attempt_session() != session.identity())
    invalid_record("construction evidence belongs to another started attempt");
  if (started_record.transaction() != request.transaction().identity() ||
      dispatch.unit().primary_node() != request.build_node())
  {
    invalid_record("construction evidence belongs to another transaction node");
  }
  if (result.materialization().source().identity() != request.source().identity() ||
      build.build().request().identity() != request.build().identity())
  {
    invalid_record("construction evidence contradicts its admitted request");
  }

  const auto expected_result = make_session_identity(
      "pkgctl/construction-result/1",
      {
          session.identity().hex(), result.materialization().identity().hex(),
          build.execution().identity().hex(), build.build().identity().hex(),
          std::to_string(static_cast<unsigned int>(result.outcome())),
      });
  if (result.identity() != expected_result)
    invalid_record("construction result identity is not canonical");

  auto session_encoding = encode_construction_session(session);
  auto materialization_encoding =
      pkgfetch::encode_source_materialization(result.materialization());
  auto backend_encoding = pkgexec::encode_backend_capability_profile(
      build.execution().backend());
  auto encoding = pkgbuild_exec::encode_build_execution_result(build);
  auto identity = construction_record_identity(
      started_record.journal(), started_record.transaction(),
      dispatch.identity(), dispatch.unit().primary_node(), session.identity(),
      result.identity(), request.identity(), session_encoding,
      result.materialization().identity(), materialization_encoding,
      request.build().identity(),
      build.execution().request().identity(),
      build.execution().backend().identity(), backend_encoding,
      build.execution().identity(),
      build.build().identity(), encoding);
  return construction_dispatch_evidence_record(
      std::move(identity), started_record.journal(),
      started_record.transaction(), dispatch.identity(),
      dispatch.unit().primary_node(), session.identity(), result.identity(),
      request.identity(), std::move(session_encoding),
      result.materialization().identity(), std::move(materialization_encoding),
      request.build().identity(),
      build.execution().request().identity(),
      build.execution().backend().identity(), std::move(backend_encoding),
      build.execution().identity(),
      build.build().identity(), std::move(encoding));
}

std::uint16_t construction_dispatch_evidence_record::schema_version() const noexcept
{ return schema_version_; }
const session_identity& construction_dispatch_evidence_record::identity() const noexcept
{ return identity_; }
const session_identity& construction_dispatch_evidence_record::journal() const noexcept
{ return journal_; }
const session_identity& construction_dispatch_evidence_record::transaction() const noexcept
{ return transaction_; }
const session_identity& construction_dispatch_evidence_record::dispatch() const noexcept
{ return dispatch_; }
const pkgtransaction::transaction_node_identity&
construction_dispatch_evidence_record::node() const noexcept
{ return node_; }
const session_identity&
construction_dispatch_evidence_record::attempt_session() const noexcept
{ return attempt_session_; }
const session_identity& construction_dispatch_evidence_record::result() const noexcept
{ return result_; }
const session_identity&
construction_dispatch_evidence_record::controller_request() const noexcept
{ return controller_request_; }
const construction_session_encoding&
construction_dispatch_evidence_record::session_encoding() const noexcept
{ return session_encoding_; }
const pkgfetch::materialization_identity&
construction_dispatch_evidence_record::materialization() const noexcept
{ return materialization_; }
const pkgfetch::source_materialization_encoding&
construction_dispatch_evidence_record::materialization_encoding() const noexcept
{ return materialization_encoding_; }
const pkgbuild::build_request_identity&
construction_dispatch_evidence_record::build_request() const noexcept
{ return build_request_; }
const pkgexec::execution_request_identity&
construction_dispatch_evidence_record::execution_request() const noexcept
{ return execution_request_; }
const pkgexec::backend_capability_profile_identity&
construction_dispatch_evidence_record::backend() const noexcept
{ return backend_; }
const pkgexec::backend_capability_profile_encoding&
construction_dispatch_evidence_record::backend_encoding() const noexcept
{ return backend_encoding_; }
const pkgexec::execution_evidence_identity&
construction_dispatch_evidence_record::execution() const noexcept
{ return execution_; }
const pkgbuild::build_result_identity&
construction_dispatch_evidence_record::build() const noexcept
{ return build_; }
const pkgbuild_exec::build_execution_result_encoding&
construction_dispatch_evidence_record::encoding() const noexcept
{ return encoding_; }

check_dispatch_evidence_record::check_dispatch_evidence_record(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    session_identity dispatch,
    pkgtransaction::transaction_node_identity node,
    session_identity attempt_session,
    session_identity result,
    session_identity controller_request,
    session_identity construction,
    check_session_encoding session_encoding,
    pkgcheck::check_request_identity check_request,
    pkgexec::execution_request_identity execution_request,
    pkgexec::backend_capability_profile_identity backend,
    pkgexec::backend_capability_profile_encoding backend_encoding,
    pkgexec::execution_evidence_identity execution,
    pkgcheck::check_result_identity check,
    pkgcheck_exec::check_execution_result_encoding encoding)
    : identity_(std::move(identity)), journal_(std::move(journal)),
      transaction_(std::move(transaction)), dispatch_(std::move(dispatch)),
      node_(std::move(node)), attempt_session_(std::move(attempt_session)),
      result_(std::move(result)),
      controller_request_(std::move(controller_request)),
      construction_(std::move(construction)),
      session_encoding_(std::move(session_encoding)),
      check_request_(std::move(check_request)),
      execution_request_(std::move(execution_request)),
      backend_(std::move(backend)),
      backend_encoding_(std::move(backend_encoding)),
      execution_(std::move(execution)),
      check_(std::move(check)), encoding_(std::move(encoding))
{
}

check_dispatch_evidence_record check_dispatch_evidence_record::admit(
    const transaction_run_journal_record& started_record,
    const transaction_dispatch& dispatch,
    const transaction_check_result& result)
{
  const auto& retained = require_started_dispatch(
      started_record, dispatch, transaction_unit_kind::check);
  const auto& session = result.session();
  const auto& request = session.request();
  const auto& execution = result.execution();

  if (*retained.attempt_session() != session.identity())
    invalid_record("check evidence belongs to another started attempt");
  if (started_record.transaction() != request.transaction().identity() ||
      dispatch.unit().primary_node() != request.check_node())
  {
    invalid_record("check evidence belongs to another transaction node");
  }
  if (execution.check().request().identity() != request.check().identity() ||
      execution.execution().request().identity() != session.execution_request())
  {
    invalid_record("check evidence contradicts its admitted request");
  }

  const auto expected_result = make_session_identity(
      "pkgctl/transaction-check-result/1",
      {
          session.identity().hex(), execution.execution().identity().hex(),
          execution.check().identity().hex(),
          std::to_string(static_cast<unsigned int>(execution.check().outcome())),
      });
  if (result.identity() != expected_result)
    invalid_record("check result identity is not canonical");

  auto session_encoding = encode_check_session(session);
  auto backend_encoding = pkgexec::encode_backend_capability_profile(
      execution.execution().backend());
  auto encoding = pkgcheck_exec::encode_check_execution_result(execution);
  auto identity = check_record_identity(
      started_record.journal(), started_record.transaction(),
      dispatch.identity(), dispatch.unit().primary_node(), session.identity(),
      result.identity(), request.identity(), request.construction().identity(),
      session_encoding, request.check().identity(),
      execution.execution().request().identity(),
      execution.execution().backend().identity(), backend_encoding,
      execution.execution().identity(), execution.check().identity(), encoding);
  return check_dispatch_evidence_record(
      std::move(identity), started_record.journal(),
      started_record.transaction(), dispatch.identity(),
      dispatch.unit().primary_node(), session.identity(), result.identity(),
      request.identity(), request.construction().identity(),
      std::move(session_encoding), request.check().identity(),
      execution.execution().request().identity(),
      execution.execution().backend().identity(), std::move(backend_encoding),
      execution.execution().identity(), execution.check().identity(),
      std::move(encoding));
}

std::uint16_t check_dispatch_evidence_record::schema_version() const noexcept
{ return schema_version_; }
const session_identity& check_dispatch_evidence_record::identity() const noexcept
{ return identity_; }
const session_identity& check_dispatch_evidence_record::journal() const noexcept
{ return journal_; }
const session_identity& check_dispatch_evidence_record::transaction() const noexcept
{ return transaction_; }
const session_identity& check_dispatch_evidence_record::dispatch() const noexcept
{ return dispatch_; }
const pkgtransaction::transaction_node_identity&
check_dispatch_evidence_record::node() const noexcept
{ return node_; }
const session_identity& check_dispatch_evidence_record::attempt_session() const noexcept
{ return attempt_session_; }
const session_identity& check_dispatch_evidence_record::result() const noexcept
{ return result_; }
const session_identity& check_dispatch_evidence_record::controller_request() const noexcept
{ return controller_request_; }
const session_identity& check_dispatch_evidence_record::construction() const noexcept
{ return construction_; }
const check_session_encoding&
check_dispatch_evidence_record::session_encoding() const noexcept
{ return session_encoding_; }
const pkgcheck::check_request_identity&
check_dispatch_evidence_record::check_request() const noexcept
{ return check_request_; }
const pkgexec::execution_request_identity&
check_dispatch_evidence_record::execution_request() const noexcept
{ return execution_request_; }
const pkgexec::backend_capability_profile_identity&
check_dispatch_evidence_record::backend() const noexcept
{ return backend_; }
const pkgexec::backend_capability_profile_encoding&
check_dispatch_evidence_record::backend_encoding() const noexcept
{ return backend_encoding_; }
const pkgexec::execution_evidence_identity&
check_dispatch_evidence_record::execution() const noexcept
{ return execution_; }
const pkgcheck::check_result_identity& check_dispatch_evidence_record::check() const noexcept
{ return check_; }
const pkgcheck_exec::check_execution_result_encoding&
check_dispatch_evidence_record::encoding() const noexcept
{ return encoding_; }

} // namespace pkgctl
