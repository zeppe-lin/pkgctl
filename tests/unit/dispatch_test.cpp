// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/check.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/error.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace construction_fixture;
namespace fs = std::filesystem;

static_assert(
    static_cast<std::uint8_t>(pkgctl::error_code::identity_failure) == 14U);
static_assert(
    static_cast<std::uint8_t>(pkgctl::error_code::invalid_dispatch) == 17U);

int failures = 0;
#define CHECK(value)                                                         \
  do {                                                                       \
    if (!(value)) {                                                          \
      std::cerr << "CHECK failed: " #value "\n";                           \
      ++failures;                                                            \
    }                                                                        \
  } while (false)

template<typename Function>
bool rejects(pkgctl::error_code expected, Function&& function)
{
  try
  {
    function();
  }
  catch (const pkgctl::error& problem)
  {
    return problem.code() == expected;
  }
  return false;
}

pkgctl::transaction_dispatch_nonce nonce(std::uint8_t marker)
{
  pkgctl::transaction_dispatch_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
}


template<typename Identity>
Identity external_identity(std::uint8_t marker)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string value(64U, '0');
  value[62] = digits[(marker >> 4U) & 0x0fU];
  value[63] = digits[marker & 0x0fU];
  return Identity::from_sha256(std::move(value));
}

const pkgtransaction::transaction_node& build_node_for(
    const pkgctl::transaction_session& transaction,
    const std::string& package)
{
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == package)
      return node;
  }
  throw std::runtime_error("dispatch fixture lacks requested build node");
}

pkgctl::construction_session construction_session_for(
    const pkgctl::transaction_session& transaction,
    const fs::path& root,
    const std::string& package,
    std::optional<std::vector<pkgbuild_exec::package_input_resource>>
        supplied_inputs = std::nullopt,
    std::uint32_t parallelism = 2U)
{
  const auto& node = build_node_for(transaction, package);
  auto request = pkgctl::construction_request::make(
      transaction, node.identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(
              parallelism, 0022, 1700000000)));

  pkgctl::construction_paths paths{
      root / "sources",
      root / "store",
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          root / "root-view",
          root / "session",
          root / "package",
          root / "artifact" / (package + ".tar"),
      },
  };
  fs::create_directories(paths.local_source_root);
  fs::create_directories(paths.build.root_view_path);

  std::vector<pkgbuild_exec::package_input_resource> inputs;
  if (supplied_inputs) {
    inputs = std::move(*supplied_inputs);
  } else {
    const auto build_inputs = request.build().inputs().for_scope(
        pkgbuild::input_scope::build);
    inputs.reserve(build_inputs.size());
    for (const auto& input : build_inputs) {
      const auto path = root / "inputs" / input.package().name();
      fs::create_directories(path);
      inputs.push_back({
          input.identity(),
          pkgexec::resource_identity::from_sha256(input.identity().hex()),
          path,
      });
    }
  }
  for (const auto& input : inputs)
    fs::create_directories(input.path);

  return pkgctl::construction_session::admit(
      std::move(request), std::move(paths), std::move(inputs),
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
}

pkgctl::construction_result execute_build(
    const pkgctl::construction_session& session,
    const std::string& payload,
    backend_mode mode)
{
  if (!session.request().source().recipe().sources().empty())
    test_support::write(
        session.paths().local_source_root / "payload", payload);
  fixture_backend backend(mode);
  pkgctl::native_construction_driver driver(backend);
  return pkgctl::execute_construction(session, driver);
}


struct two_build_fixture final {
  test_support::temporary_directory temporary;
  pkgstate::posix::canonical_generation_store store;
  std::string payload;
  pkgsource::source_snapshot source;
  pkgctl::transaction_session transaction;
  pkgctl::transaction_progress progress;

  two_build_fixture()
      : store(temporary.path() / "state", test_support::binding()),
        payload("source payload\n"),
        source(tool_source(
            sha256_text(payload),
            tool_source_options{
                "1.0", false, {}, std::nullopt})),
        transaction(transaction_session(
            source,
            std::vector<pkgsource::source_snapshot>{dependency_source()},
            store.read(), temporary.path() / "state",
            false, true, false)),
        progress(pkgctl::transaction_progress::begin(transaction))
  {
  }
};


struct dependent_build_fixture final {
  test_support::temporary_directory temporary;
  pkgstate::posix::canonical_generation_store store;
  std::string payload;
  pkgsource::source_snapshot source;
  pkgctl::transaction_session transaction;
  pkgctl::transaction_progress progress;

  dependent_build_fixture()
      : store(temporary.path() / "state", test_support::binding()),
        payload("source payload\n"),
        source(tool_source(
            sha256_text(payload),
            tool_source_options{
                "1.0", true, {}, std::nullopt})),
        transaction(transaction_session(
            source,
            std::vector<pkgsource::source_snapshot>{dependency_source()},
            store.read(), temporary.path() / "state")),
        progress(pkgctl::transaction_progress::begin(transaction))
  {
  }
};


struct check_input_fixture final {
  test_support::temporary_directory temporary;
  pkgstate::posix::canonical_generation_store store;
  std::string payload;
  pkgsource::source_snapshot source;
  pkgctl::transaction_session transaction;
  pkgctl::transaction_progress progress;

  check_input_fixture()
      : store(temporary.path() / "state", test_support::binding()),
        payload("source payload\n"),
        source(tool_source(
            sha256_text(payload),
            tool_source_options{
                "1.0", false, {"tester"},
                pkgsource::program(
                    pkgsource::program_language::posix_shell,
                    "printf 'checked\\n'\n")})),
        transaction(transaction_session(
            source,
            std::vector<pkgsource::source_snapshot>{
                package_source("tester")},
            store.read(), temporary.path() / "state",
            false, false, true)),
        progress(pkgctl::transaction_progress::begin(transaction))
  {
  }
};

std::vector<pkgexec::execution_guarantee> check_guarantees()
{
  return {
      pkgexec::execution_guarantee::exact_interpreter,
      pkgexec::execution_guarantee::closed_environment,
      pkgexec::execution_guarantee::root_view,
      pkgexec::execution_guarantee::read_only_resources,
      pkgexec::execution_guarantee::writable_resources,
      pkgexec::execution_guarantee::fixed_credentials,
      pkgexec::execution_guarantee::network_denied,
      pkgexec::execution_guarantee::loopback_isolated,
      pkgexec::execution_guarantee::resource_limits,
      pkgexec::execution_guarantee::cancellation,
      pkgexec::execution_guarantee::complete_stdout_capture,
      pkgexec::execution_guarantee::complete_stderr_capture,
      pkgexec::execution_guarantee::cleanup_verified,
      pkgexec::execution_guarantee::cpu_time_limit,
      pkgexec::execution_guarantee::address_space_limit,
      pkgexec::execution_guarantee::file_size_limit,
      pkgexec::execution_guarantee::open_files_limit,
      pkgexec::execution_guarantee::process_count_limit,
  };
}

class check_backend final : public pkgexec::execution_backend {
public:
  explicit check_backend(bool succeed) : succeed_(succeed) {}

  pkgexec::backend_capability_profile capabilities() const override
  {
    return pkgexec::backend_capability_profile::seal(
        pkgexec::backend_identity::from_sha256(std::string(64U, '8')),
        check_guarantees());
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources&) override
  {
    if (request.purpose().kind() != pkgexec::execution_purpose_kind::check)
      throw std::runtime_error("dispatch check backend received wrong purpose");

    auto profile = capabilities();
    if (succeed_)
      return pkgexec::execution_result::succeeded(
          request, std::move(profile), request.interpreter(),
          pkgexec::stream_capture::retained("check passed\n"),
          pkgexec::stream_capture::retained(""),
          request.required_guarantees(), "dispatch check success");

    return pkgexec::execution_result::failed_after_start(
        request, std::move(profile), request.interpreter(),
        pkgexec::process_termination::exited(1),
        pkgexec::stream_capture::retained(""),
        pkgexec::stream_capture::retained("check failed\n"),
        request.required_guarantees(),
        pkgexec::cleanup_outcome::verified,
        pkgexec::execution_failure_kind::program_exited_nonzero,
        "dispatch check failure");
  }

private:
  bool succeed_;
};

pkgctl::transaction_check_resources check_resources(
    const pkgctl::construction_result& construction,
    const fs::path& root,
    std::uint8_t marker)
{
  const auto& build = construction.build().build();
  const auto& artifact = build.artifact();
  if (!artifact)
    throw std::runtime_error("dispatch check fixture lacks artifact");

  fs::create_directories(root / "source");
  fs::create_directories(root / "package");
  fs::create_directories(root / "root-view");
  fs::create_directories(root / "temporary");

  std::vector<pkgcheck_exec::package_input_resource> inputs;
  const auto check_inputs =
      build.request().inputs().for_scope(pkgbuild::input_scope::check);
  inputs.reserve(check_inputs.size());
  for (std::size_t index = 0; index < check_inputs.size(); ++index)
  {
    const auto& logical = check_inputs[index];
    const auto path = root / "inputs" / logical.identity().hex();
    fs::create_directories(path);
    inputs.push_back({
        logical.identity(),
        external_identity<pkgexec::resource_identity>(
            static_cast<std::uint8_t>(marker + 4U + index)),
        path,
    });
  }

  return {
      {
          construction.session().request().source().identity(),
          external_identity<pkgexec::resource_identity>(marker),
          root / "source",
      },
      {
          artifact->identity(),
          external_identity<pkgexec::resource_identity>(
              static_cast<std::uint8_t>(marker + 1U)),
          root / "package",
      },
      std::move(inputs),
      {
          external_identity<pkgexec::root_view_identity>(
              static_cast<std::uint8_t>(marker + 2U)),
          root / "root-view",
          root / "temporary",
      },
      {
          external_identity<pkgexec::interpreter_identity>(
              static_cast<std::uint8_t>(marker + 3U)),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      },
      pkgexec::resource_limits::make(),
  };
}

struct ready_check_fixture final {
  test_support::temporary_directory temporary;
  pkgstate::posix::canonical_generation_store store;
  std::string payload;
  pkgsource::source_snapshot source;
  pkgctl::transaction_session transaction;
  pkgctl::construction_session construction_session;
  pkgctl::construction_result construction;
  pkgctl::transaction_progress progress;

  explicit ready_check_fixture(bool with_unrelated_build = false)
      : store(temporary.path() / "state", test_support::binding()),
        payload("source payload\n"),
        source(tool_source(
            sha256_text(payload),
            tool_source_options{
                "1.0", false, {},
                pkgsource::program(
                    pkgsource::program_language::posix_shell,
                    "printf 'checked\\n'\n")})),
        transaction(transaction_session(
            source,
            std::vector<pkgsource::source_snapshot>{dependency_source()},
            store.read(), temporary.path() / "state",
            true, with_unrelated_build, true)),
        construction_session(construction_session_for(
            transaction, temporary.path() / "tool-build", "tool")),
        construction(execute_build(
            construction_session, payload, backend_mode::succeed)),
        progress(pkgctl::advance_construction(
            pkgctl::transaction_progress::begin(transaction), construction))
  {
  }
};

pkgctl::transaction_check_session admit_check_session(
    const ready_check_fixture& fixture,
    const fs::path& root,
    std::uint8_t marker)
{
  auto request = pkgctl::transaction_check_request::make(
      fixture.progress, check_node(fixture.transaction).identity());
  return pkgctl::transaction_check_session::admit(
      std::move(request),
      check_resources(fixture.construction, root, marker));
}

void check_policy_and_nonce_validation()
{
  CHECK(rejects(pkgctl::error_code::invalid_dispatch_policy, [] {
    (void)pkgctl::transaction_dispatch_policy::make(0U, 1U);
  }));
  CHECK(rejects(pkgctl::error_code::invalid_dispatch_policy, [] {
    (void)pkgctl::transaction_dispatch_policy::make(1U, 0U);
  }));
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [] {
    (void)pkgctl::transaction_dispatch_nonce::from_bytes({});
  }));
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [] {
    (void)pkgctl::transaction_dispatch_nonce::from_hex("01");
  }));

  const auto first = nonce(1U);
  const auto second = pkgctl::transaction_dispatch_nonce::from_hex(first.hex());
  CHECK(first == second);
}

void check_deterministic_reservation_and_capacity()
{
  two_build_fixture fixture;
  CHECK(fixture.progress.ready_units().size() == 2U);

  auto policy = pkgctl::transaction_dispatch_policy::make(2U, 1U);
  auto first_run = pkgctl::transaction_run::begin(fixture.progress, policy);
  auto repeated_run = pkgctl::transaction_run::begin(fixture.progress, policy);
  CHECK(first_run.identity() == repeated_run.identity());

  auto first = pkgctl::reserve_next(first_run, nonce(1U));
  auto repeated = pkgctl::reserve_next(repeated_run, nonce(1U));
  CHECK(first.dispatch.has_value());
  CHECK(repeated.dispatch.has_value());
  CHECK(first.dispatch->identity() == repeated.dispatch->identity());
  CHECK(first.dispatch->unit().identity() ==
        fixture.progress.ready_units()[0].identity());
  CHECK(first.run.active_count(pkgctl::transaction_unit_kind::construction) ==
        1U);

  auto second = pkgctl::reserve_next(first.run, nonce(2U));
  CHECK(second.dispatch.has_value());
  CHECK(second.dispatch->unit().identity() ==
        fixture.progress.ready_units()[1].identity());
  CHECK(second.run.active_count(pkgctl::transaction_unit_kind::construction) ==
        2U);

  auto exhausted = pkgctl::reserve_next(second.run, nonce(3U));
  CHECK(!exhausted.dispatch.has_value());
  CHECK(exhausted.run.identity() == second.run.identity());

  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::reserve_next(second.run, nonce(1U));
  }));

  auto limited = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto limited_first = pkgctl::reserve_next(limited, nonce(4U));
  auto limited_exhausted = pkgctl::reserve_next(limited_first.run, nonce(5U));
  CHECK(limited_first.dispatch.has_value());
  CHECK(!limited_exhausted.dispatch.has_value());

  auto released = pkgctl::release_unstarted_dispatch(
      limited_first.run, *limited_first.dispatch);
  CHECK(released.records().front().state() ==
        pkgctl::transaction_dispatch_state::released_unstarted);
  CHECK(released.active_count(pkgctl::transaction_unit_kind::construction) ==
        0U);
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::reserve_next(released, nonce(4U));
  }));
  auto replacement = pkgctl::reserve_next(released, nonce(6U));
  CHECK(replacement.dispatch.has_value());
  CHECK(replacement.dispatch->unit().identity() ==
        limited_first.dispatch->unit().identity());
}


void check_foreign_dispatch_is_rejected()
{
  two_build_fixture first_fixture;
  dependent_build_fixture second_fixture;
  auto first_run = pkgctl::transaction_run::begin(
      first_fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto second_run = pkgctl::transaction_run::begin(
      second_fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto reservation = pkgctl::reserve_next(first_run, nonce(8U));
  CHECK(reservation.dispatch.has_value());
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::release_unstarted_dispatch(
        second_run, *reservation.dispatch);
  }));
}

void check_construction_completion_and_failure_containment()
{
  two_build_fixture fixture;
  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(2U, 1U));
  auto first = pkgctl::reserve_next(run, nonce(10U));
  auto second = pkgctl::reserve_next(first.run, nonce(11U));
  CHECK(first.dispatch && second.dispatch);

  const auto first_package = first.dispatch->unit().primary_node() ==
          build_node_for(fixture.transaction, "tool").identity()
      ? std::string("tool")
      : std::string("dep");
  const auto second_package = first_package == "tool" ? "dep" : "tool";

  auto first_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "first",
      first_package);
  auto second_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "second",
      second_package);

  auto started = pkgctl::start_construction_dispatch(
      second.run, *first.dispatch, first_session);
  started = pkgctl::start_construction_dispatch(
      std::move(started), *second.dispatch, second_session);
  CHECK(started.records()[0].state() ==
        pkgctl::transaction_dispatch_state::started);
  CHECK(started.records()[1].state() ==
        pkgctl::transaction_dispatch_state::started);
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::release_unstarted_dispatch(started, *first.dispatch);
  }));

  auto first_result = execute_build(
      first_session, fixture.payload, backend_mode::fail);
  auto second_result = execute_build(
      second_session, fixture.payload, backend_mode::succeed);

  auto after_failure = pkgctl::complete_construction_dispatch(
      started, *first.dispatch, first_result);
  CHECK(after_failure.progress().failed());
  CHECK(after_failure.records()[0].state() ==
        pkgctl::transaction_dispatch_state::completed);
  CHECK(after_failure.records()[1].state() ==
        pkgctl::transaction_dispatch_state::started);
  auto stopped = pkgctl::reserve_next(after_failure, nonce(12U));
  CHECK(!stopped.dispatch.has_value());

  auto completed = pkgctl::complete_construction_dispatch(
      after_failure, *second.dispatch, second_result);
  CHECK(completed.records()[1].state() ==
        pkgctl::transaction_dispatch_state::completed);
  CHECK(completed.active_count(pkgctl::transaction_unit_kind::construction) ==
        0U);
  CHECK(completed.progress().failed());

  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::complete_construction_dispatch(
        completed, *second.dispatch, second_result);
  }));
}

void check_failure_stops_unstarted_reservations()
{
  two_build_fixture fixture;
  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(2U, 1U));
  auto first = pkgctl::reserve_next(run, nonce(15U));
  auto second = pkgctl::reserve_next(first.run, nonce(16U));
  CHECK(first.dispatch && second.dispatch);

  const auto first_package = first.dispatch->unit().primary_node() ==
          build_node_for(fixture.transaction, "tool").identity()
      ? std::string("tool")
      : std::string("dep");
  const auto second_package = first_package == "tool" ? "dep" : "tool";
  auto first_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "stop-first",
      first_package);
  auto second_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "stop-second",
      second_package);

  auto started = pkgctl::start_construction_dispatch(
      second.run, *first.dispatch, first_session);
  auto failure = execute_build(
      first_session, fixture.payload, backend_mode::fail);
  auto stopped = pkgctl::complete_construction_dispatch(
      started, *first.dispatch, failure);
  CHECK(stopped.progress().failed());
  CHECK(stopped.records()[1].state() ==
        pkgctl::transaction_dispatch_state::reserved);
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::start_construction_dispatch(
        stopped, *second.dispatch, second_session);
  }));

  auto released = pkgctl::release_unstarted_dispatch(
      stopped, *second.dispatch);
  CHECK(released.records()[1].state() ==
        pkgctl::transaction_dispatch_state::released_unstarted);
}


void check_construction_binds_exact_predecessor_evidence()
{
  dependent_build_fixture fixture;
  CHECK(fixture.progress.ready_units().size() == 1U);
  CHECK(fixture.progress.ready_units().front().primary_node() ==
        build_node_for(fixture.transaction, "dep").identity());

  auto dependency_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "dependency", "dep");
  auto dependency = execute_build(
      dependency_session, fixture.payload, backend_mode::succeed);
  auto after_dependency = pkgctl::advance_construction(
      fixture.progress, dependency);
  CHECK(after_dependency.ready_units().size() == 1U);
  CHECK(after_dependency.ready_units().front().primary_node() ==
        build_node_for(fixture.transaction, "tool").identity());

  auto run = pkgctl::transaction_run::begin(
      after_dependency,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto reservation = pkgctl::reserve_next(run, nonce(18U));
  CHECK(reservation.dispatch.has_value());
  CHECK(reservation.dispatch->dependencies().size() == 1U);
  CHECK(reservation.dispatch->dependencies().front().evidence() ==
        dependency.identity());

  auto exact_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "exact-tool", "tool");
  CHECK(exact_session.request().inputs().size() == 1U);
  CHECK(exact_session.package_inputs().size() == 1U);
  CHECK(exact_session.package_inputs().front().input ==
        exact_session.request().inputs().front().identity());

  auto started = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, exact_session);
  CHECK(started.records().front().state() ==
        pkgctl::transaction_dispatch_state::started);
}


void check_check_inputs_are_constructed_before_checked_package()
{
  check_input_fixture fixture;
  CHECK(fixture.progress.ready_units().size() == 1U);
  CHECK(fixture.progress.ready_units().front().primary_node() ==
        build_node_for(fixture.transaction, "tester").identity());

  auto tester_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "tester-build",
      "tester");
  auto tester = execute_build(
      tester_session, fixture.payload, backend_mode::succeed);
  auto after_tester = pkgctl::advance_construction(
      fixture.progress, tester);
  CHECK(after_tester.ready_units().size() == 1U);
  CHECK(after_tester.ready_units().front().primary_node() ==
        build_node_for(fixture.transaction, "tool").identity());

  auto tool_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "checked-build",
      "tool");
  CHECK(tool_session.request().inputs().size() == 1U);
  CHECK(tool_session.request().inputs().front().scope() ==
        pkgbuild::input_scope::check);
  CHECK(tool_session.package_inputs().empty());
  const auto check_input = tool_session.request().inputs().front();

  auto run = pkgctl::transaction_run::begin(
      after_tester,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto build_reservation = pkgctl::reserve_next(run, nonce(19U));
  CHECK(build_reservation.dispatch.has_value());
  CHECK(build_reservation.dispatch->dependencies().size() == 1U);
  auto started_build = pkgctl::start_construction_dispatch(
      build_reservation.run, *build_reservation.dispatch, tool_session);
  auto tool = execute_build(
      tool_session, fixture.payload, backend_mode::succeed);
  auto after_tool = pkgctl::complete_construction_dispatch(
      started_build, *build_reservation.dispatch, tool);

  CHECK(after_tool.progress().ready_units().size() == 1U);
  CHECK(after_tool.progress().ready_units().front().kind() ==
        pkgctl::transaction_unit_kind::check);
  auto check_reservation = pkgctl::reserve_next(
      after_tool, nonce(21U));
  CHECK(check_reservation.dispatch.has_value());
  CHECK(check_reservation.dispatch->dependencies().size() == 1U);
  CHECK(check_reservation.dispatch->dependencies().front().evidence() ==
        tool.identity());

  auto request = pkgctl::transaction_check_request::make(
      check_reservation.run.progress(),
      check_node(fixture.transaction).identity());
  CHECK(request.check().inputs().inputs().size() == 1U);
  CHECK(request.check().inputs().inputs().front().identity() ==
        check_input.identity());
  auto session = pkgctl::transaction_check_session::admit(
      std::move(request),
      check_resources(
          tool, fixture.temporary.path() / "checked-session", 0x60U));
  auto started_check = pkgctl::start_check_dispatch(
      check_reservation.run, *check_reservation.dispatch, session);

  check_backend backend(true);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  auto completed = pkgctl::complete_check_dispatch(
      started_check, *check_reservation.dispatch, result);
  CHECK(completed.progress().status(
            check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::satisfied);
}

void check_check_completion_and_cross_session_refusal()
{
  ready_check_fixture fixture;
  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto reservation = pkgctl::reserve_next(run, nonce(20U));
  CHECK(reservation.dispatch.has_value());
  CHECK(reservation.dispatch->unit().kind() ==
        pkgctl::transaction_unit_kind::check);
  CHECK(reservation.dispatch->dependencies().size() == 1U);
  CHECK(reservation.dispatch->dependencies().front().evidence() ==
        fixture.construction.identity());

  auto session = admit_check_session(
      fixture, fixture.temporary.path() / "check", 0x50U);
  auto alternate = admit_check_session(
      fixture, fixture.temporary.path() / "alternate-check", 0x20U);

  auto other_construction_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "other-build",
      "tool", std::nullopt, 3U);
  auto other_construction = execute_build(
      other_construction_session, fixture.payload, backend_mode::succeed);
  auto other_progress = pkgctl::advance_construction(
      pkgctl::transaction_progress::begin(fixture.transaction),
      other_construction);
  auto other_request = pkgctl::transaction_check_request::make(
      other_progress, check_node(fixture.transaction).identity());
  auto other_session = pkgctl::transaction_check_session::admit(
      std::move(other_request),
      check_resources(
          other_construction,
          fixture.temporary.path() / "other-check", 0x30U));
  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::start_check_dispatch(
        reservation.run, *reservation.dispatch, other_session);
  }));

  auto started = pkgctl::start_check_dispatch(
      reservation.run, *reservation.dispatch, session);

  check_backend backend(true);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  auto foreign = pkgctl::execute_transaction_check(alternate, driver);

  CHECK(rejects(pkgctl::error_code::invalid_dispatch, [&] {
    (void)pkgctl::complete_check_dispatch(
        started, *reservation.dispatch, foreign);
  }));

  auto completed = pkgctl::complete_check_dispatch(
      started, *reservation.dispatch, result);
  CHECK(completed.records().front().state() ==
        pkgctl::transaction_dispatch_state::completed);
  CHECK(completed.progress().status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(completed.progress().status(install_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
}

void check_failed_check_stops_new_reservations()
{
  ready_check_fixture fixture;
  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto reservation = pkgctl::reserve_next(run, nonce(25U));
  CHECK(reservation.dispatch.has_value());
  auto session = admit_check_session(
      fixture, fixture.temporary.path() / "failed-check", 0x40U);
  auto started = pkgctl::start_check_dispatch(
      reservation.run, *reservation.dispatch, session);

  check_backend backend(false);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  CHECK(!result.succeeded());
  auto completed = pkgctl::complete_check_dispatch(
      started, *reservation.dispatch, result);
  CHECK(completed.progress().failed());
  CHECK(completed.progress().status(
            check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(completed.progress().status(
            install_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::blocked);
  auto stopped = pkgctl::reserve_next(completed, nonce(26U));
  CHECK(!stopped.dispatch.has_value());
}

void check_unrelated_progress_does_not_stale_check()
{
  ready_check_fixture fixture(true);
  CHECK(fixture.progress.ready_units().size() == 2U);

  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto first = pkgctl::reserve_next(run, nonce(30U));
  auto second = pkgctl::reserve_next(first.run, nonce(31U));
  CHECK(first.dispatch && second.dispatch);

  const pkgctl::transaction_dispatch* check_dispatch = nullptr;
  const pkgctl::transaction_dispatch* build_dispatch = nullptr;
  for (const auto* candidate : {&*first.dispatch, &*second.dispatch})
  {
    if (candidate->unit().kind() == pkgctl::transaction_unit_kind::check)
      check_dispatch = candidate;
    else if (candidate->unit().kind() ==
             pkgctl::transaction_unit_kind::construction)
      build_dispatch = candidate;
  }
  CHECK(check_dispatch != nullptr);
  CHECK(build_dispatch != nullptr);
  if (check_dispatch == nullptr || build_dispatch == nullptr)
    return;

  auto check_session = admit_check_session(
      fixture, fixture.temporary.path() / "check", 0x50U);
  auto dep_session = construction_session_for(
      fixture.transaction, fixture.temporary.path() / "dep-build", "dep");

  auto started = pkgctl::start_check_dispatch(
      second.run, *check_dispatch, check_session);
  started = pkgctl::start_construction_dispatch(
      std::move(started), *build_dispatch, dep_session);

  auto dep_result = execute_build(
      dep_session, fixture.payload, backend_mode::succeed);
  check_backend backend(true);
  pkgctl::native_transaction_check_driver driver(backend);
  auto check_result = pkgctl::execute_transaction_check(check_session, driver);

  const auto reserved_progress = check_dispatch->reserved_from_progress();
  auto after_build = pkgctl::complete_construction_dispatch(
      started, *build_dispatch, dep_result);
  CHECK(after_build.progress().identity() != reserved_progress);
  CHECK(after_build.progress().status(
            check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::ready);

  auto completed = pkgctl::complete_check_dispatch(
      after_build, *check_dispatch, check_result);
  CHECK(completed.progress().status(
            check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::satisfied);
}

} // namespace

int main()
{
  try
  {
    check_policy_and_nonce_validation();
    check_deterministic_reservation_and_capacity();
    check_foreign_dispatch_is_rejected();
    check_construction_completion_and_failure_containment();
    check_failure_stops_unstarted_reservations();
    check_construction_binds_exact_predecessor_evidence();
    check_check_inputs_are_constructed_before_checked_package();
    check_check_completion_and_cross_session_refusal();
    check_failed_check_stops_new_reservations();
    check_unrelated_progress_does_not_stale_check();
  }
  catch (const std::exception& problem)
  {
    std::cerr << "unexpected exception: " << problem.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
