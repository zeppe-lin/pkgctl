// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/controller.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/preparation.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_locator.h>

#include <libpkgimage/package_path.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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
    std::string_view source_digest,
    bool with_dependency)
{
  std::string requirements = "  {}\n";
  if (with_dependency)
    requirements = "  build:\n    - package: dep\n";

  return "format: zeppe-lin.recipe/1\n"
         "\n"
         "package:\n"
         "  name: " + name + "\n"
         "  version: 1.0\n"
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
         "    " + name + "-build\n"
         "\n"
         "architectures:\n"
         "  build:\n"
         "    - x86_64\n"
         "  target:\n"
         "    - x86_64\n";
}

void create_pipeline_collection(const fs::path& root)
{
  const std::string dep_source = "dependency source\n";
  const std::string tool_source = "tool source\n";
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
      pipeline_recipe("dep", sha256_text(dep_source), false));
  test_support::write(root / "dep" / "files/source.txt", dep_source);
  test_support::write(
      root / "tool" / "recipe.yml",
      pipeline_recipe("tool", sha256_text(tool_source), true));
  test_support::write(root / "tool" / "files/source.txt", tool_source);
}

const pkgtransaction::transaction_node& node_for(
    const pkgctl::transaction_session& transaction,
    pkgtransaction::transaction_action_kind action,
    std::string_view package)
{
  for (const auto& node : transaction.program().nodes())
    if (node.action() == action && node.package().name() == package)
      return node;
  throw std::runtime_error("pipeline transaction lacks requested node");
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

class pipeline_backend final : public pkgexec::execution_backend {
public:
  pkgexec::backend_capability_profile capabilities() const override
  {
    return construction_fixture::capabilities();
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override
  {
    if (request.purpose().kind() != pkgexec::execution_purpose_kind::build)
      throw std::runtime_error("pipeline backend received non-build request");

    const auto source_slot = pkgexec::resource_slot::named(
        pkgexec::resource_role::source_tree, "sources");
    const auto& source_binding = request.resources().binding(source_slot);
    const fs::path source_root =
        resources.materialization(source_binding.resource()).host_path();
    const std::string source = read_text(source_root / "source.txt");

    const auto output_slot = pkgexec::resource_slot::singleton(
        pkgexec::resource_role::package_output_root);
    const auto& output_binding = request.resources().binding(output_slot);
    const fs::path output =
        resources.materialization(output_binding.resource()).host_path();

    const auto program = request.program().material();
    if (program == "dep-build\n") {
      if (source != "dependency source\n")
        throw std::runtime_error("dependency source materialization changed");
      fs::create_directories(output / "usr/lib");
      test_support::write(output / "usr/lib/dep.marker", "dependency package\n");
    } else if (program == "tool-build\n") {
      if (source != "tool source\n")
        throw std::runtime_error("tool source materialization changed");
      const auto input_slot = pkgexec::resource_slot::named(
          pkgexec::resource_role::build_input_tree, "dep");
      const auto& input_binding = request.resources().binding(input_slot);
      const fs::path dependency =
          resources.materialization(input_binding.resource()).host_path();
      if (read_text(dependency / "usr/lib/dep.marker") != "dependency package\n")
        throw std::runtime_error("tool did not receive predecessor package tree");
      fs::create_directories(output / "usr/bin");
      test_support::write(output / "usr/bin/tool", "constructed tool\n");
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
};

pkgctl::construction_result execute_reserved_construction(
    pkgctl::transaction_run& run,
    pkgctl::transaction_run_journal_record& record,
    pkgctl::native_transaction_dispatch_session_source& locator,
    pkgctl::native_construction_driver& driver,
    std::uint8_t nonce,
    std::string_view expected_package)
{
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(nonce));
  if (!reservation.dispatch)
    throw std::runtime_error("pipeline has no ready construction dispatch");
  const auto dispatch = *reservation.dispatch;
  const auto* node = reservation.run.progress().transaction().program().find(
      dispatch.unit().primary_node());
  if (dispatch.unit().kind() != pkgctl::transaction_unit_kind::construction ||
      node == nullptr || node->package().name() != expected_package)
    throw std::runtime_error("pipeline dependency order changed");

  auto reserved_record = record.successor(reservation.run);
  auto session = locator.construction(
      reserved_record, reservation.run.progress(), dispatch);
  auto started = pkgctl::start_construction_dispatch(
      reservation.run, dispatch, session);
  auto started_record = reserved_record.successor(started);
  auto construction = pkgctl::execute_construction(session, driver);
  if (!construction.succeeded())
    throw std::runtime_error("pipeline construction failed");
  auto completed = pkgctl::complete_construction_dispatch(
      started, dispatch, construction);
  record = started_record.successor(completed);
  run = std::move(completed);
  return construction;
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

  auto resolution_request = test_support::resolution_request(collection, state);
  const auto resolution = pkgctl::resolve_packages(resolution_request);
  CHECK(resolution.resolution().selections().size() >= 2U);

  auto transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(std::move(resolution_request)));
  const auto& dep_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "dep");
  const auto& tool_build = node_for(
      transaction, pkgtransaction::transaction_action_kind::build, "tool");
  const auto& tool_install = node_for(
      transaction, pkgtransaction::transaction_action_kind::install, "tool");

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
  CHECK(run.progress().status(tool_install.identity()) ==
        pkgctl::transaction_node_status::ready);

  pkgstate::posix::canonical_generation_store store(state, test_support::binding());
  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(52);
  auto preparation_request = pkgctl::operation_preparation_request::install(
      run.progress(), tool_install.identity(), tool,
      application_target(store.read().target_binding(), target_system),
      execution_control(), empty_target_observations(target_system),
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
}

} // namespace

int main()
{
  check_package_pipeline();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
