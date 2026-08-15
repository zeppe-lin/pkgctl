// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"
#include "support/run_execute_support.h"

#include <pkgctl/check.h>

#include <pkgctl/check_codec.h>
#include <pkgctl/error.h>
#include <pkgctl/progression.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_advance.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_restart.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace construction_fixture;
namespace fs = std::filesystem;

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

char hexadecimal_offset(char seed, std::size_t offset)
{
  static constexpr std::string_view digits = "0123456789abcdef";
  const auto position = digits.find(seed);
  if (position == std::string_view::npos)
    throw std::runtime_error("fixture identity seed is not hexadecimal");
  return digits[(position + offset) % digits.size()];
}

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

template<typename Function>
bool rejects(pkgctl::error_code expected, Function&& function)
{
  try {
    function();
  } catch (const pkgctl::error& problem) {
    return problem.code() == expected;
  }
  return false;
}


class fixed_check_progress_source final
    : public pkgctl::transaction_progress_rehydration_source {
public:
  explicit fixed_check_progress_source(pkgctl::transaction_progress progress)
      : progress_(std::move(progress))
  {
  }

  pkgctl::transaction_progress rehydrate_progress(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++calls_;
    record_ = record.identity();
    return progress_;
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }

private:
  pkgctl::transaction_progress progress_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
};

class unreachable_check_recovery_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  pkgctl::construction_dispatch_recovery_authority construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction recovery request");
  }

  pkgctl::check_dispatch_recovery_authority check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check recovery request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery request");
  }
};

class tracing_check_driver final : public pkgctl::transaction_check_driver {
public:
  tracing_check_driver(
      pkgctl::transaction_check_driver& driver,
      std::vector<std::string>& trace)
      : driver_(driver), trace_(trace)
  {
  }

  pkgcheck_exec::check_execution_result execute_check(
      const pkgctl::transaction_check_session& session) override
  {
    trace_.push_back("check");
    return driver_.execute_check(session);
  }

private:
  pkgctl::transaction_check_driver& driver_;
  std::vector<std::string>& trace_;
};


class check_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source,
      public pkgctl::transaction_dispatch_session_source {
public:
  explicit check_execution_authority_source(
      pkgctl::transaction_check_session session)
      : session_(std::move(session))
  {
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction execution authority request");
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction session authority request");
  }

  pkgctl::transaction_check_session check(
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
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_progress& progress,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = record.identity();
    run_ = progress.identity();
    dispatch_ = dispatch.identity();
    return session_;
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
  pkgctl::transaction_check_session session_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> run_;
  std::optional<pkgctl::session_identity> dispatch_;
};

class check_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  explicit check_recovery_authority_source(pkgctl::transaction_check_result result)
      : authority_(std::move(result))
  {
  }

  explicit check_recovery_authority_source(pkgctl::transaction_check_session session)
      : authority_(std::move(session))
  {
  }

  pkgctl::construction_dispatch_recovery_authority construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction recovery authority request");
  }

  pkgctl::check_dispatch_recovery_authority check(
      const pkgctl::transaction_run_restart_checkpoint& checkpoint,
      const pkgctl::transaction_dispatch_restart_assessment& assessment,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = checkpoint.record().identity();
    assessment_ = assessment.dispatch();
    dispatch_ = dispatch.identity();
    return authority_;
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
  pkgctl::check_dispatch_recovery_authority authority_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> assessment_;
  std::optional<pkgctl::session_identity> dispatch_;
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

pkgexec::backend_capability_profile check_capabilities(char seed = '8')
{
  return pkgexec::backend_capability_profile::seal(
      pkgexec::backend_identity::from_sha256(std::string(64U, seed)),
      check_guarantees());
}


class check_recovery_context_source final
    : public pkgctl::transaction_dispatch_recovery_context_source {
public:
  explicit check_recovery_context_source(
      pkgctl::transaction_check_result result,
      std::optional<pkgexec::backend_capability_profile> backend = std::nullopt)
      : result_(std::move(result)), backend_(backend
            ? std::move(*backend)
            : result_.execution().execution().backend())
  {
  }

  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected construction recovery context request");
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    ++calls_;
    return {
        result_.session(), result_.execution().execution().request(), backend_,
    };
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery context request");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  pkgctl::transaction_check_result result_;
  pkgexec::backend_capability_profile backend_;
  std::size_t calls_ = 0U;
};

enum class check_backend_mode {
  pass,
  unavailable,
  program_failed,
};

class unreachable_operation_recovery_context_source final
    : public pkgctl::transaction_operation_recovery_authority_source {
public:
  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery context request");
  }
};


class check_backend final : public pkgexec::execution_backend {
public:
  explicit check_backend(check_backend_mode mode) : mode_(mode) {}

  pkgexec::backend_capability_profile capabilities() const override
  {
    return check_capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources&) override
  {
    if (request.purpose().kind() != pkgexec::execution_purpose_kind::check)
      throw std::runtime_error("check fixture received non-check request");

    auto profile = capabilities();
    if (mode_ == check_backend_mode::pass) {
      return pkgexec::execution_result::succeeded(
          request, std::move(profile), request.interpreter(),
          pkgexec::stream_capture::retained("check passed\n"),
          pkgexec::stream_capture::retained(""),
          request.required_guarantees(), "check fixture success");
    }

    if (mode_ == check_backend_mode::unavailable) {
      return pkgexec::execution_result::failed_before_start(
          request, std::move(profile),
          pkgexec::execution_failure_kind::interpreter_unavailable,
          {}, "check interpreter unavailable");
    }

    return pkgexec::execution_result::failed_after_start(
        request, std::move(profile), request.interpreter(),
        pkgexec::process_termination::exited(1),
        pkgexec::stream_capture::retained(""),
        pkgexec::stream_capture::retained("check failed\n"),
        request.required_guarantees(), pkgexec::cleanup_outcome::verified,
        pkgexec::execution_failure_kind::program_exited_nonzero,
        "check fixture failure");
  }

private:
  check_backend_mode mode_;
};

class prepared_temporary_check_backend final
    : public pkgexec::execution_backend {
public:
  pkgexec::backend_capability_profile capabilities() const override
  {
    return check_capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override
  {
    const auto slot = pkgexec::resource_slot::singleton(
        pkgexec::resource_role::private_temporary_root);
    const auto resource = request.resources().binding(slot).resource();
    const auto& temporary = resources.materialization(resource).host_path();
    if (!fs::is_directory(temporary) ||
        !fs::is_directory(temporary / "home") ||
        fs::exists(temporary / "stale"))
      throw std::runtime_error(
          "native check driver did not prepare a clean temporary resource");

    struct stat status{};
    if (::stat(temporary.c_str(), &status) != 0 ||
        (status.st_mode & 0777U) != 0700U ||
        status.st_uid != ::geteuid() || status.st_gid != ::getegid())
      throw std::runtime_error(
          "native check driver prepared incorrect temporary authority");

    observed_ = true;
    return pkgexec::execution_result::succeeded(
        request, capabilities(), request.interpreter(),
        pkgexec::stream_capture::retained("check passed\n"),
        pkgexec::stream_capture::retained(""),
        request.required_guarantees(), "prepared temporary fixture success");
  }

  [[nodiscard]] bool observed() const noexcept { return observed_; }

private:
  bool observed_ = false;
};

class throwing_check_backend final : public pkgexec::execution_backend {
public:
  pkgexec::backend_capability_profile capabilities() const override
  {
    return check_capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request&,
      const pkgexec::execution_resources&) override
  {
    throw std::runtime_error("backend escaped without execution evidence");
  }
};

class throwing_driver final : public pkgctl::transaction_check_driver {
public:
  pkgcheck_exec::check_execution_result execute_check(
      const pkgctl::transaction_check_session&) override
  {
    throw std::runtime_error("driver escaped without check evidence");
  }
};

class variant_build_backend final : public pkgexec::execution_backend {
public:
  explicit variant_build_backend(std::string marker)
      : marker_(std::move(marker))
  {
  }

  pkgexec::backend_capability_profile capabilities() const override
  {
    return construction_fixture::capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override
  {
    const auto output_slot = pkgexec::resource_slot::singleton(
        pkgexec::resource_role::package_output_root);
    const auto& binding = request.resources().binding(output_slot);
    const auto output =
        resources.materialization(binding.resource()).host_path();
    fs::create_directories(output / "usr/bin");
    test_support::write(output / "usr/bin/tool", "tool\n");
    if (::chmod((output / "usr/bin/tool").c_str(), 0755) != 0)
      throw std::runtime_error("cannot chmod variant build fixture");

    return pkgexec::execution_result::succeeded(
        request, capabilities(), request.interpreter(),
        pkgexec::stream_capture::retained(marker_),
        pkgexec::stream_capture::retained(""),
        capabilities().guarantees(), marker_);
  }

private:
  std::string marker_;
};

pkgctl::construction_session independent_construction_session(
    const pkgctl::transaction_session& transaction,
    const fs::path& root,
    const std::string& package);

struct ready_check_fixture final {
  pkgctl::transaction_session transaction;
  pkgctl::construction_result construction;
  pkgctl::transaction_progress progress;
};

struct ready_check_options final {
  bool include_target = true;
  bool include_unrelated_build = false;
  pkgexec::execution_backend* build_backend = nullptr;
  std::string version = "1.0";
  std::vector<std::string> check_dependencies;
};

ready_check_fixture make_ready_check(
    const fs::path& root,
    ready_check_options options = {})
{
  test_support::initialize_state(root / "state");
  pkgstate::posix::canonical_generation_store store(
      root / "state", test_support::binding());

  const std::string payload = "source payload\n";
  tool_source_options source_options;
  source_options.version = std::move(options.version);
  source_options.with_build_dependency = false;
  source_options.check_dependencies = options.check_dependencies;
  source_options.check_program = pkgsource::program(
      pkgsource::program_language::posix_shell,
      "printf 'checked\\n'\n");
  auto source = tool_source(sha256_text(payload), std::move(source_options));

  std::vector<pkgsource::source_snapshot> dependencies;
  dependencies.push_back(dependency_source());
  for (const auto& package : options.check_dependencies) {
    if (package != "dep")
      dependencies.push_back(package_source(package));
  }

  auto transaction = transaction_session(
      source, std::move(dependencies), store.read(), root / "state",
      options.include_target, options.include_unrelated_build, true);

  fixture_backend dependency_backend(backend_mode::succeed);
  pkgctl::native_construction_driver dependency_driver(dependency_backend);
  std::vector<pkgctl::construction_result> dependency_constructions;
  for (const auto& package : options.check_dependencies) {
    auto dependency_session = independent_construction_session(
        transaction, root / "dependencies" / package, package);
    dependency_constructions.push_back(pkgctl::execute_construction(
        dependency_session, dependency_driver));
  }

  auto session = construction_session_with_inputs(
      transaction, root / "tool", true);
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  fixture_backend default_backend(backend_mode::succeed);
  pkgexec::execution_backend& backend =
      options.build_backend == nullptr
          ? static_cast<pkgexec::execution_backend&>(default_backend)
          : *options.build_backend;
  pkgctl::native_construction_driver driver(backend);
  auto construction = pkgctl::execute_construction(session, driver);

  auto progress = pkgctl::transaction_progress::begin(transaction);
  for (auto& dependency : dependency_constructions)
    progress = pkgctl::advance_construction(
        std::move(progress), std::move(dependency));
  progress = pkgctl::advance_construction(
      std::move(progress), construction);
  return {
      std::move(transaction),
      std::move(construction),
      std::move(progress),
  };
}

using check_resources = pkgctl::transaction_check_resources;

const pkgbuild_exec::package_input_resource& concrete_input_resource(
    const pkgctl::construction_result& construction,
    const pkgbuild::build_input& expected)
{
  const pkgbuild_exec::package_input_resource* found = nullptr;
  for (const auto& resource : construction.session().package_inputs()) {
    if (resource.input != expected.identity())
      continue;
    if (found != nullptr)
      throw std::runtime_error(
          "construction fixture contains duplicate package input resources");
    found = &resource;
  }

  if (found == nullptr)
    throw std::runtime_error(
        "construction fixture lacks a package input resource");
  return *found;
}

void remove_retained_package_input(const fs::path& path)
{
  fs::permissions(
      path, fs::perms::owner_write, fs::perm_options::add);
  const auto removed = fs::remove_all(path);
  if (removed == 0U)
    throw std::runtime_error(
        "retained package input fixture was not removed");
}

std::vector<pkgcheck_exec::package_input_resource> check_input_resources(
    const pkgctl::construction_result& construction,
    const fs::path& root,
    char seed,
    const pkgctl::transaction_progress* progress)
{
  std::vector<pkgcheck_exec::package_input_resource> result;
  const auto inputs = construction.build().build().request().inputs().for_scope(
      pkgbuild::input_scope::check);
  result.reserve(inputs.size());

  for (std::size_t index = 0; index < inputs.size(); ++index) {
    const auto& input = inputs[index];
    const auto& concrete = concrete_input_resource(construction, input);
    if (progress != nullptr && input.selection().candidate() != nullptr) {
      const pkgctl::construction_result* authority = nullptr;
      for (const auto& candidate : progress->constructions()) {
        if (candidate.session().request().build().subject().identity() !=
            input.selection().identity())
          continue;
        if (authority != nullptr)
          throw std::runtime_error(
              "check fixture contains ambiguous candidate input construction");
        authority = &candidate;
      }
      if (authority == nullptr || !authority->build().image_authority())
        throw std::runtime_error(
            "check fixture lacks candidate input image authority");
      result.push_back({
          input.identity(),
          pkgexec::resource_identity::from_sha256(
              std::string(64U, hexadecimal_offset(seed, 4 + index))),
          root / "inputs" / input.identity().hex(),
      });
      continue;
    }
    result.push_back({
        input.identity(),
        pkgexec::resource_identity::from_sha256(
            std::string(64U, hexadecimal_offset(seed, 4 + index))),
        concrete.path,
    });
  }
  return result;
}


check_resources resources_for(
    const pkgctl::construction_result& construction,
    const fs::path& root,
    char seed = '1',
    const pkgctl::transaction_progress* progress = nullptr)
{
  const auto& artifact = construction.build().build().artifact();
  if (!artifact || !construction.build().image_authority())
    throw std::runtime_error(
        "check fixture construction lacks sealed image authority");

  fs::create_directories(root / "root-view");
  fs::create_directories(root / "source-tree");
  fs::create_directories(root / "package-tree");
  fs::create_directories(root / "temporary");

  return {
      {
          construction.session().request().source().identity(),
          pkgexec::resource_identity::from_sha256(
              std::string(64U, hexadecimal_offset(seed, 0))),
          root / "source-tree",
      },
      {
          artifact->identity(),
          pkgexec::resource_identity::from_sha256(
              std::string(64U, hexadecimal_offset(seed, 1))),
          root / "package-tree",
      },
      check_input_resources(construction, root, seed, progress),
      {
          pkgexec::root_view_identity::from_sha256(
              std::string(64U, hexadecimal_offset(seed, 2))),
          root / "root-view",
          root / "temporary",
      },
      {
          pkgexec::interpreter_identity::from_sha256(
              std::string(64U, hexadecimal_offset(seed, 3))),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      },
      pkgexec::resource_limits::make(),
  };
}

pkgctl::transaction_check_request make_check_request(
    const pkgctl::transaction_progress& progress,
    const pkgctl::transaction_session& transaction)
{
  return pkgctl::transaction_check_request::make(
      progress, check_node(transaction).identity());
}

pkgctl::transaction_check_session admit_check(
    const pkgctl::transaction_progress& progress,
    const pkgctl::transaction_session& transaction,
    check_resources resources)
{
  return pkgctl::transaction_check_session::admit(
      make_check_request(progress, transaction), std::move(resources));
}

void check_durable_session_codec()
{
  test_support::temporary_directory temporary;
  ready_check_options options;
  options.check_dependencies = {"dep"};
  auto fixture = make_ready_check(temporary.path() / "retained", options);
  const auto check_root = temporary.path() / "retained-check";
  auto resources = resources_for(
      fixture.construction, check_root, '5', &fixture.progress);
  resources.execution_identity.supplementary_groups = {
      static_cast<std::uint64_t>(::getegid()) + 1000U,
      static_cast<std::uint64_t>(::getegid()) + 2000U,
  };
  resources.limits = pkgexec::resource_limits::make(
      5000U, 64U * 1024U * 1024U, 32U * 1024U * 1024U, 64U, 16U);
  auto session = admit_check(
      fixture.progress, fixture.transaction, std::move(resources));
  CHECK(session.request().constructed_inputs().size() == 1U);
  const auto encoding = pkgctl::encode_check_session(session);

  const auto retained_source = session.execution_session().source().path;
  const auto retained_package = session.execution_session().package().path;
  const auto retained_root_view =
      session.execution_session().paths().root_view_path;
  const auto retained_temporary =
      session.execution_session().paths().temporary_root;
  std::vector<fs::path> retained_inputs;
  retained_inputs.reserve(session.execution_session().inputs().size());
  for (const auto& input : session.execution_session().inputs())
    retained_inputs.push_back(input.path);

  fs::remove_all(check_root);
  for (const auto& input : retained_inputs)
    remove_retained_package_input(input);
  CHECK(!fs::exists(retained_source));
  CHECK(!fs::exists(retained_package));
  CHECK(!fs::exists(retained_root_view));
  CHECK(!fs::exists(retained_temporary));
  for (const auto& input : retained_inputs)
    CHECK(!fs::exists(input));

  auto request = make_check_request(fixture.progress, fixture.transaction);
  auto decoded = pkgctl::decode_check_session(encoding, std::move(request));
  CHECK(decoded.identity() == session.identity());
  CHECK(decoded.execution_request() == session.execution_request());
  CHECK(decoded.execution_session().source().path == retained_source);
  CHECK(decoded.execution_session().package().path == retained_package);
  CHECK(decoded.execution_session().paths().root_view_path == retained_root_view);
  CHECK(decoded.execution_session().paths().temporary_root == retained_temporary);
  CHECK(decoded.execution_session().inputs().size() == retained_inputs.size());
  if (decoded.execution_session().inputs().size() == retained_inputs.size())
  {
    for (std::size_t index = 0U; index < retained_inputs.size(); ++index)
      CHECK(decoded.execution_session().inputs()[index].path ==
            retained_inputs[index]);
  }
  CHECK(decoded.execution_session().limits().identity() ==
        session.execution_session().limits().identity());
  CHECK(decoded.request().constructed_inputs().size() ==
        session.request().constructed_inputs().size());
  if (decoded.request().constructed_inputs().size() == 1U &&
      session.request().constructed_inputs().size() == 1U)
  {
    CHECK(decoded.request().constructed_inputs().front().input ==
          session.request().constructed_inputs().front().input);
    CHECK(decoded.request().constructed_inputs().front().construction.identity() ==
          session.request().constructed_inputs().front().construction.identity());
  }
  CHECK(pkgctl::encode_check_session(decoded) == encoding);

  {
    auto corrupt = encoding;
    corrupt.back() ^= 0x01U;
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_check_session(
          corrupt, make_check_request(fixture.progress, fixture.transaction));
    }
    catch (const pkgctl::check_codec_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::check_codec_error_code::corrupt_encoding;
    }
    CHECK(rejected);
  }

  {
    ready_check_options foreign_options;
    foreign_options.version = "2.0";
    auto foreign = make_ready_check(
        temporary.path() / "foreign", std::move(foreign_options));
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_check_session(
          encoding, make_check_request(foreign.progress, foreign.transaction));
    }
    catch (const pkgctl::check_codec_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::check_codec_error_code::authority_mismatch;
    }
    CHECK(rejected);
  }
}


pkgctl::construction_session independent_construction_session(
    const pkgctl::transaction_session& transaction,
    const fs::path& root,
    const std::string& package)
{
  const pkgtransaction::transaction_node* selected = nullptr;
  for (const auto& node : transaction.program().nodes()) {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == package) {
      selected = &node;
      break;
    }
  }
  if (selected == nullptr)
    throw std::runtime_error("independent build node is absent");

  auto request = pkgctl::construction_request::make(
      transaction, selected->identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(2, 0022, 1700000000)));
  pkgctl::construction_paths paths{
      root / "sources",
      root / "store",
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, '6')),
          root / "root-view",
          root / "session",
          root / "package",
          root / "artifact" / (package + ".tar"),
      },
  };
  fs::create_directories(paths.local_source_root);
  fs::create_directories(paths.build.root_view_path);
  return pkgctl::construction_session::admit(
      std::move(request), std::move(paths), {},
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, '7')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
}

class mismatched_driver final : public pkgctl::transaction_check_driver {
public:
  mismatched_driver(pkgcheck_exec::admitted_check_session session,
                    pkgexec::execution_backend& backend)
      : session_(std::move(session)), backend_(backend)
  {
  }

  pkgcheck_exec::check_execution_result execute_check(
      const pkgctl::transaction_check_session&) override
  {
    return pkgcheck_exec::execute(session_, backend_);
  }

private:
  pkgcheck_exec::admitted_check_session session_;
  pkgexec::execution_backend& backend_;
};

void check_successful_session_and_progression()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto resources = resources_for(
      fixture.construction, temporary.path() / "check");
  auto session = admit_check(fixture.progress, fixture.transaction, resources);
  auto repeated = admit_check(fixture.progress, fixture.transaction, resources);

  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto journal = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(1U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(1U));
  CHECK(reservation.dispatch.has_value());
  CHECK(reservation.dispatch &&
        reservation.dispatch->unit().kind() ==
            pkgctl::transaction_unit_kind::check);
  auto reserved_journal = journal.successor(reservation.run);
  auto started_run = pkgctl::start_check_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started_journal = reserved_journal.successor(started_run);
  const auto restart =
      pkgctl::transaction_run_restart_checkpoint::make(
          started_run.progress(), started_journal).assessment();
  CHECK(restart.active().size() == 1U);
  CHECK(restart.external_evidence_required());
  CHECK(restart.active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::recover_check);
  CHECK(restart.active().front().attempt_session() == session.identity());
  const auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
      fixture.progress, started_journal);
  CHECK(checkpoint.run().identity() == started_run.identity());

  CHECK(session.identity() == repeated.identity());
  CHECK(session.request().prepared_from_progress() == fixture.progress.identity());
  CHECK(session.request().transaction().identity() == fixture.transaction.identity());
  CHECK(session.request().construction().identity() == fixture.construction.identity());
  CHECK(session.request().check().check_node().identity() ==
        check_node(fixture.transaction).identity());
  CHECK(session.request().check().build().identity() ==
        fixture.construction.build().build().identity());
  CHECK(session.request().check().program().material() == "printf 'checked\\n'\n");
  CHECK(session.execution_session().request().identity() ==
        session.request().check().identity());

  const auto prepared = pkgcheck_exec::prepare(session.execution_session());
  CHECK(prepared.request.identity() == session.execution_request());
  CHECK(prepared.request.purpose().kind() ==
        pkgexec::execution_purpose_kind::check);

  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  auto repeated_result = pkgctl::execute_transaction_check(repeated, driver);
  CHECK(result.identity() == repeated_result.identity());
  CHECK(result.succeeded());
  CHECK(result.outcome() == pkgcheck::check_outcome::passed);
  CHECK(result.execution().check().request().identity() ==
        session.request().check().identity());

  auto completed_run = pkgctl::complete_check_dispatch(
      started_run, *reservation.dispatch, result);
  auto completed_journal = started_journal.successor(completed_run);
  CHECK(pkgctl::transaction_run_restart_checkpoint::make(
            completed_run.progress(), completed_journal).assessment().quiescent());
  const auto& progressed = completed_run.progress();
  CHECK(progressed.check(check_node(fixture.transaction).identity()) != nullptr);
  CHECK(progressed.check(check_node(fixture.transaction).identity())->identity() ==
        result.identity());
  CHECK(progressed.status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(progressed.status(install_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(progressed.checks().size() == 1U);
  CHECK(!progressed.failed());

  CHECK(rejects(pkgctl::error_code::invalid_progression, [&] {
    (void)pkgctl::advance_check(progressed, result);
  }));
}

void check_native_driver_prepares_temporary_resource()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto resources = resources_for(
      fixture.construction, temporary.path() / "prepared-temporary");
  const auto temporary_root = resources.paths.temporary_root;
  {
    std::ofstream stale(temporary_root / "stale");
    stale << "stale\n";
  }
  auto session = admit_check(
      fixture.progress, fixture.transaction, std::move(resources));

  prepared_temporary_check_backend backend;
  pkgctl::native_transaction_check_driver driver(backend);
  const auto result = pkgctl::execute_transaction_check(session, driver);

  CHECK(result.succeeded());
  CHECK(backend.observed());
  CHECK(!fs::exists(temporary_root / "stale"));
  CHECK(fs::is_directory(temporary_root / "home"));
}

void check_failure_blocks_target()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  const auto check_root = temporary.path() / "check";
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, check_root));

  check_backend backend(check_backend_mode::program_failed);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  CHECK(!result.succeeded());
  CHECK(result.execution().check().failure() ==
        pkgcheck::check_failure_kind::program_failed);

  auto progressed = pkgctl::advance_check(fixture.progress, result);
  CHECK(progressed.status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(progressed.status(install_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(progressed.failed());
  CHECK(progressed.ready_units().empty());
}

void check_unavailable_is_terminal_failure()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "check"));

  check_backend backend(check_backend_mode::unavailable);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  CHECK(result.execution().check().failure() ==
        pkgcheck::check_failure_kind::execution_unavailable);

  auto progressed = pkgctl::advance_check(fixture.progress, result);
  CHECK(progressed.status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(progressed.status(install_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::blocked);
}

void check_admission_requires_ready_check()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(
      sha256_text(payload), "1.0", false,
      pkgsource::program(pkgsource::program_language::posix_shell, "true\n"));
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true, false, true);
  auto construction_session = construction_session_without_inputs(
      transaction, temporary.path() / "tool");
  test_support::write(
      construction_session.paths().local_source_root / "payload", payload);
  fixture_backend build_backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(build_backend);
  auto construction = pkgctl::execute_construction(
      construction_session, construction_driver);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto resources = resources_for(construction, temporary.path() / "check");

  CHECK(rejects(pkgctl::error_code::invalid_check_request, [&] {
    (void)make_check_request(progress, transaction);
  }));

  progress = pkgctl::advance_construction(progress, construction);
  CHECK(rejects(pkgctl::error_code::invalid_check_request, [&] {
    (void)pkgctl::transaction_check_request::make(
        progress, build_node(transaction).identity());
  }));
}

void check_driver_contract_rejects_other_execution()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto expected = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "expected", '1'));
  auto other = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "other", 'a'));
  CHECK(expected.request().identity() == other.request().identity());
  CHECK(expected.execution_request() != other.execution_request());

  check_backend backend(check_backend_mode::pass);
  mismatched_driver driver(other.execution_session(), backend);
  CHECK(rejects(pkgctl::error_code::check_driver_contract_violation, [&] {
    (void)pkgctl::execute_transaction_check(expected, driver);
  }));
}

void check_native_and_custom_driver_exceptions_are_controller_errors()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "check"));

  throwing_check_backend backend;
  pkgctl::native_transaction_check_driver native_driver(backend);
  CHECK(rejects(pkgctl::error_code::check_driver_contract_violation, [&] {
    (void)pkgctl::execute_transaction_check(session, native_driver);
  }));

  throwing_driver custom_driver;
  CHECK(rejects(pkgctl::error_code::check_driver_contract_violation, [&] {
    (void)pkgctl::execute_transaction_check(session, custom_driver);
  }));
}

void check_cross_transaction_evidence_is_rejected()
{
  test_support::temporary_directory first_root;
  test_support::temporary_directory second_root;
  auto first = make_ready_check(first_root.path());
  ready_check_options second_options;
  second_options.version = "2.0";
  auto second = make_ready_check(second_root.path(), second_options);

  auto session = admit_check(
      first.progress, first.transaction,
      resources_for(first.construction, first_root.path() / "check"));
  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);

  CHECK(rejects(pkgctl::error_code::invalid_progression, [&] {
    (void)pkgctl::advance_check(second.progress, result);
  }));
}

void check_exact_construction_binding()
{
  test_support::temporary_directory temporary;
  variant_build_backend first_backend("first build\n");
  variant_build_backend second_backend("second build\n");
  ready_check_options first_options;
  first_options.build_backend = &first_backend;
  auto first = make_ready_check(
      temporary.path() / "first", first_options);

  auto second_session = construction_session_without_inputs(
      first.transaction, temporary.path() / "second" / "tool");
  test_support::write(
      second_session.paths().local_source_root / "payload", "source payload\n");
  pkgctl::native_construction_driver second_driver(second_backend);
  auto second_construction = pkgctl::execute_construction(
      second_session, second_driver);
  CHECK(first.construction.identity() != second_construction.identity());

  auto second_progress = pkgctl::advance_construction(
      pkgctl::transaction_progress::begin(first.transaction),
      second_construction);
  auto second_check = admit_check(
      second_progress, first.transaction,
      resources_for(second_construction, temporary.path() / "second-check"));
  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(second_check, driver);

  CHECK(rejects(pkgctl::error_code::invalid_progression, [&] {
    (void)pkgctl::advance_check(first.progress, result);
  }));
}

void check_unrelated_progress_does_not_stale_session()
{
  test_support::temporary_directory temporary;
  ready_check_options options;
  options.include_target = false;
  options.include_unrelated_build = true;
  auto fixture = make_ready_check(temporary.path(), options);
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "check"));

  auto dep_session = independent_construction_session(
      fixture.transaction, temporary.path() / "dep", "dep");
  fixture_backend build_backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(build_backend);
  auto dep_construction = pkgctl::execute_construction(
      dep_session, construction_driver);
  auto advanced = pkgctl::advance_construction(
      fixture.progress, dep_construction);
  CHECK(advanced.identity() != fixture.progress.identity());
  CHECK(advanced.status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::ready);

  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);
  auto completed = pkgctl::advance_check(advanced, result);
  CHECK(completed.status(check_node(fixture.transaction).identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(completed.complete());
}

void check_exact_input_resource_projection()
{
  test_support::temporary_directory temporary;
  ready_check_options options;
  options.check_dependencies = {"dep", "aux"};
  auto fixture = make_ready_check(temporary.path(), options);

  auto canonical_resources = resources_for(
      fixture.construction, temporary.path() / "canonical");
  CHECK(canonical_resources.inputs.size() == 2U);

  auto reversed_resources = canonical_resources;
  std::reverse(
      reversed_resources.inputs.begin(), reversed_resources.inputs.end());
  auto canonical = admit_check(
      fixture.progress, fixture.transaction, canonical_resources);
  auto reversed = admit_check(
      fixture.progress, fixture.transaction, reversed_resources);

  CHECK(canonical.identity() == reversed.identity());
  CHECK(canonical.execution_request() == reversed.execution_request());
  CHECK(canonical.execution_session().inputs().size() == 2U);
  const auto& expected_inputs =
      canonical.request().check().inputs().inputs();
  CHECK(expected_inputs.size() ==
        canonical.execution_session().inputs().size());
  for (std::size_t index = 0; index < expected_inputs.size(); ++index) {
    CHECK(canonical.execution_session().inputs()[index].input ==
          expected_inputs[index].identity());
  }

  const auto prepared = pkgcheck_exec::prepare(
      canonical.execution_session());
  std::size_t observed_check_inputs = 0;
  for (const auto& input : canonical.execution_session().inputs()) {
    const auto slot = pkgexec::resource_slot::named(
        pkgexec::resource_role::check_input_tree, input.input.hex());
    const auto& binding = prepared.request.resources().binding(slot);
    CHECK(binding.resource() == input.resource);
    CHECK(binding.access() == pkgexec::resource_access::read_only);
    CHECK(binding.mount_point().string() ==
          "/check/inputs/" + input.input.hex());
    CHECK(prepared.resources.materialization(input.resource).host_path() ==
          input.path);
    ++observed_check_inputs;
  }
  CHECK(observed_check_inputs == 2U);

  auto missing = canonical_resources;
  missing.inputs.pop_back();
  CHECK(rejects(pkgctl::error_code::invalid_check_session, [&] {
    (void)admit_check(
        fixture.progress, fixture.transaction, std::move(missing));
  }));

  auto duplicate = canonical_resources;
  duplicate.inputs.push_back(duplicate.inputs.front());
  CHECK(rejects(pkgctl::error_code::invalid_check_session, [&] {
    (void)admit_check(
        fixture.progress, fixture.transaction, std::move(duplicate));
  }));

  auto forged = canonical_resources;
  forged.inputs.front().input =
      source_identity<pkgbuild::build_input_identity>('f');
  CHECK(rejects(pkgctl::error_code::invalid_check_session, [&] {
    (void)admit_check(
        fixture.progress, fixture.transaction, std::move(forged));
  }));
}

void check_concrete_paths_participate_in_session_identity()
{
  test_support::temporary_directory temporary;
  ready_check_options options;
  options.include_target = false;
  auto fixture = make_ready_check(temporary.path(), options);
  auto first = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "first", '1'));
  auto second_resources = resources_for(
      fixture.construction, temporary.path() / "first", '1');
  second_resources.source.path = temporary.path() / "different-source-path";
  fs::create_directories(second_resources.source.path);
  auto second = admit_check(
      fixture.progress, fixture.transaction, std::move(second_resources));

  CHECK(first.request().identity() == second.request().identity());
  CHECK(first.execution_request() == second.execution_request());
  CHECK(first.identity() != second.identity());
}


void check_durable_dispatch_execution()
{
  const auto reserve_check = [](const ready_check_fixture& fixture,
                                std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        fixture.progress,
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::check)
      throw std::runtime_error("fixture did not reserve a check dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_tuple(
        std::move(reservation), std::move(reserved));
  };

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 41U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver native_driver(backend);
    tracing_check_driver driver(native_driver, trace);
    const auto completed = pkgctl::execute_check_dispatch_durable(
        reserved, reservation.run, *reservation.dispatch,
        session, driver, evidence_store, run_store);

    CHECK(trace == std::vector<std::string>(
        {"attempt-check", "run-1", "check", "evidence-check", "run-2"}));
    CHECK(completed.result.succeeded());
    CHECK(completed.evidence.result() == completed.result.identity());
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

    const auto encoding = pkgctl::encode_check_dispatch_evidence(
        completed.evidence);
    const auto decoded = pkgctl::decode_check_dispatch_evidence(encoding);
    CHECK(decoded.identity() == completed.evidence.identity());
    CHECK(completed.evidence.session_encoding() ==
          pkgctl::encode_check_session(session));
    CHECK(decoded.session_encoding() == completed.evidence.session_encoding());
    CHECK(completed.evidence.backend_encoding() ==
          pkgexec::encode_backend_capability_profile(
              completed.result.execution().execution().backend()));
    CHECK(decoded.backend_encoding() == completed.evidence.backend_encoding());
    CHECK(pkgexec::decode_backend_capability_profile(
              decoded.backend_encoding()) ==
          completed.result.execution().execution().backend());
    CHECK(decoded.encoding() == completed.evidence.encoding());
    CHECK(pkgctl::encode_check_dispatch_evidence(decoded) == encoding);

    const auto evidence_path = temporary.path() / "check-evidence";
    fs::create_directory(evidence_path);
    auto durable_store = pkgctl::posix_transaction_run_evidence_store::open(
        evidence_path.string());
    CHECK(durable_store.publish(completed.evidence).identity() ==
          completed.evidence.identity());
    const auto loaded = durable_store.load_check(
        completed.evidence.journal(), completed.evidence.dispatch(),
        completed.evidence.attempt_session());
    CHECK(loaded.has_value());
    CHECK(loaded && loaded->identity() == completed.evidence.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 40U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(
        trace, false, false, false, true);
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver native_driver(backend);
    tracing_check_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_check_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          session, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"attempt-check"}));
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 42U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 1U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver native_driver(backend);
    tracing_check_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_check_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          session, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"attempt-check", "run-1"}));
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 43U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 2U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver native_driver(backend);
    tracing_check_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_check_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          session, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-check", "run-1", "check", "evidence-check", "run-2"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::recover_check);
    }
    CHECK(!run_store.latest().dispatches().empty());
    if (!run_store.latest().dispatches().empty() &&
        run_store.latest().dispatches().front().attempt_session())
    {
      const auto retained = evidence_store.load_check(
          run_store.latest().journal(), reservation.dispatch->identity(),
          *run_store.latest().dispatches().front().attempt_session());
      CHECK(retained.has_value());
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 45U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(
        trace, false, true);
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver native_driver(backend);
    tracing_check_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_check_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          session, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-check", "run-1", "check", "evidence-check"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::recover_check);
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 44U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    throwing_driver escaped;
    tracing_check_driver driver(escaped, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_check_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          session, driver, evidence_store, run_store);
    }
    catch (const pkgctl::error& problem)
    {
      failed = problem.code() ==
          pkgctl::error_code::check_driver_contract_violation;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-check", "run-1", "check"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::recover_check);
    }
  }
}


void check_durable_restart_reconciliation()
{
  const auto reserve_check = [](const ready_check_fixture& fixture,
                                std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        fixture.progress,
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::check)
      throw std::runtime_error("fixture did not reserve a check dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_tuple(
        std::move(reservation), std::move(reserved));
  };

  {
    test_support::temporary_directory temporary;
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 51U);
    auto started_run = pkgctl::start_check_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);

    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver driver(backend);
    auto result = pkgctl::execute_transaction_check(session, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    const auto completed = pkgctl::reconcile_check_dispatch_durable(
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
    auto fixture = make_ready_check(temporary.path());
    auto resources = resources_for(
        fixture.construction, temporary.path() / "check");
    auto session = admit_check(
        fixture.progress, fixture.transaction, std::move(resources));
    auto [reservation, reserved] = reserve_check(fixture, 52U);
    auto started_run = pkgctl::start_check_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);

    auto foreign_resources = resources_for(
        fixture.construction, temporary.path() / "foreign-check", '9');
    auto foreign_session = admit_check(
        fixture.progress, fixture.transaction, std::move(foreign_resources));
    check_backend backend(check_backend_mode::pass);
    pkgctl::native_transaction_check_driver driver(backend);
    auto foreign_result = pkgctl::execute_transaction_check(
        foreign_session, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    bool refused = false;
    try
    {
      (void)pkgctl::reconcile_check_dispatch_durable(
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

} // namespace


void check_run_authority_rehydration()
{
  const auto reserve_check = [](const ready_check_fixture& fixture,
                                std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        fixture.progress,
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::check)
      throw std::runtime_error("fixture did not reserve a check dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_tuple(std::move(reservation), std::move(reserved));
  };

  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "check"));
  auto [reservation, reserved] = reserve_check(fixture, 61U);

  check_execution_authority_source execution_source(session);
  const auto execution =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch,
          execution_source);
  CHECK(execution_source.calls() == 1U);
  CHECK(execution_source.record() ==
        std::optional<pkgctl::session_identity>(reserved.identity()));
  CHECK(execution_source.run() ==
        std::optional<pkgctl::session_identity>(reservation.run.identity()));
  CHECK(execution_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(execution.kind() == pkgctl::transaction_unit_kind::check);
  CHECK(execution.construction() == nullptr);
  CHECK(execution.check() != nullptr);
  CHECK(execution.operation() == nullptr);
  if (execution.check())
    CHECK(execution.check()->identity() == session.identity());

  auto started_run = pkgctl::start_check_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);
  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);

  check_recovery_authority_source recovery_source(result);
  const auto recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, recovery_source);
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
        pkgctl::transaction_dispatch_restart_disposition::recover_check);
  CHECK(!recovery.releases_reserved());
  CHECK(recovery.construction() == nullptr);
  CHECK(recovery.check() != nullptr);
  CHECK(recovery.operation() == nullptr);
  if (recovery.check())
    CHECK(recovery.check()->identity() == result.identity());

  auto foreign_session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "foreign", 'a'));
  check_execution_authority_source alternate_execution(foreign_session);
  const auto alternate =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch,
          alternate_execution);
  CHECK(alternate.check() != nullptr);
  if (alternate.check())
  {
    CHECK(alternate.check()->identity() == foreign_session.identity());
    CHECK(alternate.check()->identity() != session.identity());
  }
  CHECK(alternate.identity() != execution.identity());

  auto foreign_result = pkgctl::execute_transaction_check(
      foreign_session, driver);
  check_recovery_authority_source foreign_recovery(foreign_result);
  bool refused = false;
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



void check_stored_check_recovery()
{
  test_support::temporary_directory temporary;
  ready_check_options options;
  options.check_dependencies = {"dep"};
  auto fixture = make_ready_check(temporary.path(), std::move(options));
  const auto check_root = temporary.path() / "check";
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, check_root, '1', &fixture.progress));
  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver driver(backend);
  auto result = pkgctl::execute_transaction_check(session, driver);

  auto run = pkgctl::transaction_run::begin(
      fixture.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(74U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(74U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  auto reserved = admitted.successor(reservation.run);
  auto started_run = pkgctl::start_check_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  const auto evidence_path = temporary.path() / "evidence";
  std::filesystem::create_directory(evidence_path);
  auto evidence = pkgctl::check_dispatch_evidence_record::admit(
      started, *reservation.dispatch, result);
  {
    auto writer = pkgctl::posix_transaction_run_evidence_store::open(
        evidence_path.string());
    CHECK(writer.publish(evidence).identity() == evidence.identity());
  }
  auto evidence_store = pkgctl::posix_transaction_run_evidence_store::open(
      evidence_path.string());

  auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
      started_run.progress(), started);

  const auto attempt_path = temporary.path() / "attempt-evidence";
  fs::create_directory(attempt_path);
  auto attempt_store = pkgctl::posix_transaction_run_evidence_store::open(
      attempt_path.string());
  const auto attempt = pkgctl::check_dispatch_attempt_record::admit(
      reserved, *reservation.dispatch, session);
  const auto attempt_encoding = pkgctl::encode_check_dispatch_attempt(attempt);
  const auto decoded_attempt =
      pkgctl::decode_check_dispatch_attempt(attempt_encoding);
  CHECK(decoded_attempt.identity() == attempt.identity());
  CHECK(decoded_attempt.session_encoding() == attempt.session_encoding());
  CHECK(pkgctl::encode_check_dispatch_attempt(decoded_attempt) ==
        attempt_encoding);
  CHECK(attempt_store.publish(attempt).identity() == attempt.identity());
  pkgctl::native_transaction_dispatch_recovery_context_source attempt_context;
  pkgctl::stored_transaction_dispatch_recovery_authority_source attempt_source(
      attempt_store, attempt_context);
  auto attempt_recovery = pkgctl::acquire_transaction_dispatch_recovery_authority(
      checkpoint, *reservation.dispatch, attempt_source);
  CHECK(attempt_recovery.check_retry() != nullptr);
  CHECK(attempt_recovery.check() == nullptr);
  if (attempt_recovery.check_retry())
  {
    CHECK(attempt_recovery.check_retry()->identity() == session.identity());
    CHECK(attempt_recovery.check_retry()->request().constructed_inputs().size() ==
          session.request().constructed_inputs().size());
    if (attempt_recovery.check_retry()->request().constructed_inputs().size() ==
            1U &&
        session.request().constructed_inputs().size() == 1U)
    {
      CHECK(attempt_recovery.check_retry()
                ->request()
                .constructed_inputs()
                .front()
                .input == session.request().constructed_inputs().front().input);
      CHECK(attempt_recovery.check_retry()
                ->request()
                .constructed_inputs()
                .front()
                .construction
                .identity() ==
            session.request()
                .constructed_inputs()
                .front()
                .construction
                .identity());
    }
    CHECK(pkgctl::encode_check_session(*attempt_recovery.check_retry()) ==
          pkgctl::encode_check_session(session));
  }

  check_recovery_context_source context(result);
  pkgctl::stored_transaction_dispatch_recovery_authority_source recovery_source(
      evidence_store, context);
  auto recovery = pkgctl::acquire_transaction_dispatch_recovery_authority(
      checkpoint, *reservation.dispatch, recovery_source);
  CHECK(context.calls() == 1U);
  CHECK(recovery.check() != nullptr);
  if (recovery.check())
  {
    CHECK(recovery.check()->identity() == result.identity());
    CHECK(recovery.check()->session().identity() == result.session().identity());
    CHECK(pkgcheck_exec::encode_check_execution_result(
              recovery.check()->execution()) ==
          pkgcheck_exec::encode_check_execution_result(result.execution()));
  }

  const auto empty_path = temporary.path() / "empty-evidence";
  std::filesystem::create_directory(empty_path);
  auto empty_store = pkgctl::posix_transaction_run_evidence_store::open(
      empty_path.string());
  check_recovery_context_source unused_context(result);
  pkgctl::stored_transaction_dispatch_recovery_authority_source missing_source(
      empty_store, unused_context);
  bool missing = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, missing_source);
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    missing = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::evidence_missing;
  }
  CHECK(missing);
  CHECK(unused_context.calls() == 0U);

  check_recovery_context_source foreign_context(
      result, check_capabilities('9'));
  pkgctl::stored_transaction_dispatch_recovery_authority_source foreign_source(
      evidence_store, foreign_context);
  bool mismatched = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, foreign_source);
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    mismatched = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::
            recovery_context_mismatch;
  }
  CHECK(mismatched);
  CHECK(foreign_context.calls() == 1U);

  const auto retained_session_encoding = evidence.session_encoding();
  const auto retained_source = session.execution_session().source().path;
  const auto retained_package = session.execution_session().package().path;
  const auto retained_root_view =
      session.execution_session().paths().root_view_path;
  const auto retained_temporary =
      session.execution_session().paths().temporary_root;
  std::vector<fs::path> retained_inputs;
  retained_inputs.reserve(session.execution_session().inputs().size());
  for (const auto& input : session.execution_session().inputs())
    retained_inputs.push_back(input.path);

  fs::remove_all(check_root);
  for (const auto& input : retained_inputs)
    remove_retained_package_input(input);
  CHECK(!fs::exists(retained_source));
  CHECK(!fs::exists(retained_package));
  CHECK(!fs::exists(retained_root_view));
  CHECK(!fs::exists(retained_temporary));
  for (const auto& input : retained_inputs)
    CHECK(!fs::exists(input));

  pkgctl::native_transaction_dispatch_recovery_context_source native_context;
  pkgctl::stored_transaction_dispatch_recovery_authority_source native_source(
      evidence_store, native_context);
  auto native_recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          checkpoint, *reservation.dispatch, native_source);
  CHECK(native_recovery.check() != nullptr);
  if (native_recovery.check())
  {
    CHECK(native_recovery.check()->identity() == result.identity());
    CHECK(native_recovery.check()->session().identity() == session.identity());
    CHECK(pkgctl::encode_check_session(native_recovery.check()->session()) ==
          retained_session_encoding);
  }
}

void check_single_step_check_advancement()
{
  test_support::temporary_directory temporary;
  auto fixture = make_ready_check(temporary.path());
  auto session = admit_check(
      fixture.progress, fixture.transaction,
      resources_for(fixture.construction, temporary.path() / "check-step"));
  check_backend backend(check_backend_mode::pass);
  pkgctl::native_transaction_check_driver native_driver(backend);

  const auto make_admitted = [&](std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        fixture.progress,
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  {
    auto [run, admitted] = make_admitted(71U);
    fixed_check_progress_source progress_source(run.progress());
    check_execution_authority_source execution_source(session);
    unreachable_check_recovery_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_check_driver driver(native_driver, trace);

    const auto advanced = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(71U),
        {progress_source, execution_source, recovery_source},
        {nullptr, &driver, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(advanced.disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_check);
    CHECK(advanced.durable_transition_committed());
    CHECK(advanced.dispatch().has_value());
    CHECK(advanced.construction() == nullptr);
    CHECK(advanced.check() != nullptr);
    CHECK(advanced.operation() == nullptr);
    CHECK(advanced.record().sequence() == 3U);
    CHECK(advanced.record().identity() == run_store.latest().identity());
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 1U);
    CHECK(trace == std::vector<std::string>({
        "run-1", "attempt-check", "run-2", "check", "evidence-check", "run-3"}));
  }

  {
    auto [run, admitted] = make_admitted(72U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(72U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("check recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_check_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);
    auto result = pkgctl::execute_transaction_check(session, native_driver);

    fixed_check_progress_source progress_source(started_run.progress());
    check_execution_authority_source execution_source(session);
    check_recovery_authority_source recovery_source(result);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    const auto reconciled = pkgctl::advance_transaction_run_once(
        started.journal(), dispatch_nonce(73U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(reconciled.disposition() ==
          pkgctl::transaction_run_advance_disposition::reconciled_check);
    CHECK(reconciled.check() != nullptr);
    CHECK(reconciled.check() &&
          reconciled.check()->identity() == result.identity());
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(74U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(74U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("check replay fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_check_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);

    fixed_check_progress_source progress_source(started_run.progress());
    check_execution_authority_source execution_source(session);
    check_recovery_authority_source recovery_source(session);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_check_driver driver(native_driver, trace);

    const auto replayed = pkgctl::advance_transaction_run_once(
        started.journal(), dispatch_nonce(75U),
        {progress_source, execution_source, recovery_source},
        {nullptr, &driver, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(replayed.disposition() ==
          pkgctl::transaction_run_advance_disposition::reconciled_check);
    CHECK(replayed.check() != nullptr);
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(replayed.record().sequence() == started.sequence() + 1U);
    CHECK(trace == std::vector<std::string>(
        {"check", "evidence-check", "run-1"}));
  }
}


int main()
{
  try {
    check_successful_session_and_progression();
    check_durable_session_codec();
    check_native_driver_prepares_temporary_resource();
    check_failure_blocks_target();
    check_unavailable_is_terminal_failure();
    check_admission_requires_ready_check();
    check_driver_contract_rejects_other_execution();
    check_native_and_custom_driver_exceptions_are_controller_errors();
    check_cross_transaction_evidence_is_rejected();
    check_exact_construction_binding();
    check_unrelated_progress_does_not_stale_session();
    check_exact_input_resource_projection();
    check_concrete_paths_participate_in_session_identity();
    check_durable_dispatch_execution();
    check_durable_restart_reconciliation();
    check_run_authority_rehydration();
    check_stored_check_recovery();
    check_single_step_check_advancement();
  } catch (const std::exception& problem) {
    std::cerr << "unexpected exception: " << problem.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
