// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/run_cleanup.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

using namespace construction_fixture;
namespace fs = std::filesystem;

int failures = 0;
#define CHECK(value)                                                         \
  do {                                                                       \
    if (!(value)) {                                                          \
      std::cerr << "CHECK failed: " #value "\n";                          \
      ++failures;                                                            \
    }                                                                        \
  } while (false)


std::string read_text(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read cleanup fixture file");
  return std::string(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

pkgctl::transaction_run_nonce run_nonce(std::uint8_t marker)
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

pkgctl::native_transaction_session_configuration configuration(
    const fs::path& root)
{
  fs::create_directories(root / "root-view");
  return pkgctl::native_transaction_session_configuration::make(
      {
          root / "content",
          root / "construction-sessions",
          root / "package-outputs",
          root / "artifacts",
          root / "check-resources",
          root / "check-temporary",
          pkgexec::root_view_identity::from_sha256(std::string(64U, '8')),
          root / "root-view",
      },
      {
          pkgbuild::build_policy::make(
              pkgbuild::environment_policy::hermetic(
                  2U, 0022, 1700000000)),
          pkgfetch::acquisition_policy::defaults(),
          {
              pkgexec::interpreter_identity::from_sha256(
                  std::string(64U, '9')),
              static_cast<std::uint64_t>(::geteuid()),
              static_cast<std::uint64_t>(::getegid()),
              {},
          },
          {
              pkgexec::interpreter_identity::from_sha256(
                  std::string(64U, 'a')),
              static_cast<std::uint64_t>(::geteuid()),
              static_cast<std::uint64_t>(::getegid()),
              {},
          },
          pkgexec::resource_limits::make(),
          pkgbuild::artifact_compression::none,
      });
}

struct run_records final {
  pkgctl::transaction_run_journal_record admitted;
  pkgctl::transaction_run_journal_record started;
  pkgctl::transaction_run_journal_record completed;
  pkgctl::transaction_run_journal_record failed;
};

run_records records(const fs::path& root)
{
  const auto state_path = root / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());
  const std::string payload = "cleanup payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(), state_path);

  const auto make_started = [&](std::uint8_t marker,
                                const fs::path& attempt_root) {
    auto session = construction_session_without_inputs(
        transaction, attempt_root);
    test_support::write(session.paths().local_source_root / "payload", payload);
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, run_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch)
      throw std::runtime_error("cleanup fixture did not reserve construction");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);
    return std::make_tuple(
        std::move(session), std::move(admitted), std::move(started_run),
        std::move(started), *reservation.dispatch);
  };

  auto [success_session, admitted, success_started_run, started, success_dispatch] =
      make_started(21U, root / "success-attempt");
  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto success = pkgctl::execute_construction(success_session, driver);
  auto completed_run = pkgctl::complete_construction_dispatch(
      std::move(success_started_run), success_dispatch, std::move(success));
  auto completed = started.successor(completed_run);

  auto [failure_session, ignored_admitted, failure_started_run,
        failure_started, failure_dispatch] =
      make_started(22U, root / "failure-attempt");
  (void)ignored_admitted;
  fixture_backend failure_backend(backend_mode::fail);
  pkgctl::native_construction_driver failure_driver(failure_backend);
  auto failure = pkgctl::execute_construction(failure_session, failure_driver);
  auto failed_run = pkgctl::complete_construction_dispatch(
      std::move(failure_started_run), failure_dispatch, std::move(failure));
  auto failed = failure_started.successor(failed_run);

  return {
      std::move(admitted), std::move(started), std::move(completed),
      std::move(failed)};
}


pkgctl::transaction_run_journal_record completed_after_released_reservation(
    const fs::path& root)
{
  const auto state_path = root / "released-state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());
  const std::string payload = "released cleanup payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(), state_path);
  auto session = construction_session_without_inputs(
      transaction, root / "released-attempt");
  test_support::write(session.paths().local_source_root / "payload", payload);

  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto journal = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(23U));

  auto first = pkgctl::reserve_next(run, dispatch_nonce(23U));
  if (!first.dispatch)
    throw std::runtime_error("cleanup fixture did not reserve released dispatch");
  auto first_reserved = journal.successor(first.run);
  auto released_run = pkgctl::release_unstarted_dispatch(
      first.run, *first.dispatch);
  auto released = first_reserved.successor(released_run);

  auto second = pkgctl::reserve_next(released_run, dispatch_nonce(24U));
  if (!second.dispatch)
    throw std::runtime_error("cleanup fixture did not reserve retry dispatch");
  auto second_reserved = released.successor(second.run);
  auto started_run = pkgctl::start_construction_dispatch(
      second.run, *second.dispatch, session);
  auto started = second_reserved.successor(started_run);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  auto completed_run = pkgctl::complete_construction_dispatch(
      std::move(started_run), *second.dispatch, std::move(result));
  auto completed = started.successor(completed_run);
  if (!completed.complete() || completed.failed())
    throw std::runtime_error("cleanup released fixture did not complete");
  return completed;
}


struct checked_run_records final {
  pkgctl::transaction_run_journal_record check_started;
  pkgctl::transaction_run_journal_record completed;
};

class passing_check_backend final : public pkgexec::execution_backend {
public:
  pkgexec::backend_capability_profile capabilities() const override
  {
    return construction_fixture::capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources&) override
  {
    if (request.purpose().kind() != pkgexec::execution_purpose_kind::check)
      throw std::runtime_error("cleanup check backend received non-check request");
    return pkgexec::execution_result::succeeded(
        request, capabilities(), request.interpreter(),
        pkgexec::stream_capture::retained("cleanup check passed\n"),
        pkgexec::stream_capture::retained(""), request.required_guarantees(),
        "cleanup check success");
  }
};

class passing_check_driver final : public pkgctl::transaction_check_driver {
public:
  pkgcheck_exec::check_execution_result execute_check(
      const pkgctl::transaction_check_session& session) override
  {
    return pkgcheck_exec::execute(session.execution_session(), backend_);
  }

private:
  passing_check_backend backend_;
};

checked_run_records checked_records(const fs::path& root)
{
  const auto state_path = root / "checked-state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "checked cleanup payload\n";
  tool_source_options options;
  options.with_build_dependency = false;
  options.check_program = pkgsource::program(
      pkgsource::program_language::posix_shell, "true\n");
  auto source = tool_source(sha256_text(payload), std::move(options));
  auto transaction = transaction_session(
      source, std::vector<pkgsource::source_snapshot>{}, state_store.read(),
      state_path, false, false, true);

  auto construction = construction_session_without_inputs(
      transaction, root / "checked-construction");
  test_support::write(
      construction.paths().local_source_root / "payload", payload);

  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto journal = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(31U));
  auto build_reservation = pkgctl::reserve_next(run, dispatch_nonce(31U));
  if (!build_reservation.dispatch ||
      build_reservation.dispatch->unit().kind() !=
          pkgctl::transaction_unit_kind::construction)
    throw std::runtime_error("cleanup checked fixture did not reserve build");
  auto reserved_build = journal.successor(build_reservation.run);
  auto started_build = pkgctl::start_construction_dispatch(
      build_reservation.run, *build_reservation.dispatch, construction);
  auto started_build_record = reserved_build.successor(started_build);

  fixture_backend build_backend(backend_mode::succeed);
  pkgctl::native_construction_driver build_driver(build_backend);
  auto construction_result = pkgctl::execute_construction(
      construction, build_driver);
  auto built = pkgctl::complete_construction_dispatch(
      std::move(started_build), *build_reservation.dispatch,
      std::move(construction_result));
  auto built_record = started_build_record.successor(built);
  CHECK(!built_record.complete());

  auto check_reservation = pkgctl::reserve_next(built, dispatch_nonce(32U));
  if (!check_reservation.dispatch ||
      check_reservation.dispatch->unit().kind() !=
          pkgctl::transaction_unit_kind::check)
    throw std::runtime_error("cleanup checked fixture did not reserve check");
  auto reserved_check = built_record.successor(check_reservation.run);

  auto request = pkgctl::transaction_check_request::make(
      check_reservation.run.progress(), check_node(transaction).identity());
  const auto& artifact = request.construction().build().build().artifact();
  if (!artifact)
    throw std::runtime_error("cleanup checked fixture lacks build artifact");
  const auto check_root = root / "checked-check";
  fs::create_directories(check_root / "source");
  fs::create_directories(check_root / "package");
  fs::create_directories(check_root / "root-view");
  fs::create_directories(check_root / "temporary");
  auto check_session = pkgctl::transaction_check_session::admit(
      std::move(request),
      {
          {
              source.identity(),
              pkgexec::resource_identity::from_sha256(std::string(64U, 'b')),
              check_root / "source",
          },
          {
              artifact->identity(),
              pkgexec::resource_identity::from_sha256(std::string(64U, 'c')),
              check_root / "package",
          },
          {},
          {
              pkgexec::root_view_identity::from_sha256(std::string(64U, 'd')),
              check_root / "root-view",
              check_root / "temporary",
          },
          {
              pkgexec::interpreter_identity::from_sha256(std::string(64U, 'e')),
              static_cast<std::uint64_t>(::geteuid()),
              static_cast<std::uint64_t>(::getegid()),
              {},
          },
          pkgexec::resource_limits::make(),
      });
  auto check_started_run = pkgctl::start_check_dispatch(
      check_reservation.run, *check_reservation.dispatch, check_session);
  auto check_started = reserved_check.successor(check_started_run);

  passing_check_driver check_driver;
  auto check_result = pkgctl::execute_transaction_check(
      std::move(check_session), check_driver);
  auto checked = pkgctl::complete_check_dispatch(
      std::move(check_started_run), *check_reservation.dispatch,
      std::move(check_result));
  auto completed = check_started.successor(checked);
  if (!completed.complete() || completed.failed())
    throw std::runtime_error("cleanup checked fixture did not complete");

  return {std::move(check_started), std::move(completed)};
}

class unknown_refusing_cleaner final
    : public pkgctl::transaction_run_private_realization_cleaner {
public:
  void remove(
      const pkgctl::transaction_run_private_realization& target) override
  {
    paths_.push_back(target.path());
    if (paths_.size() == 1U)
      throw 17;
  }

  [[nodiscard]] const std::vector<fs::path>& paths() const noexcept
  {
    return paths_;
  }

private:
  std::vector<fs::path> paths_;
};


class counting_cleaner final
    : public pkgctl::transaction_run_private_realization_cleaner {
public:
  explicit counting_cleaner(bool fail_first = false)
      : fail_first_(fail_first)
  {
  }

  void remove(
      const pkgctl::transaction_run_private_realization& target) override
  {
    paths_.push_back(target.path());
    if (fail_first_ && paths_.size() == 1U)
      throw std::runtime_error("injected cleanup refusal");
  }

  [[nodiscard]] const std::vector<fs::path>& paths() const noexcept
  {
    return paths_;
  }

private:
  bool fail_first_;
  std::vector<fs::path> paths_;
};

void check_cleanup_authority_and_failure_containment()
{
  test_support::temporary_directory temporary;
  auto value = records(temporary.path());
  auto config = configuration(temporary.path() / "runtime");

  const auto admitted = pkgctl::transaction_run_cleanup_plan::make(
      value.admitted, config);
  CHECK(admitted.disposition() ==
        pkgctl::transaction_run_cleanup_disposition::incomplete);
  CHECK(!admitted.eligible());
  CHECK(admitted.targets().empty());

  const auto started = pkgctl::transaction_run_cleanup_plan::make(
      value.started, config);
  CHECK(started.disposition() ==
        pkgctl::transaction_run_cleanup_disposition::incomplete);
  CHECK(!started.eligible());
  CHECK(started.targets().empty());

  const auto failed = pkgctl::transaction_run_cleanup_plan::make(
      value.failed, config);
  CHECK(failed.disposition() ==
        pkgctl::transaction_run_cleanup_disposition::stopped_after_failure);
  CHECK(!failed.eligible());
  CHECK(failed.targets().empty());

  auto completed = pkgctl::transaction_run_cleanup_plan::make(
      value.completed, config);
  CHECK(completed.disposition() ==
        pkgctl::transaction_run_cleanup_disposition::completed);
  CHECK(completed.eligible());
  CHECK(completed.journal() == value.completed.journal());
  CHECK(completed.record() == value.completed.identity());
  CHECK(completed.targets().size() == 2U);
  CHECK(completed.targets()[0].kind() ==
        pkgctl::transaction_run_private_realization_kind::construction_session);
  CHECK(completed.targets()[1].kind() ==
        pkgctl::transaction_run_private_realization_kind::package_output);
  for (const auto& target : completed.targets())
  {
    CHECK(target.dispatch() == value.completed.dispatches().front().dispatch().identity());
    CHECK(target.relative_path() ==
          fs::path(value.completed.journal().hex()) /
              value.completed.dispatches().front().dispatch().identity().hex());
  }

  counting_cleaner untouched;
  auto ineligible = pkgctl::cleanup_transaction_run_private_realizations(
      pkgctl::transaction_run_cleanup_plan::make(value.failed, config),
      untouched);
  CHECK(untouched.paths().empty());
  CHECK(ineligible.cleaned() == 0U);
  CHECK(ineligible.failures().empty());
  CHECK(!ineligible.complete());

  counting_cleaner refusing(true);
  const auto expected_record = completed.record();
  auto result = pkgctl::cleanup_transaction_run_private_realizations(
      std::move(completed), refusing);
  CHECK(result.plan().record() == expected_record);
  CHECK(refusing.paths().size() == 2U);
  CHECK(result.cleaned() == 1U);
  CHECK(result.failures().size() == 1U);
  CHECK(result.failures().front().problem == "injected cleanup refusal");
  CHECK(!result.complete());

  unknown_refusing_cleaner unknown;
  auto unknown_result = pkgctl::cleanup_transaction_run_private_realizations(
      pkgctl::transaction_run_cleanup_plan::make(value.completed, config),
      unknown);
  CHECK(unknown.paths().size() == 2U);
  CHECK(unknown_result.cleaned() == 1U);
  CHECK(unknown_result.failures().size() == 1U);
  CHECK(unknown_result.failures().front().problem == "unknown cleanup failure");
  CHECK(!unknown_result.complete());
}

void check_released_reservation_is_not_cleanup_authority()
{
  test_support::temporary_directory temporary;
  auto completed = completed_after_released_reservation(temporary.path());
  auto config = configuration(temporary.path() / "runtime");
  CHECK(completed.dispatches().size() == 2U);
  CHECK(completed.dispatches()[0].state() ==
        pkgctl::transaction_dispatch_state::released_unstarted);
  CHECK(completed.dispatches()[1].state() ==
        pkgctl::transaction_dispatch_state::completed);

  auto plan = pkgctl::transaction_run_cleanup_plan::make(completed, config);
  CHECK(plan.eligible());
  CHECK(plan.targets().size() == 2U);
  for (const auto& target : plan.targets())
    CHECK(target.dispatch() == completed.dispatches()[1].dispatch().identity());
}

void populate_target(
    const pkgctl::transaction_run_private_realization& target,
    const fs::path& external)
{
  fs::create_directories(target.path() / "nested");
  test_support::write(target.path() / "nested" / "payload", "garbage\n");
  fs::create_directory_symlink(external, target.path() / "external-link");
}


void check_build_and_check_stage_authority()
{
  test_support::temporary_directory temporary;
  auto value = checked_records(temporary.path());
  auto config = configuration(temporary.path() / "runtime");

  const auto started = pkgctl::transaction_run_cleanup_plan::make(
      value.check_started, config);
  CHECK(started.disposition() ==
        pkgctl::transaction_run_cleanup_disposition::incomplete);
  CHECK(!started.eligible());
  CHECK(started.targets().empty());

  const auto completed = pkgctl::transaction_run_cleanup_plan::make(
      value.completed, config);
  CHECK(completed.eligible());
  CHECK(completed.targets().size() == 4U);
  std::size_t constructions = 0U;
  std::size_t package_outputs = 0U;
  std::size_t check_resources = 0U;
  std::size_t check_temporaries = 0U;
  for (const auto& target : completed.targets())
  {
    CHECK(target.relative_path().parent_path() ==
          fs::path(value.completed.journal().hex()));
    switch (target.kind())
    {
      case pkgctl::transaction_run_private_realization_kind::construction_session:
        ++constructions;
        break;
      case pkgctl::transaction_run_private_realization_kind::package_output:
        ++package_outputs;
        break;
      case pkgctl::transaction_run_private_realization_kind::check_resource:
        ++check_resources;
        break;
      case pkgctl::transaction_run_private_realization_kind::check_temporary:
        ++check_temporaries;
        break;
    }
  }
  CHECK(constructions == 1U);
  CHECK(package_outputs == 1U);
  CHECK(check_resources == 1U);
  CHECK(check_temporaries == 1U);
}


void check_posix_cleanup_never_discovers_foreign_siblings()
{
  test_support::temporary_directory temporary;
  auto value = records(temporary.path());
  auto config = configuration(temporary.path() / "runtime");
  auto plan = pkgctl::transaction_run_cleanup_plan::make(value.completed, config);
  CHECK(plan.targets().size() == 2U);

  const auto external = temporary.path() / "external-sentinel";
  fs::create_directories(external);
  test_support::write(external / "keep", "outside\n");
  for (const auto& target : plan.targets())
    populate_target(target, external);

  const auto foreign = plan.targets().front().path().parent_path() / "foreign";
  fs::create_directories(foreign);
  test_support::write(foreign / "keep", "not-authorized\n");

  pkgctl::posix_transaction_run_private_realization_cleaner cleaner;
  auto result = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(result.complete());
  for (const auto& target : plan.targets())
    CHECK(!fs::exists(target.path()));
  CHECK(read_text(foreign / "keep") == "not-authorized\n");
  CHECK(read_text(external / "keep") == "outside\n");

  auto repeated = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(repeated.complete());
  CHECK(read_text(foreign / "keep") == "not-authorized\n");
}

void check_posix_cleanup_refuses_ancestor_substitution()
{
  {
    test_support::temporary_directory temporary;
    auto value = records(temporary.path());
    auto config = configuration(temporary.path() / "runtime");
    auto plan = pkgctl::transaction_run_cleanup_plan::make(
        value.completed, config);
    CHECK(plan.targets().size() == 2U);

    const auto external = temporary.path() / "root-external";
    fs::create_directories(external);
    test_support::write(external / "keep", "root-outside\n");
    fs::create_directories(plan.targets().front().root().parent_path());
    fs::create_directory_symlink(external, plan.targets().front().root());
    populate_target(plan.targets().back(), external);

    pkgctl::posix_transaction_run_private_realization_cleaner cleaner;
    auto result = pkgctl::cleanup_transaction_run_private_realizations(
        plan, cleaner);
    CHECK(!result.complete());
    CHECK(result.cleaned() == 1U);
    CHECK(result.failures().size() == 1U);
    CHECK(fs::is_symlink(plan.targets().front().root()));
    CHECK(read_text(external / "keep") == "root-outside\n");
    CHECK(!fs::exists(plan.targets().back().path()));
  }

  {
    test_support::temporary_directory temporary;
    auto value = records(temporary.path());
    auto config = configuration(temporary.path() / "runtime");
    auto plan = pkgctl::transaction_run_cleanup_plan::make(
        value.completed, config);
    CHECK(plan.targets().size() == 2U);

    const auto external = temporary.path() / "journal-external";
    fs::create_directories(external);
    test_support::write(external / "keep", "journal-outside\n");
    const auto& hostile = plan.targets().front();
    fs::create_directories(hostile.root());
    fs::create_directory_symlink(
        external, hostile.root() / value.completed.journal().hex());
    populate_target(plan.targets().back(), external);

    pkgctl::posix_transaction_run_private_realization_cleaner cleaner;
    auto result = pkgctl::cleanup_transaction_run_private_realizations(
        plan, cleaner);
    CHECK(!result.complete());
    CHECK(result.cleaned() == 1U);
    CHECK(result.failures().size() == 1U);
    CHECK(fs::is_symlink(
        hostile.root() / value.completed.journal().hex()));
    CHECK(read_text(external / "keep") == "journal-outside\n");
    CHECK(!fs::exists(plan.targets().back().path()));
  }
}


void check_posix_cleanup_removes_sealed_owner_directories()
{
  test_support::temporary_directory temporary;
  auto value = records(temporary.path());
  auto config = configuration(temporary.path() / "runtime");
  auto plan = pkgctl::transaction_run_cleanup_plan::make(
      value.completed, config);
  CHECK(plan.targets().size() == 2U);

  const auto external = temporary.path() / "sealed-external-sentinel";
  fs::create_directories(external);
  test_support::write(external / "keep", "outside\n");

  for (const auto& target : plan.targets())
  {
    populate_target(target, external);
    fs::permissions(
        target.path() / "nested" / "payload", fs::perms::owner_read,
        fs::perm_options::replace);
    fs::permissions(
        target.path() / "nested",
        fs::perms::owner_read | fs::perms::owner_exec,
        fs::perm_options::replace);
    fs::permissions(
        target.path(), fs::perms::owner_read | fs::perms::owner_exec,
        fs::perm_options::replace);
    fs::permissions(
        target.path().parent_path(),
        fs::perms::owner_read | fs::perms::owner_exec,
        fs::perm_options::replace);
  }

  pkgctl::posix_transaction_run_private_realization_cleaner cleaner;
  auto result = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(result.complete());
  CHECK(result.cleaned() == plan.targets().size());
  CHECK(result.failures().empty());
  for (const auto& target : plan.targets())
  {
    CHECK(!fs::exists(target.path()));
    CHECK(!fs::exists(target.root() / value.completed.journal().hex()));
  }
  CHECK(read_text(external / "keep") == "outside\n");
}

void check_posix_cleanup_is_idempotent_and_nofollow()
{
  test_support::temporary_directory temporary;
  auto value = records(temporary.path());
  auto config = configuration(temporary.path() / "runtime");
  auto plan = pkgctl::transaction_run_cleanup_plan::make(value.completed, config);
  CHECK(plan.targets().size() == 2U);

  const auto external = temporary.path() / "external-sentinel";
  fs::create_directories(external);
  test_support::write(external / "keep", "outside\n");
  for (const auto& target : plan.targets())
    populate_target(target, external);

  pkgctl::posix_transaction_run_private_realization_cleaner cleaner;
  auto cleaned = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(cleaned.complete());
  CHECK(cleaned.cleaned() == plan.targets().size());
  CHECK(cleaned.failures().empty());
  for (const auto& target : plan.targets())
  {
    CHECK(!fs::exists(target.path()));
    CHECK(!fs::exists(target.root() / value.completed.journal().hex()));
  }
  CHECK(read_text(external / "keep") == "outside\n");

  auto repeated = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(repeated.complete());
  CHECK(repeated.cleaned() == plan.targets().size());
  CHECK(repeated.failures().empty());

  const auto& hostile_target = plan.targets().front();
  fs::create_directories(hostile_target.path().parent_path());
  fs::create_directory_symlink(external, hostile_target.path());
  populate_target(plan.targets().back(), external);

  auto hostile = pkgctl::cleanup_transaction_run_private_realizations(
      plan, cleaner);
  CHECK(!hostile.complete());
  CHECK(hostile.cleaned() == 1U);
  CHECK(hostile.failures().size() == 1U);
  CHECK(hostile.failures().front().target.path() == hostile_target.path());
  CHECK(fs::is_symlink(hostile_target.path()));
  CHECK(read_text(external / "keep") == "outside\n");
  CHECK(!fs::exists(plan.targets().back().path()));
}

} // namespace

int main()
{
  try
  {
    check_cleanup_authority_and_failure_containment();
    check_released_reservation_is_not_cleanup_authority();
    check_build_and_check_stage_authority();
    check_posix_cleanup_never_discovers_foreign_siblings();
    check_posix_cleanup_refuses_ancestor_substitution();
    check_posix_cleanup_removes_sealed_owner_directories();
    check_posix_cleanup_is_idempotent_and_nofollow();
  }
  catch (const std::exception& problem)
  {
    std::cerr << "unexpected exception: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
