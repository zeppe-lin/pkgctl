// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/check.h>

#include <pkgctl/error.h>
#include <pkgctl/progression.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pkgctl {
namespace {

using node_identity = pkgtransaction::transaction_node_identity;

const pkgtransaction::transaction_node& require_ready_check_node(
    const transaction_progress& progression,
    const node_identity& identity)
{
  const auto* node = progression.transaction().program().find(identity);
  if (node == nullptr)
    throw error(error_code::invalid_check_request,
                "check node is absent from the transaction program");
  if (node->action() != pkgtransaction::transaction_action_kind::check)
    throw error(error_code::invalid_check_request,
                "requested transaction node is not a check action");
  if (progression.status(identity) != transaction_node_status::ready)
    throw error(error_code::invalid_check_request,
                "transaction check node is not ready");
  return *node;
}

const pkgtransaction::transaction_node& require_build_predecessor(
    const pkgtransaction::transaction_program& program,
    const node_identity& check_node)
{
  const pkgtransaction::transaction_edge* predecessor = nullptr;
  for (const auto& edge : program.edges()) {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::phase ||
        edge.after() != check_node || !edge.phase_order() ||
        *edge.phase_order() !=
            pkgtransaction::phase_order_kind::build_before_check)
      continue;

    if (predecessor != nullptr)
      throw error(error_code::invalid_check_request,
                  "check node has more than one build predecessor");
    predecessor = &edge;
  }

  if (predecessor == nullptr)
    throw error(error_code::invalid_check_request,
                "check node has no build predecessor");

  const auto* build = program.find(predecessor->before());
  if (build == nullptr ||
      build->action() != pkgtransaction::transaction_action_kind::build)
    throw error(error_code::invalid_check_request,
                "check predecessor is not a build node");
  return *build;
}

const construction_result& require_construction(
    const transaction_progress& progression,
    const pkgtransaction::transaction_node& build_node)
{
  const auto* construction = progression.construction(build_node.identity());
  if (construction == nullptr)
    throw error(error_code::invalid_check_request,
                "ready check node lacks retained construction evidence");
  if (!construction->succeeded())
    throw error(error_code::invalid_check_request,
                "check execution requires successful construction evidence");
  if (construction->session().request().transaction().identity() !=
      progression.transaction().identity())
    throw error(error_code::invalid_check_request,
                "construction evidence belongs to another transaction");
  return *construction;
}

std::vector<std::string> request_identity_fields(
    const transaction_progress& progression,
    const node_identity& check_node,
    const construction_result& construction,
    const pkgcheck::check_request& check)
{
  return {
      progression.transaction().identity().hex(),
      progression.identity().hex(),
      check_node.hex(),
      construction.identity().hex(),
      check.identity().hex(),
  };
}

std::string path_text(const std::filesystem::path& path)
{
  return path.generic_string();
}

std::vector<std::string> session_identity_fields(
    const transaction_check_request& request,
    const pkgcheck_exec::admitted_check_session& execution_session,
    const pkgexec::execution_request& execution_request)
{
  const auto& source = execution_session.source();
  const auto& package = execution_session.package();
  const auto& paths = execution_session.paths();
  const auto& execution_identity = execution_session.identity();

  std::vector<std::string> fields{
      request.identity().hex(),
      execution_request.identity().hex(),
      source.source.hex(),
      source.tree.hex(),
      path_text(source.path),
      package.artifact.hex(),
      package.tree.hex(),
      path_text(package.path),
      std::to_string(execution_session.inputs().size()),
  };

  for (const auto& input : execution_session.inputs()) {
    fields.push_back(input.input.hex());
    fields.push_back(input.resource.hex());
    fields.push_back(path_text(input.path));
  }

  fields.push_back(paths.root_view.hex());
  fields.push_back(path_text(paths.root_view_path));
  fields.push_back(path_text(paths.temporary_root));
  fields.push_back(execution_identity.interpreter.hex());
  fields.push_back(std::to_string(execution_identity.user_id));
  fields.push_back(std::to_string(execution_identity.group_id));
  fields.push_back(
      std::to_string(execution_identity.supplementary_groups.size()));
  for (const auto group : execution_identity.supplementary_groups)
    fields.push_back(std::to_string(group));
  fields.push_back(execution_session.limits().identity().hex());
  return fields;
}

void validate_driver_result(
    const transaction_check_session& session,
    const pkgcheck_exec::check_execution_result& result)
{
  if (result.check().request().identity() !=
      session.request().check().identity())
    throw error(error_code::check_driver_contract_violation,
                "check driver returned evidence for another check request");

  if (result.execution().request().identity() != session.execution_request())
    throw error(error_code::check_driver_contract_violation,
                "check driver returned evidence for another execution request");

  if (result.execution().request().purpose().kind() !=
      pkgexec::execution_purpose_kind::check)
    throw error(error_code::check_driver_contract_violation,
                "check driver returned non-check execution evidence");
}

void prepare_native_check_temporary(
    const pkgcheck_exec::admitted_check_session& session)
{
  namespace fs = std::filesystem;

  const auto& temporary = session.paths().temporary_root;
  std::error_code ec;
  fs::remove_all(temporary, ec);
  if (ec)
    throw error(
        error_code::check_driver_contract_violation,
        "cannot clear native check temporary root " + temporary.string() +
            ": " + ec.message());

  fs::create_directories(temporary, ec);
  if (ec)
    throw error(
        error_code::check_driver_contract_violation,
        "cannot create native check temporary root " + temporary.string() +
            ": " + ec.message());

  const auto prepare_directory = [&](const fs::path& path, mode_t mode) {
    const int descriptor = ::open(
        path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0)
      throw error(
          error_code::check_driver_contract_violation,
          "cannot open native check temporary directory " + path.string() +
              ": " + std::strerror(errno));

    const auto close_descriptor = [&]() noexcept { (void)::close(descriptor); };
    if (::fchown(
            descriptor,
            static_cast<uid_t>(session.identity().user_id),
            static_cast<gid_t>(session.identity().group_id)) != 0 ||
        ::fchmod(descriptor, mode) != 0 || ::fsync(descriptor) != 0)
    {
      const int saved = errno;
      close_descriptor();
      throw error(
          error_code::check_driver_contract_violation,
          "cannot prepare native check temporary directory " + path.string() +
              ": " + std::strerror(saved));
    }
    close_descriptor();
  };

  prepare_directory(temporary, 0700);
  const auto home = temporary / "home";
  if (!fs::create_directory(home, ec) || ec)
  {
    const auto detail = ec ? ec.message() : std::string("path already exists");
    throw error(
        error_code::check_driver_contract_violation,
        "cannot create native check home directory " + home.string() +
            ": " + detail);
  }
  prepare_directory(home, 0700);
}

pkgcheck_exec::check_execution_result invoke_check_driver(
    transaction_check_driver& driver,
    const pkgcheck_exec::admitted_check_session& session)
{
  try {
    return driver.execute_check(session);
  } catch (const error& problem) {
    if (problem.code() == error_code::check_driver_contract_violation)
      throw;
    throw error(
        error_code::check_driver_contract_violation,
        "check driver threw controller refusal instead of evidence: " +
            std::string(problem.what()));
  } catch (const std::exception& problem) {
    throw error(
        error_code::check_driver_contract_violation,
        "check driver threw instead of returning evidence: " +
            std::string(problem.what()));
  } catch (...) {
    throw error(
        error_code::check_driver_contract_violation,
        "check driver threw non-standard execution evidence");
  }
}

} // namespace

transaction_check_request::transaction_check_request(
    transaction_session transaction,
    session_identity prepared_from_progress,
    pkgtransaction::transaction_node_identity check_node,
    construction_result construction,
    pkgcheck::check_request check,
    session_identity identity)
    : transaction_(std::move(transaction)),
      prepared_from_progress_(std::move(prepared_from_progress)),
      check_node_(std::move(check_node)),
      construction_(std::move(construction)),
      check_(std::move(check)),
      identity_(std::move(identity))
{
}

transaction_check_request transaction_check_request::make(
    const transaction_progress& progression,
    pkgtransaction::transaction_node_identity check_node)
{
  (void)require_ready_check_node(progression, check_node);
  const auto& build_node = require_build_predecessor(
      progression.transaction().program(), check_node);
  const auto& retained_construction = require_construction(
      progression, build_node);
  construction_result construction = retained_construction;

  auto check = pkgcheck::check_request::seal(
      progression.transaction().program(), check_node,
      construction.build().build());
  auto identity = make_session_identity(
      "pkgctl/transaction-check-request/1",
      request_identity_fields(
          progression, check_node, construction, check));

  return transaction_check_request(
      progression.transaction(), progression.identity(),
      std::move(check_node), std::move(construction),
      std::move(check), std::move(identity));
}

const transaction_session&
transaction_check_request::transaction() const noexcept
{
  return transaction_;
}

const session_identity&
transaction_check_request::prepared_from_progress() const noexcept
{
  return prepared_from_progress_;
}

const pkgtransaction::transaction_node_identity&
transaction_check_request::check_node() const noexcept
{
  return check_node_;
}

const construction_result&
transaction_check_request::construction() const noexcept
{
  return construction_;
}

const pkgcheck::check_request&
transaction_check_request::check() const noexcept
{
  return check_;
}

const session_identity& transaction_check_request::identity() const noexcept
{
  return identity_;
}

transaction_check_session::transaction_check_session(
    transaction_check_request request,
    pkgcheck_exec::admitted_check_session execution_session,
    pkgexec::execution_request_identity execution_request,
    session_identity identity)
    : request_(std::move(request)),
      execution_session_(std::move(execution_session)),
      execution_request_(std::move(execution_request)),
      identity_(std::move(identity))
{
}

transaction_check_session transaction_check_session::admit(
    transaction_check_request request,
    transaction_check_resources resources)
{
  try {
    auto execution_session = pkgcheck_exec::admitted_check_session::admit(
        request.check(), std::move(resources.source),
        std::move(resources.package), std::move(resources.inputs),
        std::move(resources.paths), std::move(resources.execution_identity),
        std::move(resources.limits));
    auto execution_request =
        pkgcheck_exec::seal_execution_request(execution_session);

    auto identity = make_session_identity(
        "pkgctl/transaction-check-session/1",
        session_identity_fields(request, execution_session, execution_request));
    return transaction_check_session(
        std::move(request), std::move(execution_session),
        execution_request.identity(), std::move(identity));
  } catch (const pkgcheck_exec::error& problem) {
    throw error(
        error_code::invalid_check_session,
        "check execution session admission failed: " +
            std::string(problem.what()));
  } catch (const pkgexec::error& problem) {
    throw error(
        error_code::invalid_check_session,
        "check execution request preparation failed: " +
            std::string(problem.what()));
  }
}

const transaction_check_request&
transaction_check_session::request() const noexcept
{
  return request_;
}

const pkgcheck_exec::admitted_check_session&
transaction_check_session::execution_session() const noexcept
{
  return execution_session_;
}

const pkgexec::execution_request_identity&
transaction_check_session::execution_request() const noexcept
{
  return execution_request_;
}

const session_identity& transaction_check_session::identity() const noexcept
{
  return identity_;
}

native_transaction_check_driver::native_transaction_check_driver(
    pkgexec::execution_backend& backend)
    : backend_(backend)
{
}

pkgcheck_exec::check_execution_result
native_transaction_check_driver::execute_check(
    const pkgcheck_exec::admitted_check_session& session)
{
  try {
    prepare_native_check_temporary(session);
    return pkgcheck_exec::execute(session, backend_);
  } catch (const pkgcheck_exec::error& problem) {
    throw error(
        error_code::check_driver_contract_violation,
        "native check execution failed: " + std::string(problem.what()));
  }
}

transaction_check_result::transaction_check_result(
    transaction_check_session session,
    pkgcheck_exec::check_execution_result execution,
    session_identity identity)
    : session_(std::move(session)),
      execution_(std::move(execution)),
      identity_(std::move(identity))
{
}

pkgcheck::check_outcome transaction_check_result::outcome() const noexcept
{
  return execution_.check().outcome();
}

bool transaction_check_result::succeeded() const noexcept
{
  return outcome() == pkgcheck::check_outcome::passed;
}

const transaction_check_session&
transaction_check_result::session() const noexcept
{
  return session_;
}

const pkgcheck_exec::check_execution_result&
transaction_check_result::execution() const noexcept
{
  return execution_;
}

const session_identity& transaction_check_result::identity() const noexcept
{
  return identity_;
}

transaction_check_result execute_transaction_check(
    transaction_check_session session,
    transaction_check_driver& driver)
{
  auto execution = invoke_check_driver(
      driver, session.execution_session());
  validate_driver_result(session, execution);

  auto identity = make_session_identity(
      "pkgctl/transaction-check-result/1",
      {
          session.identity().hex(),
          execution.execution().identity().hex(),
          execution.check().identity().hex(),
          std::to_string(
              static_cast<unsigned int>(execution.check().outcome())),
      });
  return transaction_check_result(
      std::move(session), std::move(execution), std::move(identity));
}

} // namespace pkgctl
