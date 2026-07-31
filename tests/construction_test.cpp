// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "construction_fixture.h"
#include "run_execute_support.h"

#include <pkgctl/run_admit.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_advance.h>
#include <pkgctl/run_drive.h>
#include <pkgctl/run_launch.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_reconcile.h>
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


class fixed_progress_source final
    : public pkgctl::transaction_progress_rehydration_source {
public:
  explicit fixed_progress_source(pkgctl::transaction_progress progress)
      : progress_(std::move(progress))
  {
  }

  pkgctl::transaction_progress rehydrate_progress(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++calls_;
    requested_record_ = record.identity();
    return progress_;
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& requested_record() const noexcept
  { return requested_record_; }

private:
  pkgctl::transaction_progress progress_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> requested_record_;
};




class replay_run_nonce_source final
    : public pkgctl::transaction_run_nonce_source {
public:
  replay_run_nonce_source(
      std::uint8_t marker,
      std::vector<std::string>& trace,
      bool refuse = false)
      : marker_(marker), trace_(trace), refuse_(refuse)
  {
  }

  pkgctl::transaction_run_nonce issue(
      const pkgctl::transaction_run& run) override
  {
    ++calls_;
    runs_.push_back(run.identity());
    trace_.push_back("nonce");
    if (refuse_)
      throw std::runtime_error("injected run-nonce refusal");
    return journal_nonce(marker_);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::vector<pkgctl::session_identity>& runs() const noexcept
  { return runs_; }

private:
  std::uint8_t marker_;
  std::vector<std::string>& trace_;
  bool refuse_;
  std::size_t calls_ = 0U;
  std::vector<pkgctl::session_identity> runs_;
};

class admission_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit admission_run_store(
      std::vector<std::string>& trace,
      std::size_t fail_on_append = 0U,
      std::optional<pkgctl::transaction_run_journal_record> returned =
          std::nullopt)
      : trace_(trace), fail_on_append_(fail_on_append),
        returned_(std::move(returned))
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    if (!latest_ || latest_->journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++append_calls_;
    trace_.push_back("append");
    if (append_calls_ == fail_on_append_)
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected admission-store failure");
    if (latest_)
    {
      if (latest_->identity() != record.identity())
        throw pkgctl::transaction_run_journal_error(
            pkgctl::transaction_run_journal_error_code::store_conflict,
            "foreign sequence-zero admission");
    }
    else
    {
      if (record.sequence() != 0U || record.previous())
        throw std::runtime_error("invalid admission-store input");
      latest_ = record;
    }
    return returned_ ? *returned_ : *latest_;
  }

  std::size_t append_calls() const noexcept { return append_calls_; }
  const std::optional<pkgctl::transaction_run_journal_record>& latest()
      const noexcept
  { return latest_; }

private:
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  std::optional<pkgctl::transaction_run_journal_record> returned_;
  std::optional<pkgctl::transaction_run_journal_record> latest_;
  std::size_t append_calls_ = 0U;
};

class launch_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit launch_run_store(
      std::vector<std::string>& trace,
      std::size_t fail_on_append = 0U)
      : trace_(trace), fail_on_append_(fail_on_append)
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    ++load_calls_;
    trace_.push_back("load");
    if (!latest_ || latest_->journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++append_calls_;
    trace_.push_back("append-" + std::to_string(append_calls_));
    if (append_calls_ == fail_on_append_)
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected launch-store failure");
    if (latest_ && latest_->identity() == record.identity())
      return *latest_;
    if (latest_)
      record.validate_successor_of(*latest_);
    else if (record.sequence() != 0U || record.previous())
      throw std::runtime_error("launch store did not begin at sequence zero");
    latest_ = record;
    return record;
  }

  std::size_t load_calls() const noexcept { return load_calls_; }
  std::size_t append_calls() const noexcept { return append_calls_; }
  const pkgctl::transaction_run_journal_record& latest() const
  {
    if (!latest_)
      throw std::runtime_error("launch store has no committed record");
    return *latest_;
  }

private:
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  mutable std::size_t load_calls_ = 0U;
  std::size_t append_calls_ = 0U;
  std::optional<pkgctl::transaction_run_journal_record> latest_;
};

class foreign_launch_head_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit foreign_launch_head_store(
      pkgctl::transaction_run_journal_record record)
      : record_(std::move(record))
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity&) const override
  {
    ++load_calls_;
    return record_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record&) override
  {
    ++append_calls_;
    throw std::runtime_error("foreign launch store reached append");
  }

  std::size_t load_calls() const noexcept { return load_calls_; }
  std::size_t append_calls() const noexcept { return append_calls_; }

private:
  pkgctl::transaction_run_journal_record record_;
  mutable std::size_t load_calls_ = 0U;
  std::size_t append_calls_ = 0U;
};

class head_derived_nonce_source final
    : public pkgctl::transaction_dispatch_nonce_source {
public:
  explicit head_derived_nonce_source(std::uint8_t domain)
      : domain_(domain)
  {
  }

  pkgctl::transaction_dispatch_nonce issue(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_run& run) override
  {
    ++calls_;
    records_.push_back(record.identity());
    runs_.push_back(run.identity());
    pkgctl::transaction_dispatch_nonce::byte_array bytes{};
    bytes[0] = domain_;
    bytes[30] = static_cast<std::uint8_t>((record.sequence() >> 8U) & 0xffU);
    bytes[31] = static_cast<std::uint8_t>(record.sequence() & 0xffU);
    if (bytes[0] == 0U && bytes[30] == 0U && bytes[31] == 0U)
      bytes[0] = 1U;
    return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::vector<pkgctl::session_identity>& records() const noexcept
  { return records_; }
  const std::vector<pkgctl::session_identity>& runs() const noexcept
  { return runs_; }

private:
  std::uint8_t domain_;
  std::size_t calls_ = 0U;
  std::vector<pkgctl::session_identity> records_;
  std::vector<pkgctl::session_identity> runs_;
};

class construction_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source {
public:
  explicit construction_execution_authority_source(
      pkgctl::construction_session session)
      : session_(std::move(session))
  {
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_run& run,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = record.identity();
    run_ = run.identity();
    dispatch_ = dispatch.identity();
    return session_;
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check execution authority request");
  }

  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation execution authority request");
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& run() const noexcept
  { return run_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::construction_session session_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> run_;
  std::optional<pkgctl::session_identity> dispatch_;
};


class throwing_construction_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source {
public:
  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    ++calls_;
    throw std::runtime_error("injected execution-authority failure");
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check execution authority request");
  }

  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation execution authority request");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class construction_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  explicit construction_recovery_authority_source(
      pkgctl::construction_result result)
      : result_(std::move(result))
  {
  }

  pkgctl::construction_result construction(
      const pkgctl::transaction_run_restart_checkpoint& checkpoint,
      const pkgctl::transaction_dispatch_restart_assessment& assessment,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = checkpoint.record().identity();
    assessment_ = assessment.dispatch();
    dispatch_ = dispatch.identity();
    return result_;
  }

  pkgctl::transaction_check_result check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check recovery authority request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery authority request");
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& assessment() const noexcept
  { return assessment_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::construction_result result_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> assessment_;
  std::optional<pkgctl::session_identity> dispatch_;
};

class unreachable_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  pkgctl::construction_result construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested construction evidence");
  }

  pkgctl::transaction_check_result check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested check evidence");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested operation evidence");
  }
};

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


void check_durable_restart_reconciliation()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction,
      std::uint8_t marker) {
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
    const std::string payload = "restart construction payload\n";
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
        reserve_construction(fixture.first, 51U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);

    const auto released = pkgctl::reconcile_reserved_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            reservation.run.progress(), reserved),
        *reservation.dispatch, run_store);
    CHECK(trace == std::vector<std::string>({"run-1"}));
    CHECK(released.record.sequence() == reserved.sequence() + 1U);
    CHECK(released.run.records().size() == 1U);
    if (released.run.records().size() == 1U)
      CHECK(released.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::released_unstarted);

    const auto repeated = pkgctl::reconcile_reserved_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            reservation.run.progress(), reserved),
        *reservation.dispatch, run_store);
    CHECK(repeated.record.identity() == released.record.identity());
    CHECK(run_store.latest().identity() == released.record.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 52U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto result = pkgctl::execute_construction(fixture.second, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    const auto completed = pkgctl::reconcile_construction_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, result, run_store);

    CHECK(trace == std::vector<std::string>({"run-1"}));
    CHECK(completed.result.identity() == result.identity());
    CHECK(completed.record.sequence() == started.sequence() + 1U);
    CHECK(completed.run.records().size() == 1U);
    if (completed.run.records().size() == 1U)
    {
      CHECK(completed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
      CHECK(completed.run.records().front().terminal_evidence() ==
            std::optional<pkgctl::session_identity>(result.identity()));
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 53U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto result = pkgctl::execute_construction(fixture.second, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace, 1U);
    bool failed = false;
    try
    {
      (void)pkgctl::reconcile_construction_dispatch_durable(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, result, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(run_store.latest().identity() == started.identity());

    const auto completed = pkgctl::reconcile_construction_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, result, run_store);
    CHECK(completed.record.sequence() == started.sequence() + 1U);
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 54U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    const std::string payload = "restart construction payload\n";
    pkgstate::canonical_generation_store state_store(
        temporary.path() / "state", test_support::binding());
    auto foreign_transaction = transaction_session(
        tool_source(sha256_text(payload), "2.0", false),
        dependency_source(), state_store.read(), temporary.path() / "state");
    auto foreign_session = construction_session_without_inputs(
        foreign_transaction, temporary.path() / "foreign-construction");
    test_support::write(
        foreign_session.paths().local_source_root / "payload", payload);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto foreign_result = pkgctl::execute_construction(
        foreign_session, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    bool refused = false;
    try
    {
      (void)pkgctl::reconcile_construction_dispatch_durable(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, foreign_result, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(trace.empty());
  }
}


void check_run_authority_rehydration()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction,
      std::uint8_t marker) {
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

  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "authority construction payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(session.paths().local_source_root / "payload", payload);

  auto [reservation, reserved] = reserve_construction(transaction, 61U);
  fixed_progress_source progress_source(reservation.run.progress());
  auto restart = pkgctl::rehydrate_transaction_run(reserved, progress_source);
  CHECK(progress_source.calls() == 1U);
  CHECK(progress_source.requested_record() ==
        std::optional<pkgctl::session_identity>(reserved.identity()));
  CHECK(restart.record().identity() == reserved.identity());
  CHECK(restart.run().identity() == reservation.run.identity());

  construction_execution_authority_source execution_source(session);
  const auto execution =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch, execution_source);
  CHECK(execution_source.calls() == 1U);
  CHECK(execution_source.record() ==
        std::optional<pkgctl::session_identity>(reserved.identity()));
  CHECK(execution_source.run() ==
        std::optional<pkgctl::session_identity>(reservation.run.identity()));
  CHECK(execution_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(execution.kind() == pkgctl::transaction_unit_kind::construction);
  CHECK(execution.construction() != nullptr);
  CHECK(execution.check() == nullptr);
  CHECK(execution.operation() == nullptr);
  if (execution.construction())
    CHECK(execution.construction()->identity() == session.identity());

  construction_execution_authority_source repeated_source(session);
  const auto repeated =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch, repeated_source);
  CHECK(repeated.identity() == execution.identity());

  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  construction_execution_authority_source active_execution(session);
  bool refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        started, started_run, *reservation.dispatch, active_execution);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
  CHECK(active_execution.calls() == 0U);

  construction_execution_authority_source mismatched_execution(session);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        reserved, started_run, *reservation.dispatch, mismatched_execution);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
  CHECK(mismatched_execution.calls() == 0U);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);

  fixed_progress_source started_progress_source(started_run.progress());
  auto started_restart = pkgctl::rehydrate_transaction_run(
      started, started_progress_source);
  construction_recovery_authority_source recovery_source(result);
  const auto recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          started_restart, *reservation.dispatch, recovery_source);
  CHECK(recovery_source.calls() == 1U);
  CHECK(recovery_source.record() ==
        std::optional<pkgctl::session_identity>(started.identity()));
  CHECK(recovery_source.assessment() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery.disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::
            recover_construction);
  CHECK(!recovery.releases_reserved());
  CHECK(recovery.construction() != nullptr);
  CHECK(recovery.check() == nullptr);
  CHECK(recovery.operation() == nullptr);
  if (recovery.construction())
    CHECK(recovery.construction()->identity() == result.identity());

  construction_recovery_authority_source repeated_recovery_source(result);
  const auto repeated_recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, repeated_recovery_source);
  CHECK(repeated_recovery.identity() == recovery.identity());

  unreachable_recovery_authority_source no_evidence;
  const auto release =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              reservation.run.progress(), reserved),
          *reservation.dispatch, no_evidence);
  CHECK(release.releases_reserved());
  CHECK(release.disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::release_reserved);

  auto foreign_transaction = transaction_session(
      tool_source(sha256_text(payload), "2.0", false),
      dependency_source(), store.read(), temporary.path() / "state");
  auto foreign_session = construction_session_without_inputs(
      foreign_transaction, temporary.path() / "foreign");
  fixed_progress_source foreign_progress_source(
      pkgctl::advance_construction(reservation.run.progress(), result));
  refused = false;
  try
  {
    (void)pkgctl::rehydrate_transaction_run(
        reserved, foreign_progress_source);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_record;
  }
  CHECK(refused);
  CHECK(foreign_progress_source.calls() == 1U);

  construction_execution_authority_source foreign_execution(foreign_session);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        reserved, reservation.run, *reservation.dispatch, foreign_execution);
  }
  catch (const pkgctl::error&)
  {
    refused = true;
  }
  CHECK(refused);

  test_support::write(
      foreign_session.paths().local_source_root / "payload", payload);
  auto foreign_result = pkgctl::execute_construction(foreign_session, driver);
  construction_recovery_authority_source foreign_recovery(foreign_result);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, foreign_recovery);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
}


void check_single_step_transaction_advancement()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "single-step construction payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction-step");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  const auto make_admitted = [&](std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  {
    auto [run, admitted] = make_admitted(71U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto advanced = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(71U),
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(advanced.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              executed_construction);
    CHECK(advanced.durable_transition_committed());
    CHECK(!advanced.external_resolution_required());
    CHECK(advanced.dispatch().has_value());
    CHECK(advanced.construction() != nullptr);
    CHECK(advanced.check() == nullptr);
    CHECK(advanced.operation() == nullptr);
    CHECK(advanced.record().sequence() == 3U);
    CHECK(advanced.record().identity() == run_store.latest().identity());
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 1U);
    CHECK(trace == std::vector<std::string>({
        "run-1", "run-2", "materialize", "build", "run-3"}));
  }

  {
    auto [run, admitted] = make_admitted(72U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(72U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("construction step fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);

    const auto released = pkgctl::advance_transaction_run_once(
        reserved.journal(), dispatch_nonce(73U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, nullptr});

    CHECK(released.disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(released.record().sequence() == 2U);
    CHECK(released.dispatch().has_value());
    CHECK(released.construction() == nullptr);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(74U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(74U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("construction recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);
    auto recovered_result = pkgctl::execute_construction(session, native_driver);

    fixed_progress_source progress_source(started_run.progress());
    construction_execution_authority_source execution_source(session);
    construction_recovery_authority_source recovery_source(recovered_result);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);

    const auto reconciled = pkgctl::advance_transaction_run_once(
        started.journal(), dispatch_nonce(75U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, nullptr});

    CHECK(reconciled.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              reconciled_construction);
    CHECK(reconciled.construction() != nullptr);
    CHECK(reconciled.construction() &&
          reconciled.construction()->identity() == recovered_result.identity());
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(76U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(76U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("stale-head fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto [foreign_run, foreign_admitted] = make_admitted(77U);
    fixed_progress_source progress_source(foreign_run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);

    bool refused = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          foreign_admitted.journal(), dispatch_nonce(77U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_conflict;
    }
    CHECK(refused);
    CHECK(progress_source.calls() == 0U);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace.empty());
  }


  {
    auto [run, admitted] = make_admitted(78U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);

    bool refused = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(78U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(run_store.latest().identity() == admitted.identity());
    CHECK(trace.empty());
  }

  {
    auto [run, admitted] = make_admitted(79U);
    fixed_progress_source progress_source(run.progress());
    throwing_construction_execution_authority_source execution_source;
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(79U),
          {progress_source, execution_source, recovery_source},
          {&native_driver, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      failed = std::string(problem.what()) ==
          "injected execution-authority failure";
    }
    CHECK(failed);
    CHECK(execution_source.calls() == 1U);
    CHECK(run_store.latest().sequence() == 1U);
    CHECK(run_store.latest().dispatches().size() == 1U);
    CHECK(run_store.latest().dispatches().front().state() ==
          pkgctl::transaction_dispatch_state::reserved);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(80U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    fixture_backend failing_backend(backend_mode::fail);
    pkgctl::native_construction_driver failing_driver(failing_backend);

    const auto failed = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(80U),
        {progress_source, execution_source, recovery_source},
        {&failing_driver, nullptr, nullptr}, {run_store, nullptr});
    CHECK(failed.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              executed_construction);
    CHECK(failed.construction() != nullptr);
    CHECK(failed.construction() && !failed.construction()->succeeded());
    CHECK(failed.run().stopped());

    fixed_progress_source stopped_progress(failed.run().progress());
    construction_execution_authority_source unused_execution(session);
    const auto trace_before = trace;
    const auto quiescent = pkgctl::advance_transaction_run_once(
        failed.record().journal(), dispatch_nonce(81U),
        {stopped_progress, unused_execution, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, nullptr});
    CHECK(quiescent.disposition() ==
          pkgctl::transaction_run_advance_disposition::quiescent);
    CHECK(!quiescent.durable_transition_committed());
    CHECK(!quiescent.dispatch().has_value());
    CHECK(quiescent.record().identity() == failed.record().identity());
    CHECK(unused_execution.calls() == 0U);
    CHECK(trace == trace_before);
  }

  {
    auto [run, admitted] = make_admitted(82U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    throwing_construction_driver driver;

    bool escaped = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(82U),
          {progress_source, execution_source, recovery_source},
          {&driver, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      escaped = std::string(problem.what()) ==
          "driver escaped without materialization evidence";
    }
    CHECK(escaped);
    CHECK(run_store.latest().sequence() == 2U);
    CHECK(run_store.latest().dispatches().size() == 1U);
    CHECK(run_store.latest().dispatches().front().state() ==
          pkgctl::transaction_dispatch_state::started);
    CHECK(trace == std::vector<std::string>({"run-1", "run-2"}));
  }
}



void check_durable_transaction_run_admission()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  auto transaction = transaction_session(
      tool_source(sha256_text("admission payload\n"), "1.0", false),
      dependency_source(), state_store.read(), temporary.path() / "state");
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto policy = pkgctl::transaction_dispatch_policy::make(1U, 1U);
  const auto expected_run = pkgctl::transaction_run::begin(progress, policy);

  {
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(101U, trace);
    admission_run_store store(trace);

    const auto admitted = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>({expected_run.identity()}));
    CHECK(store.append_calls() == 1U);
    CHECK(store.latest().has_value());
    CHECK(admitted.record.sequence() == 0U);
    CHECK(!admitted.record.previous().has_value());
    CHECK(admitted.record.dispatches().empty());
    CHECK(admitted.record.run() == admitted.run.identity());
    CHECK(admitted.record.progress() == admitted.run.progress().identity());
    CHECK(admitted.run.identity() == expected_run.identity());
    CHECK(store.latest() &&
          admitted.record.identity() == store.latest()->identity());

    const auto repeated = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(repeated.record.identity() == admitted.record.identity());
    CHECK(repeated.run.identity() == admitted.run.identity());
    CHECK(nonces.calls() == 2U);
    CHECK(nonces.runs() == std::vector<pkgctl::session_identity>(
          2U, expected_run.identity()));
    CHECK(store.append_calls() == 2U);
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source refusing(102U, trace, true);
    admission_run_store store(trace);
    bool refused = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, refusing, store);
    }
    catch (const std::runtime_error& problem)
    {
      refused = std::string(problem.what()) ==
          "injected run-nonce refusal";
    }
    CHECK(refused);
    CHECK(trace == std::vector<std::string>({"nonce"}));
    CHECK(store.append_calls() == 0U);
    CHECK(!store.latest().has_value());
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(103U, trace);
    admission_run_store store(trace, 1U);
    bool failed = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, nonces, store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
    CHECK(!store.latest().has_value());

    const auto retried = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(retried.record.sequence() == 0U);
    CHECK(nonces.calls() == 2U);
    CHECK(nonces.runs() == std::vector<pkgctl::session_identity>(
          2U, expected_run.identity()));
    CHECK(store.append_calls() == 2U);
  }

  {
    auto foreign_run = pkgctl::transaction_run::begin(
        progress, pkgctl::transaction_dispatch_policy::make(2U, 1U));
    auto foreign_record = pkgctl::transaction_run_journal_record::admit(
        foreign_run, journal_nonce(104U));
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(105U, trace);
    admission_run_store store(trace, 0U, foreign_record);
    bool rejected = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, nonces, store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_contract_violation;
    }
    CHECK(rejected);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
  }
}

void check_bounded_serial_transaction_drive()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "bounded drive payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "drive-construction");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  const auto make_admitted = [&](std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  {
    auto [run, admitted] = make_admitted(91U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(91U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(4U),
        nonces, {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(driven.terminal());
    CHECK(!driven.external_resolution_required());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.durable_step_count() == 1U);
    CHECK(driven.last().disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_construction);
    CHECK(driven.run().progress().complete());
    CHECK(driven.record().identity() == run_store.latest().identity());
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.records() ==
          std::vector<pkgctl::session_identity>({admitted.identity()}));
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>({run.identity()}));
  }

  {
    auto [run, admitted] = make_admitted(95U);
    auto failure_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-failure");
    test_support::write(
        failure_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(failure_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(95U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    fixture_backend failing_backend(backend_mode::fail);
    pkgctl::native_construction_driver failing_driver(failing_backend);

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(3U),
        nonces, {progress_source, execution_source, recovery_source},
        {&failing_driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::stopped_after_failure);
    CHECK(driven.terminal());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.run().stopped());
    CHECK(driven.run().progress().failed());
    CHECK(nonces.calls() == 1U);
  }

  {
    auto [run, admitted] = make_admitted(92U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(92U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("drive recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);

    auto recovery_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-recovery");
    test_support::write(
        recovery_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(recovery_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(93U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto driven = pkgctl::drive_transaction_run(
        reserved.journal(), pkgctl::transaction_run_drive_policy::make(3U),
        nonces, {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(driven.steps().size() == 2U);
    CHECK(driven.steps()[0].disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(driven.steps()[1].disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_construction);
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.records().size() == 1U);
    CHECK(nonces.records().front() == driven.steps()[0].record().identity());
  }

  {
    auto [run, admitted] = make_admitted(96U);
make_admitted(96U);
    auto retry_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-retry");
    test_support::write(
        retry_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(retry_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(96U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);

    for (std::size_t attempt = 0U; attempt < 2U; ++attempt)
    {
      bool refused = false;
      try
      {
        (void)pkgctl::drive_transaction_run(
            admitted.journal(),
            pkgctl::transaction_run_drive_policy::make(1U), nonces,
            {progress_source, execution_source, recovery_source},
            {nullptr, nullptr, nullptr}, {run_store, nullptr});
      }
      catch (const pkgctl::transaction_run_journal_error& problem)
      {
        refused = problem.code() ==
            pkgctl::transaction_run_journal_error_code::invalid_transition;
      }
      CHECK(refused);
      CHECK(run_store.latest().identity() == admitted.identity());
    }

    CHECK(nonces.calls() == 2U);
    CHECK(nonces.records() ==
          std::vector<pkgctl::session_identity>(2U, admitted.identity()));
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>(2U, run.identity()));
    CHECK(trace.empty());

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(1U),
        nonces, {progress_source, execution_source, recovery_source},
        {&native_driver, nullptr, nullptr}, {run_store, nullptr});
    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(nonces.calls() == 3U);
    CHECK(nonces.records().back() == admitted.identity());
    CHECK(nonces.runs().back() == run.identity());
  }

  {
    auto [run, admitted] = make_admitted(94U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(94U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("drive budget fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(94U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);

    const auto driven = pkgctl::drive_transaction_run(
        reserved.journal(), pkgctl::transaction_run_drive_policy::make(1U),
        nonces, {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::step_limit_reached);
    CHECK(!driven.terminal());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.last().disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(driven.durable_step_count() == 1U);
    CHECK(nonces.calls() == 0U);
  }

  bool refused = false;
  try
  {
    (void)pkgctl::transaction_run_drive_policy::make(0U);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
}


void check_restart_safe_transaction_launch()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "restart-safe launch payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  const auto initial = pkgctl::transaction_progress::begin(transaction);
  const auto policy = pkgctl::transaction_dispatch_policy::make(1U, 1U);
  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-construction");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(111U, trace);
    head_derived_nonce_source dispatch_nonces(111U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto launched = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(4U),
        run_nonces, dispatch_nonces,
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(launched.origin() ==
          pkgctl::transaction_run_launch_origin::admitted);
    CHECK(launched.admission_committed());
    CHECK(launched.starting_record().sequence() == 0U);
    CHECK(launched.drive().disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(launched.run().progress().complete());
    CHECK(launched.record().identity() == run_store.latest().identity());
    CHECK(run_nonces.calls() == 1U);
    CHECK(dispatch_nonces.calls() == 1U);
    CHECK(run_store.append_calls() == 4U);

    fixed_progress_source resumed_progress(launched.run().progress());
    construction_execution_authority_source resumed_execution(session);
    unreachable_recovery_authority_source resumed_recovery;
    const auto append_count = run_store.append_calls();
    const auto resumed = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {resumed_progress, resumed_execution, resumed_recovery},
        {&driver, nullptr, nullptr}, {run_store, nullptr});

    CHECK(resumed.origin() ==
          pkgctl::transaction_run_launch_origin::resumed);
    CHECK(!resumed.admission_committed());
    CHECK(resumed.starting_record().identity() == launched.record().identity());
    CHECK(resumed.record().identity() == launched.record().identity());
    CHECK(resumed.drive().disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(resumed.drive().steps().size() == 1U);
    CHECK(resumed.drive().last().disposition() ==
          pkgctl::transaction_run_advance_disposition::quiescent);
    CHECK(run_store.append_calls() == append_count);
    CHECK(run_nonces.calls() == 2U);
    CHECK(dispatch_nonces.calls() == 1U);
  }

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-admission-retry");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(112U, trace);
    head_derived_nonce_source dispatch_nonces(112U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace, 1U);
    tracing_construction_driver driver(native_driver, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {&driver, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(run_store.append_calls() == 1U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
    CHECK(std::find(trace.begin(), trace.end(), "materialize") == trace.end());
    CHECK(std::find(trace.begin(), trace.end(), "build") == trace.end());

    const auto retried = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, nullptr});
    CHECK(retried.admission_committed());
    CHECK(retried.run().progress().complete());
    CHECK(run_nonces.calls() == 2U);
  }

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-drive-retry");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(113U, trace);
    head_derived_nonce_source dispatch_nonces(113U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(run_store.latest().sequence() == 0U);
    CHECK(run_store.append_calls() == 1U);
    CHECK(dispatch_nonces.calls() == 1U);

    fixed_progress_source retry_progress(initial);
    construction_execution_authority_source retry_execution(session);
    unreachable_recovery_authority_source retry_recovery;
    const auto retried = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {retry_progress, retry_execution, retry_recovery},
        {&native_driver, nullptr, nullptr}, {run_store, nullptr});
    CHECK(retried.origin() ==
          pkgctl::transaction_run_launch_origin::resumed);
    CHECK(retried.starting_record().sequence() == 0U);
    CHECK(retried.run().progress().complete());
    CHECK(run_store.append_calls() == 4U);
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source refusing(114U, trace, true);
    head_derived_nonce_source dispatch_nonces(114U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(
        construction_session_without_inputs(
            transaction, temporary.path() / "launch-refused"));
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          refusing, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      refused = std::string(problem.what()) ==
          "injected run-nonce refusal";
    }
    CHECK(refused);
    CHECK(run_store.load_calls() == 0U);
    CHECK(run_store.append_calls() == 0U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
  }

  {
    auto foreign_run = pkgctl::transaction_run::begin(
        initial, pkgctl::transaction_dispatch_policy::make(2U, 1U));
    auto foreign_record = pkgctl::transaction_run_journal_record::admit(
        foreign_run, journal_nonce(115U));
    foreign_launch_head_store run_store(std::move(foreign_record));
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(116U, trace);
    head_derived_nonce_source dispatch_nonces(116U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(
        construction_session_without_inputs(
            transaction, temporary.path() / "launch-foreign"));
    unreachable_recovery_authority_source recovery_source;

    bool rejected = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_contract_violation;
    }
    CHECK(rejected);
    CHECK(run_store.load_calls() == 1U);
    CHECK(run_store.append_calls() == 0U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
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
    check_durable_restart_reconciliation();
    check_run_authority_rehydration();
    check_single_step_transaction_advancement();
    check_durable_transaction_run_admission();
    check_bounded_serial_transaction_drive();
    check_restart_safe_transaction_launch();
  }
  catch (const std::exception& value)
  {
    std::cerr << "unexpected exception: " << value.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
