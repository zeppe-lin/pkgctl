// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/controller.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/effect.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/preparation.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_locator.h>
#include <pkgctl/run_native.h>
#include <pkgctl/run_operation.h>
#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_store.h>
#include <pkgctl/target_observation.h>

#include <libpkgapply-posix/backend.h>
#include <libpkgapply-posix/journal_store.h>
#include <libpkgapply-posix/mutation_lease.h>
#include <libpkgimage/package_path.h>
#include <libpkgimage/libpkgimage.h>
#include <libpkgstate-apply/state_projection.h>
#include <libpkgreconcile-apply/adapter.h>
#include <libpkgreconcile-apply-posix/publication.h>
#include <libpkgreconcile-posix/inventory_store.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace construction_fixture;
namespace fs = std::filesystem;

int failures = 0;
#define CHECK(value)                                                         \
  do {                                                                       \
    if (!(value)) {                                                          \
      std::cerr << "CHECK failed: " #value "\n";                         \
      ++failures;                                                            \
    }                                                                        \
  } while (false)

std::string read_text(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read fixture path: " + path.string());
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

std::string pipeline_recipe(
    const std::string& name,
    std::string_view version,
    std::string_view source_digest,
    bool with_dependency,
    bool with_check)
{
  std::string requirements = "  {}\n";
  if (with_dependency)
    requirements = "  build:\n    - package: dep\n";

  std::string check;
  if (with_check) {
    check = "\n"
            "check:\n"
            "  language: posix-shell\n"
            "  script: |\n"
            "    " + name + "-check\n";
  }

  return "format: zeppe-lin.recipe/1\n"
         "\n"
         "package:\n"
         "  name: " + name + "\n"
         "  version: " + std::string(version) + "\n"
         "  release: 1\n"
         "  summary: " + name + " package\n"
         "  licenses:\n"
         "    - GPL-3.0-or-later\n"
         "\n"
         "requirements:\n" + requirements +
         "\n"
         "sources:\n"
         "  - path: files/source.txt\n"
         "    name: source.txt\n"
         "    sha256: " + std::string(source_digest) + "\n"
         "\n"
         "build:\n"
         "  language: posix-shell\n"
         "  script: |\n"
         "    " + name + "-build\n" + check +
         "\n"
         "architectures:\n"
         "  build:\n"
         "    - x86_64\n"
         "  target:\n"
         "    - x86_64\n";
}

void create_pipeline_collection(
    const fs::path& root,
    std::string_view tool_version = "1.0",
    std::string tool_source = "tool source v1\n")
{
  const std::string dep_source = "dependency source\n";
  test_support::write(
      root / "profiles.yml",
      "format: zeppe-lin.profiles/1\n"
      "\n"
      "profiles:\n"
      "  base:\n"
      "    members:\n"
      "      - package: tool\n");
  test_support::write(
      root / "dep" / "recipe.yml",
      pipeline_recipe("dep", "1.0", sha256_text(dep_source), false, false));
  test_support::write(root / "dep" / "files/source.txt", dep_source);
  test_support::write(
      root / "tool" / "recipe.yml",
      pipeline_recipe(
          "tool", tool_version, sha256_text(tool_source), true, true));
  test_support::write(root / "tool" / "files/source.txt", tool_source);
}

pkgctl::resolution_request pipeline_resolution_request(
    const fs::path& collection,
    const fs::path& state,
    pkgresolve::installed_preference preference =
        pkgresolve::installed_preference::retain_compatible)
{
  const auto base = test_support::resolution_request(
      collection, state, preference);
  auto goals = base.goals();
  goals.emplace_back(
      pkgsource::requirement_scope::check(),
      pkgsource::requirement_subject(pkgsource::package_reference("tool")),
      "<pipeline-check>");
  return pkgctl::resolution_request::make(
      base.catalog(), base.state(), base.architectures(), std::move(goals),
      base.policy());
}

pkgctl::resolution_request pipeline_removal_resolution_request(
    const fs::path& collection,
    const fs::path& state)
{
  const auto base = test_support::resolution_request(collection, state);
  std::vector<pkgresolve::resolution_goal> goals;
  goals.emplace_back(
      pkgsource::requirement_scope::build(),
      pkgsource::requirement_subject(pkgsource::package_reference("dep")),
      "<pipeline-removal-build>");
  return pkgctl::resolution_request::make(
      base.catalog(), base.state(), base.architectures(), std::move(goals),
      base.policy());
}

const pkgtransaction::transaction_node& node_for(
    const pkgctl::transaction_session& transaction,
    pkgtransaction::transaction_action_kind action,
    std::string_view package)
{
  const pkgtransaction::transaction_node* found = nullptr;
  for (const auto& node : transaction.program().nodes()) {
    if (node.action() != action || node.package().name() != package)
      continue;
    if (found != nullptr) {
      std::string message = "pipeline transaction has ambiguous requested node: ";
      message += pkgtransaction::to_string(action);
      message += ":";
      message += package;
      throw std::runtime_error(std::move(message));
    }
    found = &node;
  }
  if (found != nullptr)
    return *found;

  std::string message = "pipeline transaction lacks requested node: ";
  message += pkgtransaction::to_string(action);
  message += ":";
  message += package;
  message += "; actual nodes:";
  for (const auto& node : transaction.program().nodes()) {
    message += " ";
    message += pkgtransaction::to_string(node.action());
    message += ":";
    message += node.package().name();
  }
  throw std::runtime_error(std::move(message));
}

bool has_requirement_edge(
    const pkgctl::transaction_session& transaction,
    const pkgtransaction::transaction_node& before,
    const pkgtransaction::transaction_node& after,
    pkgsource::requirement_scope_kind scope)
{
  return std::any_of(
      transaction.program().edges().begin(), transaction.program().edges().end(),
      [&](const auto& edge) {
        return edge.kind() == pkgtransaction::transaction_edge_kind::requirement &&
               edge.before() == before.identity() &&
               edge.after() == after.identity() && edge.scope() &&
               edge.scope()->kind() == scope;
      });
}

bool has_phase_edge(
    const pkgctl::transaction_session& transaction,
    const pkgtransaction::transaction_node& before,
    const pkgtransaction::transaction_node& after,
    pkgtransaction::phase_order_kind order)
{
  return std::any_of(
      transaction.program().edges().begin(), transaction.program().edges().end(),
      [&](const auto& edge) {
        return edge.kind() == pkgtransaction::transaction_edge_kind::phase &&
               edge.before() == before.identity() &&
               edge.after() == after.identity() &&
               edge.phase_order() == order;
      });
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

pkgctl::effect_attempt_nonce effect_nonce(std::uint8_t marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
}

class directory_fd final {
public:
  explicit directory_fd(const fs::path& path)
      : fd_(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC))
  {
    if (fd_ < 0)
      throw std::runtime_error("cannot open pipeline directory: " +
                               path.string());
  }

  directory_fd(const directory_fd&) = delete;
  directory_fd& operator=(const directory_fd&) = delete;
  directory_fd(directory_fd&&) = delete;
  directory_fd& operator=(directory_fd&&) = delete;

  ~directory_fd()
  {
    if (fd_ >= 0)
      (void)::close(fd_);
  }

  [[nodiscard]] int get() const noexcept { return fd_; }

private:
  int fd_ = -1;
};

class refusing_installed_package_source final
    : public pkgctl::retained_installed_package_tree_source {
public:
  pkgctl::retained_installed_package_tree locate(
      const pkgstate::installed_package&) override
  {
    throw std::runtime_error(
        "pipeline unexpectedly requested an installed package tree");
  }
};

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
          root / "check-temporary",
          pkgexec::root_view_identity::from_sha256(std::string(64U, '8')),
          root / "root-view",
      },
      {
          pkgbuild::build_policy::make(
              pkgbuild::environment_policy::hermetic(2, 0022, 1700000000)),
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

enum class pipeline_execution_fault {
  none,
  dependency_build,
  package_check,
};

class pipeline_backend final : public pkgexec::execution_backend {
public:
  explicit pipeline_backend(
      pipeline_execution_fault fault = pipeline_execution_fault::none)
      : fault_(fault)
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
    const bool build = request.purpose().kind() ==
        pkgexec::execution_purpose_kind::build;
    const bool check = request.purpose().kind() ==
        pkgexec::execution_purpose_kind::check;
    if (!build && !check)
      throw std::runtime_error(
          "pipeline backend received unsupported execution purpose");

    const auto source_slot = pkgexec::resource_slot::named(
        pkgexec::resource_role::source_tree,
        build ? "sources" : "checked-source");
    const auto& source_binding = request.resources().binding(source_slot);
    const fs::path source_root =
        resources.materialization(source_binding.resource()).host_path();
    const std::string source = read_text(source_root / "source.txt");

    if (check) {
      if (request.program().material() != "tool-check\n" ||
          (source != "tool source v1\n" && source != "tool source v2\n"))
        throw std::runtime_error("pipeline check authority changed");
      const auto package_slot = pkgexec::resource_slot::named(
          pkgexec::resource_role::build_input_tree, "checked-package");
      const auto& package_binding = request.resources().binding(package_slot);
      const fs::path package =
          resources.materialization(package_binding.resource()).host_path();
      const std::string expected_tool = source == "tool source v1\n"
          ? "constructed tool v1\n"
          : "constructed tool v2\n";
      if (read_text(package / "usr/bin/tool") != expected_tool)
        throw std::runtime_error(
            "pipeline check did not receive the constructed package tree");
      if (fault_ == pipeline_execution_fault::package_check) {
        return pkgexec::execution_result::failed_after_start(
            request, capabilities(), request.interpreter(),
            pkgexec::process_termination::exited(1),
            pkgexec::stream_capture::retained(""),
            pkgexec::stream_capture::retained("pipeline check failed\n"),
            request.required_guarantees(), pkgexec::cleanup_outcome::verified,
            pkgexec::execution_failure_kind::program_exited_nonzero,
            "injected pipeline check failure");
      }
      return pkgexec::execution_result::succeeded(
          request, capabilities(), request.interpreter(),
          pkgexec::stream_capture::retained("pipeline check stdout\n"),
          pkgexec::stream_capture::retained("pipeline check stderr\n"),
          capabilities().guarantees(), "pipeline check success");
    }

    const auto output_slot = pkgexec::resource_slot::singleton(
        pkgexec::resource_role::package_output_root);
    const auto& output_binding = request.resources().binding(output_slot);
    const fs::path output =
        resources.materialization(output_binding.resource()).host_path();

    const auto program = request.program().material();
    if (program == "dep-build\n") {
      if (source != "dependency source\n")
        throw std::runtime_error("dependency source materialization changed");
      if (fault_ == pipeline_execution_fault::dependency_build) {
        return pkgexec::execution_result::failed_before_start(
            request, capabilities(),
            pkgexec::execution_failure_kind::backend_unsupported, {},
            "injected pipeline dependency build failure");
      }
      fs::create_directories(output / "usr/lib");
      test_support::write(output / "usr/lib/dep.marker", "dependency package\n");
    } else if (program == "tool-build\n") {
      if (source != "tool source v1\n" && source != "tool source v2\n")
        throw std::runtime_error("tool source materialization changed");
      const auto input_slot = pkgexec::resource_slot::named(
          pkgexec::resource_role::build_input_tree, "dep");
      const auto& input_binding = request.resources().binding(input_slot);
      const fs::path dependency =
          resources.materialization(input_binding.resource()).host_path();
      if (read_text(dependency / "usr/lib/dep.marker") != "dependency package\n")
        throw std::runtime_error("tool did not receive predecessor package tree");
      fs::create_directories(output / "usr/bin");
      fs::create_directories(output / "etc");
      const bool version_two = source == "tool source v2\n";
      test_support::write(
          output / "usr/bin/tool",
          version_two ? "constructed tool v2\n" : "constructed tool v1\n");
      test_support::write(
          output / "etc/tool.conf",
          version_two ? "default config v2\n" : "default config v1\n");
      if (::chmod((output / "usr/bin/tool").c_str(), 0755) != 0)
        throw std::runtime_error("cannot chmod pipeline tool payload");
    } else {
      throw std::runtime_error("pipeline backend received unknown build program");
    }

    return pkgexec::execution_result::succeeded(
        request, capabilities(), request.interpreter(),
        pkgexec::stream_capture::retained("pipeline stdout\n"),
        pkgexec::stream_capture::retained("pipeline stderr\n"),
        capabilities().guarantees(), "pipeline build success");
  }

private:
  pipeline_execution_fault fault_;
};

pkgctl::construction_result execute_construction_reservation(
    pkgctl::transaction_run& run,
    pkgctl::transaction_run_journal_record& record,
    pkgctl::native_transaction_dispatch_session_source& locator,
    pkgctl::native_construction_driver& driver,
    pkgctl::transaction_dispatch_result reservation,
    std::string_view expected_package,
    bool require_success = true)
{
  if (!reservation.dispatch)
    throw std::runtime_error("pipeline has no ready construction dispatch");
  const auto dispatch = *reservation.dispatch;
  const auto* node = reservation.run.progress().transaction().program().find(
      dispatch.unit().primary_node());
  if (dispatch.unit().kind() != pkgctl::transaction_unit_kind::construction ||
      node == nullptr || node->package().name() != expected_package) {
    std::string message = "pipeline expected construction dispatch for ";
    message += expected_package;
    message += ", received ";
    if (node == nullptr)
      message += "<missing-node>";
    else {
      message += pkgtransaction::to_string(node->action());
      message += ":";
      message += node->package().name();
    }
    throw std::runtime_error(std::move(message));
  }

  auto reserved_record = record.successor(reservation.run);
  auto session = locator.construction(
      reserved_record, reservation.run.progress(), dispatch);
  auto started = pkgctl::start_construction_dispatch(
      reservation.run, dispatch, session);
  auto started_record = reserved_record.successor(started);
  auto construction = pkgctl::execute_construction(session, driver);
  if (require_success && !construction.succeeded())
    throw std::runtime_error("pipeline construction failed");
  auto completed = pkgctl::complete_construction_dispatch(
      started, dispatch, construction);
  record = started_record.successor(completed);
  run = std::move(completed);
  return construction;
}

pkgctl::construction_result execute_reserved_construction(
    pkgctl::transaction_run& run,
    pkgctl::transaction_run_journal_record& record,
    pkgctl::native_transaction_dispatch_session_source& locator,
    pkgctl::native_construction_driver& driver,
    std::uint8_t nonce,
    std::string_view expected_package,
    bool require_success = true)
{
  return execute_construction_reservation(
      run, record, locator, driver,
      pkgctl::reserve_next(run, dispatch_nonce(nonce)), expected_package,
      require_success);
}

pkgctl::transaction_check_result execute_reserved_check(
    pkgctl::transaction_run& run,
    pkgctl::transaction_run_journal_record& record,
    pkgctl::native_transaction_dispatch_session_source& locator,
    pipeline_backend& backend,
    std::uint8_t nonce,
    std::string_view expected_package,
    bool require_success = true)
{
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(nonce));
  if (!reservation.dispatch)
    throw std::runtime_error("pipeline has no ready check dispatch");
  const auto dispatch = *reservation.dispatch;
  const auto* node = reservation.run.progress().transaction().program().find(
      dispatch.unit().primary_node());
  if (dispatch.unit().kind() != pkgctl::transaction_unit_kind::check ||
      node == nullptr || node->package().name() != expected_package)
    throw std::runtime_error("pipeline check ordering changed");

  auto reserved_record = record.successor(reservation.run);
  auto session = locator.check(
      reserved_record, reservation.run.progress(), dispatch);
  auto started = pkgctl::start_check_dispatch(
      reservation.run, dispatch, session);
  auto started_record = reserved_record.successor(started);
  pkgctl::native_transaction_check_driver driver(backend);
  auto check = pkgctl::execute_transaction_check(session, driver);
  if (require_success && !check.succeeded())
    throw std::runtime_error("pipeline package check failed");
  auto completed = pkgctl::complete_check_dispatch(
      started, dispatch, check);
  record = started_record.successor(completed);
  run = std::move(completed);
  return check;
}

struct application_environment final {
  fs::path target_root;
  fs::path application_journal_root;
  fs::path lock_root;
  fs::path rejected_store_root;
  fs::path effect_journal_root;
  std::unique_ptr<pkgapply::posix::application_posix_backend> backend;
  std::unique_ptr<pkgapply::posix::target_mutation_lease> lease;
};

application_environment prepare_application_environment(
    const fs::path& root,
    const pkgapply::application_target_context& target)
{
  const fs::path target_root = root / "target";
  const fs::path journal = root / "application-journal";
  const fs::path checkpoint = root / "application-checkpoint";
  const fs::path payload = root / "incoming-payload";
  const fs::path capture = root / "old-object-capture";
  const fs::path rejected = root / "rejected-object";
  const fs::path completed = root / "completed-evidence";
  const fs::path locks = root / "target-locks";
  const fs::path effect_journal = root / "controller-effect-journal";

  for (const auto& path : {target_root, journal, checkpoint, payload, capture,
                           rejected, completed, locks, effect_journal})
    fs::create_directories(path);

  directory_fd target_fd(target_root);
  directory_fd journal_fd(journal);
  directory_fd checkpoint_fd(checkpoint);
  directory_fd payload_fd(payload);
  directory_fd capture_fd(capture);
  directory_fd rejected_fd(rejected);
  directory_fd completed_fd(completed);
  directory_fd lock_fd(locks);

  auto backend = pkgapply::posix::application_posix_backend::from_directory_fds(
      target, target_fd.get(), journal_fd.get(), checkpoint_fd.get(),
      payload_fd.get(), capture_fd.get(), rejected_fd.get(),
      completed_fd.get());
  auto lease = pkgapply::posix::target_mutation_lease::acquire(
      target, lock_fd.get());
  return {
      target_root, journal, locks, rejected, effect_journal,
      std::move(backend), std::move(lease)};
}


void reacquire_application_lease(
    application_environment& application,
    const pkgapply::application_target_context& target)
{
  application.lease.reset();
  directory_fd lock_fd(application.lock_root);
  application.lease = pkgapply::posix::target_mutation_lease::acquire(
      target, lock_fd.get());
}

class pipeline_run_store final : public pkgctl::transaction_run_journal_store {
public:
  explicit pipeline_run_store(pkgctl::transaction_run_journal_record latest)
      : latest_(std::move(latest))
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    if (latest_.journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    if (record.journal() != latest_.journal())
      throw std::runtime_error("pipeline run store received foreign journal");
    if (record.identity() == latest_.identity())
      return latest_;
    record.validate_successor_of(latest_);
    latest_ = record;
    return latest_;
  }

private:
  pkgctl::transaction_run_journal_record latest_;
};

pkgctl::transaction_run_journal_record reopen_run_head(
    const pipeline_run_store& store,
    const pkgctl::session_identity& journal)
{
  const auto latest = store.load_latest(journal);
  if (!latest)
    throw std::runtime_error("pipeline restart lacks durable run head");
  return *latest;
}

class interrupting_effect_store final : public pkgctl::effect_journal_store {
public:
  interrupting_effect_store(
      pkgctl::effect_journal_store& delegate,
      pkgctl::effect_attempt_stage interrupt_stage)
      : delegate_(delegate), interrupt_stage_(interrupt_stage)
  {
  }

  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity& attempt) const override
  {
    return delegate_.load_latest(attempt);
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record& record) override
  {
    if (!interrupted_ && record.stage() == interrupt_stage_) {
      interrupted_ = true;
      throw pkgctl::effect_journal_error(
          pkgctl::effect_journal_error_code::store_write_failed,
          "injected pipeline effect-journal interruption");
    }
    return delegate_.append(record);
  }

  bool interrupted() const noexcept { return interrupted_; }

private:
  pkgctl::effect_journal_store& delegate_;
  pkgctl::effect_attempt_stage interrupt_stage_;
  bool interrupted_ = false;
};

class recording_effect_body_sink final
    : public pkgctl::transaction_effect_body_sink {
public:
  void retain_lifecycle(
      const pkgapply_exec::lifecycle_execution_result&) override
  {
    ++lifecycle_count_;
  }

  void retain_application(
      const pkgapply::package_application_request&,
      const pkgapply::application_receipt& receipt) override
  {
    application_ = receipt;
  }

  void retain_publication_request(
      const pkgstate::state_publication_request& request) override
  {
    publication_request_ = request;
  }

  void retain_publication_receipt(
      const pkgstate::state_publication_request&,
      const pkgstate::state_publication_receipt& receipt) override
  {
    publication_receipt_ = receipt;
  }

  std::size_t lifecycle_count() const noexcept { return lifecycle_count_; }
  const std::optional<pkgapply::application_receipt>& application() const noexcept
  {
    return application_;
  }
  const std::optional<pkgstate::state_publication_request>&
  publication_request() const noexcept
  {
    return publication_request_;
  }
  const std::optional<pkgstate::state_publication_receipt>&
  publication_receipt() const noexcept
  {
    return publication_receipt_;
  }

private:
  std::size_t lifecycle_count_ = 0U;
  std::optional<pkgapply::application_receipt> application_;
  std::optional<pkgstate::state_publication_request> publication_request_;
  std::optional<pkgstate::state_publication_receipt> publication_receipt_;
};

class counting_effect_driver final : public pkgctl::transaction_effect_driver {
public:
  explicit counting_effect_driver(pkgctl::transaction_effect_driver& delegate)
      : delegate_(delegate)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return delegate_.lease();
  }

  const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept override
  {
    return delegate_.state_projection();
  }

  pkgapply_exec::lifecycle_execution_result execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override
  {
    ++lifecycle_calls_;
    return delegate_.execute_lifecycle(session);
  }

  pkgapply::application_receipt apply_application(
      const pkgapply::package_application_request& request) override
  {
    ++application_calls_;
    return delegate_.apply_application(request);
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    ++publication_calls_;
    return delegate_.publish_state(request);
  }

  pkgapply::application_receipt resume_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_journal_record& journal) override
  {
    ++application_resume_calls_;
    return delegate_.resume_application(request, journal);
  }

  std::size_t lifecycle_calls() const noexcept { return lifecycle_calls_; }
  std::size_t application_calls() const noexcept { return application_calls_; }
  std::size_t application_resume_calls() const noexcept
  {
    return application_resume_calls_;
  }
  std::size_t publication_calls() const noexcept { return publication_calls_; }

private:
  pkgctl::transaction_effect_driver& delegate_;
  std::size_t lifecycle_calls_ = 0U;
  std::size_t application_calls_ = 0U;
  std::size_t application_resume_calls_ = 0U;
  std::size_t publication_calls_ = 0U;
};

class counting_publication_driver final
    : public pkgctl::transaction_effect_publication_driver {
public:
  explicit counting_publication_driver(
      pkgctl::transaction_effect_publication_driver& delegate)
      : delegate_(delegate)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return delegate_.lease();
  }

  pkgstate::snapshot read_state() const override
  {
    ++read_calls_;
    return delegate_.read_state();
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    ++publication_calls_;
    return delegate_.publish_state(request);
  }

  std::size_t read_calls() const noexcept { return read_calls_; }
  std::size_t publication_calls() const noexcept { return publication_calls_; }

private:
  pkgctl::transaction_effect_publication_driver& delegate_;
  mutable std::size_t read_calls_ = 0U;
  std::size_t publication_calls_ = 0U;
};

const pkgctl::transaction_dispatch_restart_assessment&
require_single_operation_restart(
    const pkgctl::transaction_run_restart_checkpoint& checkpoint,
    const pkgctl::transaction_dispatch& dispatch)
{
  if (checkpoint.assessment().active().size() != 1U)
    throw std::runtime_error("pipeline restart does not have one active dispatch");
  const auto& assessment = checkpoint.assessment().active().front();
  if (assessment.dispatch() != dispatch.identity() ||
      assessment.disposition() !=
          pkgctl::transaction_dispatch_restart_disposition::inspect_effect_journal ||
      !assessment.effect_attempt())
    throw std::runtime_error("pipeline restart authority changed");
  return assessment;
}

std::vector<pkgplan::package_path> pipeline_operation_paths()
{
  std::vector<pkgplan::package_path> paths;
  for (const char* path : {"etc", "etc/tool.conf", "usr", "usr/bin", "usr/bin/tool"})
    paths.push_back(pkgplan::package_path::parse(path));
  return paths;
}

pkgplan::package_policy_snapshot protected_config_policy()
{
  const auto protected_path = pkgplan::normalized_path_policy(
      pkgplan::incoming_path_policy::retain(
          pkgplan::rejected_object_policy::stage,
          pkgplan::retained_active_ownership_policy::do_not_claim_operated_package),
      pkgplan::obsolete_path_policy::remove(),
      pkgplan::shared_ownership_policy::forbid,
      pkgplan::directory_cleanup_policy::remove_if_empty);
  return pkgplan::package_policy_snapshot(
      plan_identity<pkgplan::policy_snapshot_identity>(55),
      pkgplan::normalized_path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(),
          pkgplan::shared_ownership_policy::forbid,
          pkgplan::directory_cleanup_policy::remove_if_empty),
      {pkgplan::path_policy_override(
          pkgplan::package_path::parse("etc/tool.conf"), protected_path)});
}

const pkgplan::upgrade_path_decision& upgrade_decision(
    const pkgplan::upgrade_plan& plan,
    const pkgplan::package_path& path)
{
  for (const auto& decision : plan.paths())
    if (decision.path() == path)
      return decision;
  throw std::runtime_error("pipeline upgrade lacks expected path decision");
}

void check_package_pipeline()
{
  test_support::temporary_directory temporary;
  const fs::path root = temporary.path();
  const fs::path collection = root / "collection";
  const fs::path state = root / "state";
  create_pipeline_collection(collection);
  test_support::initialize_state(state);

  const auto catalog = pkgctl::acquire_catalog(
      test_support::catalog_request(collection));
  CHECK(catalog.catalog().candidates().size() == 2U);

  auto resolution_request = pipeline_resolution_request(collection, state);
  const auto resolution = pkgctl::resolve_packages(resolution_request);
  CHECK(resolution.resolution().selections().size() >= 2U);

  auto transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(std::move(resolution_request)));
  const auto& dep_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "dep");
  const auto& tool_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "tool");
  const auto& tool_check = node_for(
      transaction, pkgtransaction::transaction_action_kind::check, "tool");
  const auto& tool_install = node_for(
      transaction, pkgtransaction::transaction_action_kind::install, "tool");
  CHECK(dep_build.environment() == pkgresolve::resolution_environment::build);
  CHECK(tool_build.environment() == pkgresolve::resolution_environment::target);
  CHECK(has_requirement_edge(
      transaction, dep_build, tool_build,
      pkgsource::requirement_scope_kind::build));
  CHECK(has_phase_edge(
      transaction, tool_build, tool_check,
      pkgtransaction::phase_order_kind::build_before_check));
  CHECK(has_phase_edge(
      transaction, tool_check, tool_install,
      pkgtransaction::phase_order_kind::check_before_target));

  auto progress = pkgctl::transaction_progress::begin(transaction);
  CHECK(progress.status(dep_build.identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(progress.status(tool_build.identity()) ==
        pkgctl::transaction_node_status::pending);

  auto run = pkgctl::transaction_run::begin(
      std::move(progress), pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(1U));
  refusing_installed_package_source installed_packages;
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(root / "runtime"), installed_packages);
  pipeline_backend backend;
  pkgctl::native_construction_driver construction_driver(backend);

  auto dependency = execute_reserved_construction(
      run, record, locator, construction_driver, 1U, "dep");
  CHECK(fs::is_regular_file(
      dependency.session().paths().build.package_output_root /
      "usr/lib/dep.marker"));
  CHECK(run.progress().status(tool_build.identity()) ==
        pkgctl::transaction_node_status::ready);

  auto tool = execute_reserved_construction(
      run, record, locator, construction_driver, 2U, "tool");
  CHECK(tool.session().package_inputs().size() == 1U);
  CHECK(tool.session().package_inputs().front().path ==
        dependency.session().paths().build.package_output_root);
  CHECK(fs::is_regular_file(tool.session().paths().build.artifact_path));
  CHECK(tool.build().image_authority().has_value());
  if (tool.build().image_authority()) {
    const auto& image = tool.build().image_authority()->image().image();
    CHECK(image.find(pkgimage::package_path::parse("usr/bin/tool")) != nullptr);
  }
  CHECK(run.progress().status(tool_check.identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::pending);

  const auto checked = execute_reserved_check(
      run, record, locator, backend, 3U, "tool");
  CHECK(checked.succeeded());
  CHECK(run.progress().status(tool_check.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::ready);

  auto operation_reservation = pkgctl::reserve_next(run, dispatch_nonce(4U));
  CHECK(operation_reservation.dispatch.has_value());
  if (!operation_reservation.dispatch)
    return;
  const auto operation_dispatch = *operation_reservation.dispatch;
  const auto* operation_node =
      operation_reservation.run.progress().transaction().program().find(
          operation_dispatch.unit().primary_node());
  CHECK(operation_dispatch.unit().kind() ==
        pkgctl::transaction_unit_kind::operation);
  CHECK(operation_node != nullptr);
  if (operation_node != nullptr)
    CHECK(operation_node->identity() == tool_install.identity());
  auto operation_reserved_record = record.successor(operation_reservation.run);

  pkgstate::posix::canonical_generation_store store(state, test_support::binding());
  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(52);
  const auto target =
      application_target(store.read().target_binding(), target_system);
  auto application = prepare_application_environment(root / "application", target);
  auto install_observer = pkgapply::posix::application_target_observer::open(
      application.target_root.string());
  auto install_observations = pkgctl::observe_native_target_paths(
      operation_reserved_record, operation_reservation.run.progress(),
      operation_dispatch, target_system, install_observer,
      pipeline_operation_paths());
  auto preparation_request = pkgctl::operation_preparation_request::install(
      operation_reservation.run.progress(), tool_install.identity(), tool,
      target, execution_control(), std::move(install_observations),
      plan_identity<pkgplan::runtime_dependency_closure_identity>(53),
      package_policy(), pkgctl::lifecycle_order::make({}, {}),
      pkgstate::installation_reason::explicit_request());
  pkgctl::native_operation_preparation_driver preparation_driver;
  const auto prepared = pkgctl::prepare_operation(
      std::move(preparation_request), preparation_driver);

  CHECK(prepared.prepared());
  CHECK(prepared.artifact().has_value());
  CHECK(prepared.incoming().has_value());
  CHECK(prepared.plan().has_value());
  CHECK(prepared.application().has_value());
  CHECK(prepared.effect().has_value());
  if (prepared.plan())
    CHECK(prepared.plan()->kind() == pkgplan::operation_kind::install);
  if (prepared.application() && prepared.plan())
    CHECK(prepared.application()->plan() == prepared.plan()->identity());
  if (prepared.effect())
    CHECK(prepared.effect()->action_node() == tool_install.identity());

  if (!prepared.application() || !prepared.effect() || !prepared.incoming())
    throw std::runtime_error("pipeline operation preparation is incomplete");

  auto effect_session = pkgctl::effectful_operation_session::admit(
      *prepared.effect(), {}, {});
  auto projected = pkgstate::apply_adapter::read_application_state(
      *prepared.application(), *application.lease, store);

  pkgimage::libarchive_backend archive_backend;
  auto archives = pkgctl::explicit_transaction_effect_archive_source::make(
      archive_backend,
      {{prepared.incoming()->identity(),
        tool.session().paths().build.artifact_path}});
  auto archive = pkgctl::acquire_transaction_effect_archive(
      archives, *prepared.application());
  CHECK(archive != nullptr);

  auto effect_store = pkgctl::posix_effect_journal_store::open(
      application.effect_journal_root.string());
  pipeline_run_store install_run_store(operation_reserved_record);
  recording_effect_body_sink install_bodies;
  {
    pkgctl::native_transaction_effect_driver native_effect_driver(
        projected.projection(), *application.lease, *application.backend,
        archive.get(), backend, store);
    counting_effect_driver effect_driver(native_effect_driver);
    pkgctl::native_transaction_effect_publication_driver state_observer(
        *application.lease, store);
    interrupting_effect_store interrupted_effect_store(
        effect_store, pkgctl::effect_attempt_stage::publication_terminal);

    bool interrupted = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          operation_reserved_record, operation_reservation.run,
          operation_dispatch, effect_session, effect_nonce(4U), effect_driver,
          state_observer, interrupted_effect_store, install_run_store,
          &install_bodies);
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      interrupted = problem.code() ==
          pkgctl::effect_journal_error_code::store_write_failed;
    }
    CHECK(interrupted);
    CHECK(interrupted_effect_store.interrupted());
    CHECK(effect_driver.application_calls() == 1U);
    CHECK(effect_driver.application_resume_calls() == 0U);
    CHECK(effect_driver.publication_calls() == 1U);
  }

  const auto installed_before_restart = store.read();
  const auto* installed_before_restart_tool =
      installed_before_restart.find_package("tool");
  CHECK(installed_before_restart_tool != nullptr);
  CHECK(read_text(application.target_root / "usr/bin/tool") ==
        "constructed tool v1\n");
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "default config v1\n");

  effect_store = pkgctl::posix_effect_journal_store::open(
      application.effect_journal_root.string());
  auto install_restart = pkgctl::transaction_run_restart_checkpoint::make(
      operation_reservation.run.progress(), reopen_run_head(
          install_run_store, operation_reserved_record.journal()));
  const auto& install_restart_assessment = require_single_operation_restart(
      install_restart, operation_dispatch);
  const auto install_effect_record = effect_store.load_latest(
      *install_restart_assessment.effect_attempt());
  CHECK(install_effect_record.has_value());
  if (!install_effect_record)
    throw std::runtime_error("pipeline install restart lacks effect journal");
  CHECK(install_effect_record->stage() ==
        pkgctl::effect_attempt_stage::publication_intent);
  CHECK(install_bodies.lifecycle_count() == 0U);
  CHECK(install_bodies.application().has_value());
  CHECK(install_bodies.publication_request().has_value());
  CHECK(install_bodies.publication_receipt().has_value());
  if (!install_bodies.application() || !install_bodies.publication_request())
    throw std::runtime_error(
        "pipeline install restart lacks subordinate durable bodies");

  auto install_effect_checkpoint = pkgctl::effect_restart_checkpoint::make(
      effect_session, *install_effect_record, {}, *install_bodies.application(),
      {}, *install_bodies.publication_request(), std::nullopt);
  CHECK(pkgctl::assess_effect_restart(*install_effect_record).disposition() ==
        pkgctl::effect_restart_disposition::reconcile_publication);

  reacquire_application_lease(application, target);
  pkgctl::native_transaction_effect_publication_driver native_publication(
      *application.lease, store);
  counting_publication_driver publication(native_publication);
  auto install_recovered = pkgctl::reconcile_operation_dispatch_durable(
      std::move(install_restart), operation_dispatch,
      std::move(install_effect_checkpoint), nullptr, nullptr, &publication,
      effect_store, install_run_store, &install_bodies);
  CHECK(install_recovered.run_advanced);
  CHECK(install_recovered.result.has_value());
  CHECK(install_recovered.result && install_recovered.result->succeeded());
  CHECK(install_recovered.disposition ==
        pkgctl::effect_restart_disposition::terminal);
  CHECK(publication.read_calls() >= 1U);
  CHECK(publication.publication_calls() == 0U);
  CHECK(install_recovered.effect_record.stage() ==
        pkgctl::effect_attempt_stage::terminal);

  const auto installed = store.read();
  const auto* installed_tool = installed.find_package("tool");
  CHECK(installed_tool != nullptr);
  if (installed_tool != nullptr) {
    CHECK(installed_tool->release().name() == "tool");
    CHECK(installed_tool->find(pkgstate::package_path::parse("usr/bin/tool")) !=
          nullptr);
  }
  CHECK(read_text(application.target_root / "usr/bin/tool") ==
        "constructed tool v1\n");
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "default config v1\n");

  record = install_recovered.record;
  run = std::move(install_recovered.run);
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(run.progress().complete());
  CHECK(record.complete());

  test_support::write(application.target_root / "etc/tool.conf", "local config\n");
  create_pipeline_collection(collection, "2.0", "tool source v2\n");

  auto upgrade_resolution_request = pipeline_resolution_request(
      collection, state, pkgresolve::installed_preference::prefer_catalog);
  const auto upgrade_resolution = pkgctl::resolve_packages(
      upgrade_resolution_request);
  CHECK(upgrade_resolution.resolution().selections().size() >= 2U);
  auto upgrade_transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(std::move(upgrade_resolution_request)));
  const auto& upgrade_dep_build = node_for(
      upgrade_transaction, pkgtransaction::transaction_action_kind::build, "dep");
  const auto& upgrade_tool_build = node_for(
      upgrade_transaction, pkgtransaction::transaction_action_kind::build, "tool");
  const auto& upgrade_tool_check = node_for(
      upgrade_transaction, pkgtransaction::transaction_action_kind::check, "tool");
  const auto& tool_upgrade = node_for(
      upgrade_transaction, pkgtransaction::transaction_action_kind::upgrade,
      "tool");
  CHECK(upgrade_dep_build.environment() ==
        pkgresolve::resolution_environment::build);
  CHECK(upgrade_tool_build.environment() ==
        pkgresolve::resolution_environment::target);
  CHECK(has_requirement_edge(
      upgrade_transaction, upgrade_dep_build, upgrade_tool_build,
      pkgsource::requirement_scope_kind::build));
  CHECK(has_phase_edge(
      upgrade_transaction, upgrade_tool_build, upgrade_tool_check,
      pkgtransaction::phase_order_kind::build_before_check));
  CHECK(has_phase_edge(
      upgrade_transaction, upgrade_tool_check, tool_upgrade,
      pkgtransaction::phase_order_kind::check_before_target));

  auto upgrade_progress = pkgctl::transaction_progress::begin(upgrade_transaction);
  CHECK(upgrade_progress.status(upgrade_dep_build.identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(upgrade_progress.status(tool_upgrade.identity()) ==
        pkgctl::transaction_node_status::pending);
  auto upgrade_run = pkgctl::transaction_run::begin(
      std::move(upgrade_progress),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto upgrade_record = pkgctl::transaction_run_journal_record::admit(
      upgrade_run, journal_nonce(20U));
  pkgctl::native_transaction_dispatch_session_source upgrade_locator(
      configuration(root / "runtime-upgrade"), installed_packages);

  auto upgrade_dependency = execute_reserved_construction(
      upgrade_run, upgrade_record, upgrade_locator, construction_driver,
      21U, "dep");
  CHECK(fs::is_regular_file(
      upgrade_dependency.session().paths().build.package_output_root /
      "usr/lib/dep.marker"));
  CHECK(upgrade_run.progress().status(upgrade_tool_build.identity()) ==
        pkgctl::transaction_node_status::ready);

  auto upgraded_tool = execute_reserved_construction(
      upgrade_run, upgrade_record, upgrade_locator, construction_driver,
      22U, "tool");
  CHECK(read_text(
      upgraded_tool.session().paths().build.package_output_root /
      "etc/tool.conf") == "default config v2\n");
  CHECK(upgrade_run.progress().status(upgrade_tool_check.identity()) ==
        pkgctl::transaction_node_status::ready);

  const auto upgrade_checked = execute_reserved_check(
      upgrade_run, upgrade_record, upgrade_locator, backend, 23U, "tool");
  CHECK(upgrade_checked.succeeded());
  CHECK(upgrade_run.progress().status(tool_upgrade.identity()) ==
        pkgctl::transaction_node_status::ready);

  auto upgrade_reservation = pkgctl::reserve_next(
      upgrade_run, dispatch_nonce(24U));
  CHECK(upgrade_reservation.dispatch.has_value());
  if (!upgrade_reservation.dispatch)
    return;
  const auto upgrade_dispatch = *upgrade_reservation.dispatch;
  const auto* upgrade_node =
      upgrade_reservation.run.progress().transaction().program().find(
          upgrade_dispatch.unit().primary_node());
  CHECK(upgrade_dispatch.unit().kind() ==
        pkgctl::transaction_unit_kind::operation);
  CHECK(upgrade_node != nullptr);
  if (upgrade_node != nullptr)
    CHECK(upgrade_node->identity() == tool_upgrade.identity());
  auto upgrade_reserved_record = upgrade_record.successor(
      upgrade_reservation.run);

  auto upgrade_observer = pkgapply::posix::application_target_observer::open(
      application.target_root.string());
  auto upgrade_observations = pkgctl::observe_native_target_paths(
      upgrade_reserved_record, upgrade_reservation.run.progress(),
      upgrade_dispatch, target_system, upgrade_observer,
      pipeline_operation_paths());
  auto upgrade_preparation_request =
      pkgctl::operation_preparation_request::upgrade(
          upgrade_reservation.run.progress(), tool_upgrade.identity(),
          upgraded_tool, target, execution_control(),
          std::move(upgrade_observations),
          plan_identity<pkgplan::runtime_dependency_closure_identity>(54),
          protected_config_policy(), pkgctl::lifecycle_order::make({}, {}));
  const auto prepared_upgrade = pkgctl::prepare_operation(
      std::move(upgrade_preparation_request), preparation_driver);
  CHECK(prepared_upgrade.prepared());
  CHECK(prepared_upgrade.plan().has_value());
  CHECK(prepared_upgrade.application().has_value());
  CHECK(prepared_upgrade.effect().has_value());
  CHECK(prepared_upgrade.incoming().has_value());
  if (!prepared_upgrade.plan() || !prepared_upgrade.application() ||
      !prepared_upgrade.effect() || !prepared_upgrade.incoming())
    throw std::runtime_error("pipeline upgrade preparation is incomplete");
  CHECK(prepared_upgrade.plan()->kind() == pkgplan::operation_kind::upgrade);
  const auto* upgrade_plan = prepared_upgrade.plan()->upgrade();
  CHECK(upgrade_plan != nullptr);
  if (upgrade_plan == nullptr)
    return;
  const auto config_path = pkgplan::package_path::parse("etc/tool.conf");
  const auto& config_decision = upgrade_decision(*upgrade_plan, config_path);
  CHECK(config_decision.active() ==
        pkgplan::planned_active_outcome::retain_observed);
  CHECK(config_decision.rejected() ==
        pkgplan::planned_rejected_outcome::stage_incoming);
  CHECK(config_decision.rejected_object().has_value());
  CHECK(config_decision.rejected_object() &&
        config_decision.rejected_object()->reason() ==
            pkgplan::rejected_object_reason::upgrade_incoming_protected);
  CHECK(!config_decision.ownership().incoming_package_owns_after());

  auto upgrade_effect_session = pkgctl::effectful_operation_session::admit(
      *prepared_upgrade.effect(), {}, {});
  auto upgrade_projected = pkgstate::apply_adapter::read_application_state(
      *prepared_upgrade.application(), *application.lease, store);
  auto upgrade_archives = pkgctl::explicit_transaction_effect_archive_source::make(
      archive_backend,
      {{prepared_upgrade.incoming()->identity(),
        upgraded_tool.session().paths().build.artifact_path}});
  auto upgrade_archive = pkgctl::acquire_transaction_effect_archive(
      upgrade_archives, *prepared_upgrade.application());
  CHECK(upgrade_archive != nullptr);

  pipeline_run_store upgrade_run_store(upgrade_reserved_record);
  recording_effect_body_sink upgrade_bodies;
  {
    pkgctl::native_transaction_effect_driver native_upgrade_driver(
        upgrade_projected.projection(), *application.lease,
        *application.backend, upgrade_archive.get(), backend, store);
    counting_effect_driver upgrade_driver(native_upgrade_driver);
    pkgctl::native_transaction_effect_publication_driver upgrade_state_observer(
        *application.lease, store);
    interrupting_effect_store interrupted_effect_store(
        effect_store, pkgctl::effect_attempt_stage::application_terminal);

    bool interrupted = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          upgrade_reserved_record, upgrade_reservation.run, upgrade_dispatch,
          upgrade_effect_session, effect_nonce(24U), upgrade_driver,
          upgrade_state_observer, interrupted_effect_store, upgrade_run_store,
          &upgrade_bodies);
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      interrupted = problem.code() ==
          pkgctl::effect_journal_error_code::store_write_failed;
    }
    CHECK(interrupted);
    CHECK(interrupted_effect_store.interrupted());
    CHECK(upgrade_driver.application_calls() == 1U);
    CHECK(upgrade_driver.application_resume_calls() == 0U);
    CHECK(upgrade_driver.publication_calls() == 0U);
  }

  CHECK(upgrade_bodies.application().has_value());
  CHECK(!upgrade_bodies.publication_request().has_value());
  CHECK(read_text(application.target_root / "usr/bin/tool") ==
        "constructed tool v2\n");
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "local config\n");
  const auto state_before_upgrade_restart = store.read();
  const auto* state_before_upgrade_restart_tool =
      state_before_upgrade_restart.find_package("tool");
  CHECK(state_before_upgrade_restart_tool != nullptr);
  if (state_before_upgrade_restart_tool != nullptr)
    CHECK(state_before_upgrade_restart_tool->release().version() == "1.0");

  effect_store = pkgctl::posix_effect_journal_store::open(
      application.effect_journal_root.string());
  auto upgrade_restart = pkgctl::transaction_run_restart_checkpoint::make(
      upgrade_reservation.run.progress(), reopen_run_head(
          upgrade_run_store, upgrade_reserved_record.journal()));
  const auto& upgrade_restart_assessment = require_single_operation_restart(
      upgrade_restart, upgrade_dispatch);
  const auto upgrade_effect_record = effect_store.load_latest(
      *upgrade_restart_assessment.effect_attempt());
  CHECK(upgrade_effect_record.has_value());
  if (!upgrade_effect_record)
    throw std::runtime_error("pipeline upgrade restart lacks effect journal");
  CHECK(upgrade_effect_record->stage() ==
        pkgctl::effect_attempt_stage::application_intent);
  CHECK(pkgctl::assess_effect_restart(*upgrade_effect_record).disposition() ==
        pkgctl::effect_restart_disposition::resume_application);

  auto application_journal_store =
      pkgapply::posix::application_journal_store::open(
          application.application_journal_root.string());
  const auto upgrade_application_journal =
      application_journal_store.load_active(
          prepared_upgrade.application()->identity());
  CHECK(upgrade_application_journal.has_value());
  if (!upgrade_application_journal)
    throw std::runtime_error(
        "pipeline upgrade restart lacks application journal");

  auto upgrade_effect_checkpoint = pkgctl::effect_restart_checkpoint::make(
      upgrade_effect_session, *upgrade_effect_record, {}, std::nullopt, {},
      std::nullopt, std::nullopt, *upgrade_application_journal);

  reacquire_application_lease(application, target);
  auto resumed_upgrade_projection = pkgstate::apply_adapter::read_application_state(
      *prepared_upgrade.application(), *application.lease, store);
  pkgctl::native_transaction_effect_driver native_resumed_upgrade_driver(
      resumed_upgrade_projection.projection(), *application.lease,
      *application.backend, upgrade_archive.get(), backend, store);
  counting_effect_driver resumed_upgrade_driver(native_resumed_upgrade_driver);
  pkgctl::native_transaction_effect_publication_driver
      resumed_upgrade_state_observer(*application.lease, store);
  auto upgrade_recovered = pkgctl::reconcile_operation_dispatch_durable(
      std::move(upgrade_restart), upgrade_dispatch,
      std::move(upgrade_effect_checkpoint), &resumed_upgrade_driver,
      &resumed_upgrade_state_observer, nullptr, effect_store,
      upgrade_run_store, &upgrade_bodies);
  CHECK(upgrade_recovered.run_advanced);
  CHECK(upgrade_recovered.result.has_value());
  CHECK(upgrade_recovered.result && upgrade_recovered.result->succeeded());
  CHECK(resumed_upgrade_driver.application_calls() == 0U);
  CHECK(resumed_upgrade_driver.application_resume_calls() == 1U);
  CHECK(resumed_upgrade_driver.publication_calls() == 1U);
  CHECK(upgrade_recovered.effect_record.stage() ==
        pkgctl::effect_attempt_stage::terminal);

  if (!upgrade_recovered.result)
    throw std::runtime_error("pipeline upgrade restart produced no result");
  const auto& upgrade_effect = *upgrade_recovered.result;
  CHECK(upgrade_effect.application().has_value());
  CHECK(read_text(application.target_root / "usr/bin/tool") ==
        "constructed tool v2\n");
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "local config\n");

  const auto upgraded_state = store.read();
  const auto* upgraded_installed_tool = upgraded_state.find_package("tool");
  CHECK(upgraded_installed_tool != nullptr);
  if (upgraded_installed_tool != nullptr) {
    CHECK(upgraded_installed_tool->release().version() == "2.0");
    CHECK(upgraded_installed_tool->find(
              pkgstate::package_path::parse("usr/bin/tool")) != nullptr);
    CHECK(upgraded_installed_tool->find(
              pkgstate::package_path::parse("etc/tool.conf")) == nullptr);
  }

  if (!upgrade_effect.application() ||
      !upgrade_effect.application()->completed_evidence())
    throw std::runtime_error(
        "pipeline upgrade lacks completed application evidence");
  const auto& upgrade_completed =
      *upgrade_effect.application()->completed_evidence();
  const auto rejected_consequence = std::find_if(
      upgrade_completed.paths().begin(), upgrade_completed.paths().end(),
      [&](const auto& consequence) {
        return consequence.path() == config_path;
      });
  CHECK(rejected_consequence != upgrade_completed.paths().end());
  if (rejected_consequence != upgrade_completed.paths().end()) {
    CHECK(rejected_consequence->rejected_status() ==
          pkgapply::application_effect_status::completed);
    CHECK(rejected_consequence->rejected_object().has_value());
  }

  upgrade_record = upgrade_recovered.record;
  upgrade_run = std::move(upgrade_recovered.run);
  CHECK(upgrade_run.progress().status(tool_upgrade.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(upgrade_run.progress().complete());
  CHECK(upgrade_record.complete());

  const auto reconciliation_projection =
      pkgreconcile::apply_adapter::project_completed_application(
          target, upgrade_completed);
  CHECK(reconciliation_projection.pending().size() == 1U);
  if (reconciliation_projection.pending().empty())
    throw std::runtime_error("pipeline upgrade produced no reconciliation work");
  const auto pending_config = reconciliation_projection.pending().front();
  CHECK(pending_config.path() == fs::path("etc/tool.conf"));
  CHECK(pending_config.side() == pkgreconcile::rejected_object_side::incoming);

  const fs::path reconciliation_root = root / "reconciliation";
  const auto rejected_store =
      pkgapply::posix::application_rejected_object_store::open(
          application.rejected_store_root.string());
  {
    pkgreconcile::posix::inventory_generation_store reconciliation_store(
        reconciliation_root, reconciliation_projection.target());
    const auto publication =
        pkgreconcile::apply_posix::publish_verified_projection(
            reconciliation_projection, target.rejected_store(),
            rejected_store, reconciliation_store);
    CHECK(publication.published() == 1U);
    CHECK(publication.already_pending() == 0U);
    CHECK(publication.suppressed_resolved() == 0U);
    CHECK(publication.changed());

    const auto duplicate_publication =
        pkgreconcile::apply_posix::publish_verified_projection(
            reconciliation_projection, target.rejected_store(),
            rejected_store, reconciliation_store);
    CHECK(duplicate_publication.published() == 0U);
    CHECK(duplicate_publication.already_pending() == 1U);
    CHECK(duplicate_publication.suppressed_resolved() == 0U);
    CHECK(!duplicate_publication.changed());
  }
  {
    auto reconciliation_store =
        pkgreconcile::posix::inventory_generation_store::open_existing(
            reconciliation_root, reconciliation_projection.target());
    const auto inventory = reconciliation_store.read();
    CHECK(inventory.size() == 1U);
    const auto* record_value = inventory.find(pending_config);
    CHECK(record_value != nullptr);
    if (record_value != nullptr)
      CHECK(record_value->status() ==
            pkgreconcile::reconciliation_record_status::pending);
    CHECK(reconciliation_store.resolve(pending_config) ==
          pkgreconcile::posix::resolution_outcome::resolved);
  }
  {
    auto reconciliation_store =
        pkgreconcile::posix::inventory_generation_store::open_existing(
            reconciliation_root, reconciliation_projection.target());
    const auto inventory = reconciliation_store.read();
    const auto* record_value = inventory.find(pending_config);
    CHECK(record_value != nullptr);
    if (record_value != nullptr)
      CHECK(record_value->status() ==
            pkgreconcile::reconciliation_record_status::resolved);
    const auto suppressed =
        pkgreconcile::apply_posix::publish_verified_projection(
            reconciliation_projection, target.rejected_store(),
            rejected_store, reconciliation_store);
    CHECK(suppressed.published() == 0U);
    CHECK(suppressed.already_pending() == 0U);
    CHECK(suppressed.suppressed_resolved() == 1U);
    CHECK(!suppressed.changed());
  }

  auto removal_resolution_request =
      pipeline_removal_resolution_request(collection, state);
  const auto removal_resolution = pkgctl::resolve_packages(
      removal_resolution_request);
  CHECK(!removal_resolution.resolution().selections().empty());
  auto removal_transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(
          std::move(removal_resolution_request),
          pkgtransaction::convergence_policy::converge_exact()));
  const auto& removal_dep_build = node_for(
      removal_transaction, pkgtransaction::transaction_action_kind::build,
      "dep");
  const auto& tool_remove = node_for(
      removal_transaction, pkgtransaction::transaction_action_kind::remove,
      "tool");

  auto removal_progress = pkgctl::transaction_progress::begin(
      removal_transaction);
  CHECK(removal_progress.status(removal_dep_build.identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(removal_progress.status(tool_remove.identity()) ==
        pkgctl::transaction_node_status::ready);
  auto removal_run = pkgctl::transaction_run::begin(
      std::move(removal_progress),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto removal_record = pkgctl::transaction_run_journal_record::admit(
      removal_run, journal_nonce(30U));
  pkgctl::native_transaction_dispatch_session_source removal_locator(
      configuration(root / "runtime-removal"), installed_packages);

  std::uint8_t removal_dispatch_marker = 31U;
  auto removal_reservation = pkgctl::reserve_next(
      removal_run, dispatch_nonce(removal_dispatch_marker++));
  if (!removal_reservation.dispatch)
    throw std::runtime_error("pipeline removal has no ready dispatch");
  const auto* first_removal_node =
      removal_reservation.run.progress().transaction().program().find(
          removal_reservation.dispatch->unit().primary_node());
  if (removal_reservation.dispatch->unit().kind() ==
      pkgctl::transaction_unit_kind::construction) {
    if (first_removal_node == nullptr ||
        first_removal_node->identity() != removal_dep_build.identity())
      throw std::runtime_error(
          "pipeline removal selected an unexpected construction unit");
    const auto removal_dependency = execute_construction_reservation(
        removal_run, removal_record, removal_locator, construction_driver,
        std::move(removal_reservation), "dep");
    CHECK(removal_dependency.succeeded());
    removal_reservation = pkgctl::reserve_next(
        removal_run, dispatch_nonce(removal_dispatch_marker++));
    if (!removal_reservation.dispatch)
      throw std::runtime_error(
          "pipeline removal has no operation after construction");
  }

  const auto removal_dispatch = *removal_reservation.dispatch;
  const auto* removal_node =
      removal_reservation.run.progress().transaction().program().find(
          removal_dispatch.unit().primary_node());
  CHECK(removal_dispatch.unit().kind() ==
        pkgctl::transaction_unit_kind::operation);
  CHECK(removal_node != nullptr);
  if (removal_node != nullptr)
    CHECK(removal_node->identity() == tool_remove.identity());
  auto removal_reserved_record = removal_record.successor(
      removal_reservation.run);

  auto removal_observer = pkgapply::posix::application_target_observer::open(
      application.target_root.string());
  auto removal_observations = pkgctl::observe_native_target_paths(
      removal_reserved_record, removal_reservation.run.progress(),
      removal_dispatch, target_system, removal_observer,
      pipeline_operation_paths());
  auto removal_preparation_request =
      pkgctl::operation_preparation_request::remove(
          removal_reservation.run.progress(), tool_remove.identity(), target,
          execution_control(), std::move(removal_observations),
          package_policy(), pkgctl::lifecycle_order::make({}, {}));
  const auto prepared_removal = pkgctl::prepare_operation(
      std::move(removal_preparation_request), preparation_driver);
  CHECK(prepared_removal.prepared());
  CHECK(prepared_removal.plan().has_value());
  CHECK(prepared_removal.application().has_value());
  CHECK(prepared_removal.effect().has_value());
  CHECK(!prepared_removal.incoming().has_value());
  if (!prepared_removal.plan() || !prepared_removal.application() ||
      !prepared_removal.effect())
    throw std::runtime_error("pipeline removal preparation is incomplete");
  CHECK(prepared_removal.plan()->kind() == pkgplan::operation_kind::remove);

  auto removal_effect_session = pkgctl::effectful_operation_session::admit(
      *prepared_removal.effect(), {}, {});
  auto removal_projected = pkgstate::apply_adapter::read_application_state(
      *prepared_removal.application(), *application.lease, store);
  pipeline_run_store removal_run_store(removal_reserved_record);
  recording_effect_body_sink removal_bodies;
  {
    pkgctl::native_transaction_effect_driver native_removal_driver(
        removal_projected.projection(), *application.lease,
        *application.backend, nullptr, backend, store);
    counting_effect_driver removal_driver(native_removal_driver);
    pkgctl::native_transaction_effect_publication_driver removal_state_observer(
        *application.lease, store);
    interrupting_effect_store interrupted_effect_store(
        effect_store, pkgctl::effect_attempt_stage::application_terminal);

    bool interrupted = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          removal_reserved_record, removal_reservation.run, removal_dispatch,
          removal_effect_session, effect_nonce(32U), removal_driver,
          removal_state_observer, interrupted_effect_store, removal_run_store,
          &removal_bodies);
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      interrupted = problem.code() ==
          pkgctl::effect_journal_error_code::store_write_failed;
    }
    CHECK(interrupted);
    CHECK(interrupted_effect_store.interrupted());
    CHECK(removal_driver.application_calls() == 1U);
    CHECK(removal_driver.application_resume_calls() == 0U);
    CHECK(removal_driver.publication_calls() == 0U);
  }

  CHECK(removal_bodies.application().has_value());
  CHECK(!removal_bodies.publication_request().has_value());
  CHECK(!fs::exists(application.target_root / "usr/bin/tool"));
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "local config\n");
  const auto state_before_removal_restart = store.read();
  const auto* state_before_removal_restart_tool =
      state_before_removal_restart.find_package("tool");
  CHECK(state_before_removal_restart_tool != nullptr);
  if (state_before_removal_restart_tool != nullptr)
    CHECK(state_before_removal_restart_tool->release().version() == "2.0");

  effect_store = pkgctl::posix_effect_journal_store::open(
      application.effect_journal_root.string());
  application_journal_store = pkgapply::posix::application_journal_store::open(
      application.application_journal_root.string());
  auto removal_restart = pkgctl::transaction_run_restart_checkpoint::make(
      removal_reservation.run.progress(), reopen_run_head(
          removal_run_store, removal_reserved_record.journal()));
  const auto& removal_restart_assessment = require_single_operation_restart(
      removal_restart, removal_dispatch);
  const auto removal_effect_record = effect_store.load_latest(
      *removal_restart_assessment.effect_attempt());
  CHECK(removal_effect_record.has_value());
  if (!removal_effect_record)
    throw std::runtime_error("pipeline removal restart lacks effect journal");
  CHECK(removal_effect_record->stage() ==
        pkgctl::effect_attempt_stage::application_intent);
  CHECK(pkgctl::assess_effect_restart(*removal_effect_record).disposition() ==
        pkgctl::effect_restart_disposition::resume_application);

  const auto removal_application_journal =
      application_journal_store.load_active(
          prepared_removal.application()->identity());
  CHECK(removal_application_journal.has_value());
  if (!removal_application_journal)
    throw std::runtime_error(
        "pipeline removal restart lacks application journal");

  auto removal_effect_checkpoint = pkgctl::effect_restart_checkpoint::make(
      removal_effect_session, *removal_effect_record, {}, std::nullopt, {},
      std::nullopt, std::nullopt, *removal_application_journal);

  reacquire_application_lease(application, target);
  auto resumed_removal_projection = pkgstate::apply_adapter::read_application_state(
      *prepared_removal.application(), *application.lease, store);
  pkgctl::native_transaction_effect_driver native_resumed_removal_driver(
      resumed_removal_projection.projection(), *application.lease,
      *application.backend, nullptr, backend, store);
  counting_effect_driver resumed_removal_driver(native_resumed_removal_driver);
  pkgctl::native_transaction_effect_publication_driver
      resumed_removal_state_observer(*application.lease, store);
  auto removal_recovered = pkgctl::reconcile_operation_dispatch_durable(
      std::move(removal_restart), removal_dispatch,
      std::move(removal_effect_checkpoint), &resumed_removal_driver,
      &resumed_removal_state_observer, nullptr, effect_store,
      removal_run_store, &removal_bodies);
  CHECK(removal_recovered.run_advanced);
  CHECK(removal_recovered.result.has_value());
  CHECK(removal_recovered.result && removal_recovered.result->succeeded());
  CHECK(resumed_removal_driver.application_calls() == 0U);
  CHECK(resumed_removal_driver.application_resume_calls() == 1U);
  CHECK(resumed_removal_driver.publication_calls() == 1U);
  CHECK(removal_recovered.effect_record.stage() ==
        pkgctl::effect_attempt_stage::terminal);

  const auto removed_state = store.read();
  CHECK(removed_state.find_package("tool") == nullptr);
  CHECK(!fs::exists(application.target_root / "usr/bin/tool"));
  CHECK(read_text(application.target_root / "etc/tool.conf") ==
        "local config\n");

  removal_record = removal_recovered.record;
  removal_run = std::move(removal_recovered.run);
  CHECK(removal_run.progress().status(tool_remove.identity()) ==
        pkgctl::transaction_node_status::satisfied);

  if (removal_run.progress().status(removal_dep_build.identity()) !=
      pkgctl::transaction_node_status::satisfied) {
    const auto removal_dependency = execute_reserved_construction(
        removal_run, removal_record, removal_locator, construction_driver,
        removal_dispatch_marker++, "dep");
    CHECK(removal_dependency.succeeded());
  }
  CHECK(removal_run.progress().status(removal_dep_build.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(removal_run.progress().complete());
  CHECK(removal_record.complete());

  {
    auto reconciliation_store =
        pkgreconcile::posix::inventory_generation_store::open_existing(
            reconciliation_root, reconciliation_projection.target());
    const auto inventory = reconciliation_store.read();
    const auto* record_value = inventory.find(pending_config);
    CHECK(record_value != nullptr);
    if (record_value != nullptr)
      CHECK(record_value->status() ==
            pkgreconcile::reconciliation_record_status::resolved);
  }
}


void check_pipeline_build_failure()
{
  test_support::temporary_directory temporary;
  const fs::path root = temporary.path();
  const fs::path collection = root / "collection";
  const fs::path state = root / "state";
  create_pipeline_collection(collection);
  test_support::initialize_state(state);

  auto resolution_request = pipeline_resolution_request(collection, state);
  auto transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(std::move(resolution_request)));
  const auto& dep_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "dep");
  const auto& tool_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "tool");
  const auto& tool_check = node_for(
      transaction, pkgtransaction::transaction_action_kind::check, "tool");
  const auto& tool_install = node_for(
      transaction, pkgtransaction::transaction_action_kind::install, "tool");

  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(60U));
  refusing_installed_package_source installed_packages;
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(root / "runtime"), installed_packages);
  pipeline_backend backend(pipeline_execution_fault::dependency_build);
  pkgctl::native_construction_driver driver(backend);

  const auto failed = execute_reserved_construction(
      run, record, locator, driver, 61U, "dep", false);
  CHECK(!failed.succeeded());
  CHECK(run.progress().status(dep_build.identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(run.progress().status(tool_build.identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(run.progress().status(tool_check.identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(run.progress().failed());
  CHECK(run.progress().ready_units().empty());

  pkgstate::posix::canonical_generation_store store(
      state, test_support::binding());
  CHECK(store.read().find_package("tool") == nullptr);
}

void check_pipeline_check_failure()
{
  test_support::temporary_directory temporary;
  const fs::path root = temporary.path();
  const fs::path collection = root / "collection";
  const fs::path state = root / "state";
  create_pipeline_collection(collection);
  test_support::initialize_state(state);

  auto resolution_request = pipeline_resolution_request(collection, state);
  auto transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(std::move(resolution_request)));
  const auto& dep_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "dep");
  const auto& tool_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "tool");
  const auto& tool_check = node_for(
      transaction, pkgtransaction::transaction_action_kind::check, "tool");
  const auto& tool_install = node_for(
      transaction, pkgtransaction::transaction_action_kind::install, "tool");

  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(70U));
  refusing_installed_package_source installed_packages;
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(root / "runtime"), installed_packages);
  pipeline_backend build_backend;
  pkgctl::native_construction_driver construction_driver(build_backend);

  const auto dependency = execute_reserved_construction(
      run, record, locator, construction_driver, 71U, "dep");
  CHECK(dependency.succeeded());
  const auto tool = execute_reserved_construction(
      run, record, locator, construction_driver, 72U, "tool");
  CHECK(tool.succeeded());
  CHECK(run.progress().status(tool_check.identity()) ==
        pkgctl::transaction_node_status::ready);

  pipeline_backend check_backend(pipeline_execution_fault::package_check);
  const auto failed = execute_reserved_check(
      run, record, locator, check_backend, 73U, "tool", false);
  CHECK(!failed.succeeded());
  CHECK(run.progress().status(dep_build.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(run.progress().status(tool_build.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  CHECK(run.progress().status(tool_check.identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(run.progress().failed());
  CHECK(run.progress().ready_units().empty());
  CHECK(fs::is_regular_file(tool.session().paths().build.artifact_path));

  pkgstate::posix::canonical_generation_store store(
      state, test_support::binding());
  CHECK(store.read().find_package("tool") == nullptr);
}

} // namespace

int main(int argc, char** argv)
{
  if (argc == 2 && std::string_view(argv[1]) == "--failure-matrix") {
    check_pipeline_build_failure();
    check_pipeline_check_failure();
  } else if (argc == 1) {
    check_package_pipeline();
  } else {
    std::cerr << "usage: package-pipeline-test [--failure-matrix]\n";
    return EXIT_FAILURE;
  }
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
