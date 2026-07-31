// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "construction_fixture.h"
#include "run_execute_support.h"

#include <pkgctl/run_execute.h>
#include <pkgctl/run_restart.h>

#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

namespace {

using namespace construction_fixture;

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

pkgctl::transaction_run_nonce journal_nonce(std::uint8_t marker)
{
  pkgctl::transaction_run_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_run_nonce::from_bytes(bytes);
}

pkgctl::transaction_dispatch_nonce dispatch_nonce(std::uint8_t marker)
{
  pkgctl::transaction_dispatch_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
}

class throwing_construction_driver final : public pkgctl::construction_driver {
public:
  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request&) override
  {
    throw std::runtime_error("driver escaped without materialization evidence");
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session&) override
  {
    throw std::runtime_error("unreachable build execution");
  }
};

class tracing_construction_driver final : public pkgctl::construction_driver {
public:
  tracing_construction_driver(
      pkgctl::construction_driver& driver, std::vector<std::string>& trace)
      : driver_(driver), trace_(trace)
  {
  }

  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) override
  {
    trace_.push_back("materialize");
    return driver_.materialize_source(request);
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session& session) override
  {
    trace_.push_back("build");
    return driver_.execute_build(session);
  }

private:
  pkgctl::construction_driver& driver_;
  std::vector<std::string>& trace_;
};

void check_success()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto session = construction_session(transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  CHECK(result.succeeded());
  CHECK(result.outcome() == pkgctl::construction_outcome::completed);
  CHECK(result.materialization().source().identity() == source.identity());
  CHECK(result.materialization().objects().size() == 1U);
  CHECK(result.build().build().outcome() == pkgbuild::build_outcome::succeeded);
  CHECK(result.build().artifact_inspection().has_value());
  CHECK(fs::is_regular_file(session.paths().build.artifact_path));

  auto progression = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::pending);
  bool refused = false;
  try
  {
    (void)pkgctl::advance_construction(progression, result);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(refused);

  auto other_transaction = transaction_session(
      tool_source(sha256_text(payload), "2.0"), dependency_source(),
      store.read(), temporary.path() / "state");
  auto other_progression =
      pkgctl::transaction_progress::begin(other_transaction);
  refused = false;
  try
  {
    (void)pkgctl::advance_construction(other_progression, result);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(refused);
}

void check_install_preparation()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true, true);
  CHECK(!transaction.program().nodes_for(
      pkgsource::package_reference("dep")).empty());
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(backend);
  auto construction = pkgctl::execute_construction(session, construction_driver);

  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(52);
  auto progression = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  bool dependency_ready = false;
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == "dep")
    {
      dependency_ready =
          progression.status(node.identity()) ==
          pkgctl::transaction_node_status::ready;
    }
  }
  CHECK(dependency_ready);
  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.status(install_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);

  auto request = pkgctl::operation_preparation_request::install(
      progression, install_node(transaction).identity(), construction,
      application_target(store.read().target_binding(), target_system),
      execution_control(), empty_target_observations(target_system),
      plan_identity<pkgplan::runtime_dependency_closure_identity>(53),
      package_policy(), pkgctl::lifecycle_order::make({}, {}),
      pkgstate::installation_reason::explicit_request());
  pkgimage::libarchive_backend archives;
  pkgctl::native_operation_preparation_driver preparation_driver(archives);
  const auto result = pkgctl::prepare_operation(
      std::move(request), preparation_driver);

  CHECK(result.prepared());
  CHECK(result.artifact().has_value());
  CHECK(result.incoming().has_value());
  CHECK(!result.refusal());
  CHECK(result.plan() &&
        result.plan()->kind() == pkgplan::operation_kind::install);
  CHECK(result.application() && result.application()->installation());
  CHECK(result.application() && result.plan() &&
        result.application()->plan() == result.plan()->identity());
  CHECK(result.application() && result.incoming() &&
        result.application()->incoming() &&
        result.application()->incoming()->identity() ==
            result.incoming()->identity());
  CHECK(result.effect() &&
        result.effect()->action_node() == install_node(transaction).identity());
  CHECK(result.effect() &&
        result.effect()->transaction().identity() == transaction.identity());
  CHECK(result.effect() && result.application() &&
        result.effect()->application().identity() ==
            result.application()->identity());
  CHECK(result.effect() &&
        result.effect()->expected_state().identity() ==
            progression.current_state().identity());
  CHECK(result.artifact() && construction.build().artifact_inspection() &&
        result.artifact()->image().receipt().archive_digest() ==
            construction.build().artifact_inspection()->archive_digest());
  CHECK(result.artifact()->image().receipt().image_identity() ==
        construction.build().artifact_inspection()->image_identity());
  CHECK(result.artifact()->image().receipt().entry_count() ==
        construction.build().artifact_inspection()->entry_count());
}

void check_check_progression()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(
      sha256_text(payload), "1.0", false,
      pkgsource::program(pkgsource::program_language::posix_shell, "true\n"));
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      false, false, true);
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto construction = pkgctl::execute_construction(session, driver);

  auto progression = pkgctl::transaction_progress::begin(transaction);
  const auto repeated = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.identity() == repeated.identity());
  const auto initial_progress = progression.identity();
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(check_node(transaction).check_program());
  CHECK(check_node(transaction).check_program()->material() == "true\n");
  CHECK(progression.status(check_node(transaction).identity()) ==
        pkgctl::transaction_node_status::pending);
  CHECK(progression.ready_units().size() == 1U);
  CHECK(progression.ready_units().front().kind() ==
        pkgctl::transaction_unit_kind::construction);

  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.identity() != initial_progress);
  CHECK(progression.status(check_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  bool duplicate_refused = false;
  try
  {
    (void)pkgctl::advance_construction(progression, construction);
  }
  catch (const pkgctl::error& problem)
  {
    duplicate_refused =
        problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(duplicate_refused);
  CHECK(progression.ready_units().size() == 1U);
  CHECK(progression.ready_units().front().kind() ==
        pkgctl::transaction_unit_kind::check);
  CHECK(progression.ready_units().front().primary_node() ==
        check_node(transaction).identity());
  CHECK(!progression.complete());
  CHECK(!progression.failed());
}

void check_failed_progression()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true);
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::fail);
  pkgctl::native_construction_driver driver(backend);
  auto construction = pkgctl::execute_construction(session, driver);
  CHECK(!construction.succeeded());

  auto progression = pkgctl::transaction_progress::begin(transaction);
  const auto initial_state = progression.current_state().identity();
  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(progression.status(install_node(transaction).identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(progression.current_state().identity() == initial_state);
  CHECK(progression.failed());
  CHECK(!progression.complete());
  CHECK(progression.ready_units().empty());
}

void check_failed_build()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto session = construction_session(transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::fail);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  CHECK(!result.succeeded());
  CHECK(result.outcome() == pkgctl::construction_outcome::build_failed);
  CHECK(result.build().build().outcome() == pkgbuild::build_outcome::failed);
  CHECK(!result.build().artifact_inspection().has_value());
  CHECK(!fs::exists(session.paths().build.artifact_path));
}

void check_admission()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");

  bool unknown_node = false;
  try
  {
    (void)pkgctl::construction_request::make(
        transaction,
        source_identity<pkgtransaction::transaction_node_identity>('f'),
        {dependency_input(transaction)},
        pkgbuild::build_policy::make(
            pkgbuild::environment_policy::hermetic(1)));
  }
  catch (const pkgctl::error& value)
  {
    unknown_node = value.code() ==
        pkgctl::error_code::invalid_construction_request;
  }
  CHECK(unknown_node);

  auto input = dependency_input(transaction);
  auto request = pkgctl::construction_request::make(
      transaction, build_node(transaction).identity(), {input},
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1)));
  pkgbuild_exec::package_input_tree exact{
      input.resolved().identity(), input.tree(), temporary.path() / "exact",
  };
  pkgbuild_exec::package_input_tree extra{
      source_identity<pkgbuild::resolved_package_input_identity>('1'),
      source_identity<pkgbuild::input_tree_identity>('2'),
      temporary.path() / "extra",
  };
  bool extra_tree = false;
  try
  {
    (void)pkgctl::construction_session::admit(
        request,
        {temporary.path() / "sources", temporary.path() / "store",
         {pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          temporary.path() / "root", temporary.path() / "session",
          temporary.path() / "package", temporary.path() / "artifact.tar"}},
        {exact, extra},
        {pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
         static_cast<std::uint64_t>(::geteuid()),
         static_cast<std::uint64_t>(::getegid()), {}});
  }
  catch (const pkgctl::error& value)
  {
    extra_tree = value.code() ==
        pkgctl::error_code::invalid_construction_session;
  }
  CHECK(extra_tree);

  bool overlapping_coordinates = false;
  try
  {
    (void)pkgctl::construction_session::admit(
        request,
        {temporary.path() / "shared", temporary.path() / "shared" / "store",
         {pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          temporary.path() / "root", temporary.path() / "session",
          temporary.path() / "package", temporary.path() / "artifact.tar"}},
        {exact},
        {pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
         static_cast<std::uint64_t>(::geteuid()),
         static_cast<std::uint64_t>(::getegid()), {}});
  }
  catch (const pkgctl::error& value)
  {
    overlapping_coordinates = value.code() ==
        pkgctl::error_code::invalid_construction_session;
  }
  CHECK(overlapping_coordinates);

  const auto& selected = dependency_selection(transaction);
  auto forged = pkgbuild::materialized_package_input(
      pkgbuild::resolved_package_input::make(
          pkgbuild::input_scope::build,
          pkgsource::package_reference("dep"),
          pkgsource::package_release(
              pkgsource::package_reference("dep"), "9.0", 1),
          selected.source_snapshot(),
          source_identity<pkgbuild::build_result_identity>('3'),
          source_identity<pkgbuild::artifact_identity>('4')),
      source_identity<pkgbuild::input_tree_identity>('5'));
  bool forged_input = false;
  try
  {
    (void)pkgctl::construction_request::make(
        transaction, build_node(transaction).identity(), {forged},
        pkgbuild::build_policy::make(
            pkgbuild::environment_policy::hermetic(1)));
  }
  catch (const pkgctl::error& value)
  {
    forged_input = value.code() ==
        pkgctl::error_code::invalid_construction_request;
  }
  CHECK(forged_input);
}


void check_identity_and_driver_contract()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto first = construction_session(transaction, temporary.path() / "first");
  auto second = construction_session(transaction, temporary.path() / "second");
  CHECK(first.identity() == second.identity());

  test_support::write(first.paths().local_source_root / "payload", payload);
  mismatched_materialization_driver mismatch(
      tool_source(sha256_text(payload), "2.0"));
  bool rejected = false;
  try
  {
    (void)pkgctl::execute_construction(first, mismatch);
  }
  catch (const pkgctl::error& value)
  {
    rejected = value.code() ==
        pkgctl::error_code::construction_driver_contract_violation;
  }
  CHECK(rejected);

  auto build_session = construction_session(
      transaction, temporary.path() / "mismatched-build");
  test_support::write(
      build_session.paths().local_source_root / "payload", payload);
  mismatched_build_driver mismatched_build;
  bool build_rejected = false;
  try
  {
    (void)pkgctl::execute_construction(build_session, mismatched_build);
  }
  catch (const pkgctl::error& value)
  {
    build_rejected = value.code() ==
        pkgctl::error_code::construction_driver_contract_violation;
  }
  CHECK(build_rejected);
}


void check_durable_dispatch_execution()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction, std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::construction)
      throw std::runtime_error(
          "fixture did not reserve a construction dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  const auto make_fixture = [](const std::filesystem::path& root) {
    test_support::initialize_state(root / "state");
    pkgstate::canonical_generation_store store(
        root / "state", test_support::binding());
    const std::string payload = "durable construction payload\n";
    auto source = tool_source(sha256_text(payload), "1.0", false);
    auto transaction = transaction_session(
        source, dependency_source(), store.read(), root / "state");
    auto session = construction_session_without_inputs(
        transaction, root / "construction");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    return std::make_pair(std::move(transaction), std::move(session));
  };

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 41U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    const auto completed = pkgctl::execute_construction_dispatch_durable(
        reserved, reservation.run, *reservation.dispatch,
        fixture.second, driver, run_store);

    CHECK(trace == std::vector<std::string>(
        {"run-1", "materialize", "build", "run-2"}));
    CHECK(completed.result.succeeded());
    CHECK(completed.record.sequence() == reserved.sequence() + 2U);
    CHECK(completed.run.records().size() == 1U);
    if (completed.run.records().size() == 1U)
    {
      CHECK(completed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
      CHECK(completed.run.records().front().terminal_evidence() ==
            std::optional<pkgctl::session_identity>(
                completed.result.identity()));
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 42U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 1U);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"run-1"}));
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 43U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 2U);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"run-1", "materialize", "build", "run-2"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto reopened = run_store.latest().reopen(reservation.run.progress());
    CHECK(reopened.records().size() == 1U);
    if (reopened.records().size() == 1U)
    {
      CHECK(reopened.records().front().state() ==
            pkgctl::transaction_dispatch_state::started);
    }
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                recover_construction);
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 44U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    throwing_construction_driver escaped;
    tracing_construction_driver driver(escaped, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, run_store);
    }
    catch (const std::runtime_error&)
    {
      failed = true;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"run-1", "materialize"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                recover_construction);
    }
  }
}

} // namespace

int main()
{
  try
  {
    check_success();
    check_install_preparation();
    check_check_progression();
    check_failed_progression();
    check_failed_build();
    check_admission();
    check_identity_and_driver_contract();
    check_durable_dispatch_execution();
  }
  catch (const std::exception& value)
  {
    std::cerr << "unexpected exception: " << value.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
