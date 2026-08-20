// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/construction_codec.h>
#include <pkgctl/run_locator.h>
#include <pkgctl/run_resource.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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

const pkgtransaction::transaction_node& build_node_for(
    const pkgctl::transaction_session& transaction,
    std::string_view package)
{
  for (const auto& node : transaction.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == package)
      return node;
  throw std::runtime_error("transaction lacks requested build node");
}


class fixed_installed_package_source final
    : public pkgctl::retained_installed_package_tree_source {
public:
  fixed_installed_package_source(
      pkgstate::installed_package_identity requested_package,
      pkgstate::installed_package_identity returned_package,
      pkgexec::resource_identity resource,
      fs::path path)
      : requested_package_(std::move(requested_package)),
        returned_package_(std::move(returned_package)),
        resource_(std::move(resource)), path_(std::move(path))
  {
  }

  pkgctl::retained_installed_package_tree locate(
      const pkgstate::installed_package& package) override
  {
    ++calls_;
    if (package.identity() != requested_package_)
      throw std::runtime_error(
          "installed package source received another package");
    return {returned_package_, resource_, path_};
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  pkgstate::installed_package_identity requested_package_;
  pkgstate::installed_package_identity returned_package_;
  pkgexec::resource_identity resource_;
  fs::path path_;
  std::size_t calls_ = 0U;
};

class refusing_installed_package_source final
    : public pkgctl::retained_installed_package_tree_source {
public:
  pkgctl::retained_installed_package_tree locate(
      const pkgstate::installed_package&) override
  {
    ++calls_;
    throw std::runtime_error(
        "installed package resource was unexpectedly requested");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
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
          root / "installed-resources",
          root / "check-resources",
          root / "check-temporary",
          pkgexec::root_view_identity::from_sha256(std::string(64U, '8')),
          root / "root-view",
      },
      {
          pkgbuild::build_policy::make(
              pkgbuild::environment_policy::hermetic(
                  2, 0022, 1700000000)),
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

pkgctl::construction_result build_dependency(
    const pkgctl::transaction_session& transaction,
    const fs::path& root)
{
  const auto& node = build_node_for(transaction, "dep");
  auto request = pkgctl::construction_request::make(
      transaction, node.identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(
              2, 0022, 1700000000)));
  pkgctl::construction_paths paths{
      root / "collection" / "dep",
      root / "dependency-content",
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          root / "dependency-root-view",
          root / "dependency-session",
          root / "dependency-package",
          root / "dependency-artifact" / "dep.tar",
      },
  };
  fs::create_directories(paths.build.root_view_path);
  auto session = pkgctl::construction_session::admit(
      std::move(request), std::move(paths), {},
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
  fixture_backend backend(backend_mode::succeed, true);
  pkgctl::native_construction_driver driver(backend);
  return pkgctl::execute_construction(std::move(session), driver);
}

void check_native_locator()
{
  test_support::temporary_directory temporary;
  const auto root = temporary.path();
  const auto collection = root / "collection";
  fs::create_directories(collection / "tool");
  fs::create_directories(collection / "dep");

  const std::string payload = "locator source bytes\n";
  test_support::write(collection / "tool" / "payload", payload);

  tool_source_options options;
  // A check-only dependency must not enter the construction resource set.
  options.with_build_dependency = false;
  options.check_dependencies = {"dep"};
  options.source_document =
      (collection / "tool" / "recipe.yml").generic_string();
  options.check_program = pkgsource::program(
      pkgsource::program_language::posix_shell, "true\n");
  auto source = tool_source(sha256_text(payload), std::move(options));
  auto dependency = package_source(
      "dep", (collection / "dep" / "recipe.yml").generic_string());

  const auto binding = test_support::binding();
  auto installed = pkgstate::snapshot::make(binding);
  auto transaction = transaction_session(
      source, dependency, installed, root / "state", false, false, true,
      collection);

  auto dependency_result = build_dependency(transaction, root);
  CHECK(dependency_result.succeeded());
  auto progress = pkgctl::transaction_progress::begin(transaction);
  progress = pkgctl::advance_construction(
      std::move(progress), dependency_result);

  refusing_installed_package_source installed_packages;
  const auto runtime_root = root / "runtime";
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(runtime_root), installed_packages);

  auto run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(1U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(1U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  const auto dispatch = *reservation.dispatch;
  CHECK(dispatch.unit().kind() ==
        pkgctl::transaction_unit_kind::construction);
  const auto* node = transaction.program().find(
      dispatch.unit().primary_node());
  CHECK(node != nullptr && node->package().name() == "tool");
  auto reserved_record = record.successor(reservation.run);

  auto session = locator.construction(
      reserved_record, reservation.run.progress(), dispatch);
  auto repeated = locator.construction(
      reserved_record, reservation.run.progress(), dispatch);
  CHECK(session.identity() == repeated.identity());
  CHECK(session.request().build_node() == dispatch.unit().primary_node());
  CHECK(session.paths().local_source_root == collection / "tool");
  CHECK(session.paths().content_store_root == runtime_root / "content");
  CHECK(session.package_inputs().empty());
  CHECK(installed_packages.calls() == 0U);
  CHECK(!fs::exists(session.paths().build.session_root));
  CHECK(!fs::exists(session.paths().build.package_output_root));
  CHECK(!fs::exists(session.paths().build.artifact_path));

  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, dispatch, session);
  auto started_record = reserved_record.successor(started_run);
  auto recovered = locator.construction(
      started_record, started_run.progress(), dispatch);
  CHECK(recovered.identity() == session.identity());
  CHECK(recovered.paths().build.session_root ==
        session.paths().build.session_root);

  // Deliberately make the two fixture package trees metadata-identical so
  // this case proves role-scoped execution identities do not collapse when
  // candidate and principal package-image authority is equal.
  fixture_backend build_backend(backend_mode::succeed, true);
  pkgctl::native_construction_driver build_driver(build_backend);
  auto construction = pkgctl::execute_construction(session, build_driver);
  CHECK(construction.succeeded());
  CHECK(dependency_result.build().image_authority());
  CHECK(construction.build().image_authority());
  if (dependency_result.build().image_authority() &&
      construction.build().image_authority()) {
    CHECK(dependency_result.build().image_authority()->image().image().identity() ==
          construction.build().image_authority()->image().image().identity());
  }
  progress = pkgctl::advance_construction(
      std::move(progress), construction);

  auto check_run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto check_record = pkgctl::transaction_run_journal_record::admit(
      check_run, journal_nonce(2U));
  auto check_reservation = pkgctl::reserve_next(
      check_run, dispatch_nonce(2U));
  CHECK(check_reservation.dispatch.has_value());
  if (!check_reservation.dispatch)
    return;
  const auto check_dispatch = *check_reservation.dispatch;
  CHECK(check_dispatch.unit().kind() == pkgctl::transaction_unit_kind::check);
  auto reserved_check_record = check_record.successor(check_reservation.run);

  auto check_session = locator.check(
      reserved_check_record, check_reservation.run.progress(), check_dispatch);

  // CHECK with no installed package input does not borrow present package-byte
  // authority. Candidate/source resources remain owned by their existing
  // construction/check preparation boundaries.
  pkgctl::native_transaction_resource_session_source resource_sessions(
      configuration(root / "resource-runtime"), nullptr);
  auto resource_only_check = resource_sessions.check(
      reserved_check_record, check_reservation.run.progress(), check_dispatch);
  CHECK(resource_only_check.execution_session().inputs().size() == 1U);

  auto repeated_check = locator.check(
      reserved_check_record, check_reservation.run.progress(), check_dispatch);
  CHECK(check_session.identity() == repeated_check.identity());
  CHECK(!fs::exists(
      check_session.execution_session().paths().temporary_root));

  const auto check_scope =
      fs::path(reserved_check_record.journal().hex()) /
      check_dispatch.identity().hex();
  const auto check_resource_root = runtime_root / "check-resources" / check_scope;
  CHECK(check_session.execution_session().source().path ==
        check_resource_root / "source");
  CHECK(check_session.execution_session().package().path ==
        check_resource_root / "package");
  CHECK(check_session.execution_session().source().tree !=
        check_session.execution_session().package().tree);
  CHECK(check_session.execution_session().inputs().size() == 1U);
  CHECK(check_session.execution_session().inputs().front().path ==
        check_resource_root / "inputs" /
            check_session.execution_session().inputs().front().input.hex());
  CHECK(check_session.execution_session().inputs().front().resource !=
        check_session.execution_session().source().tree);
  CHECK(check_session.execution_session().inputs().front().resource !=
        check_session.execution_session().package().tree);
  CHECK(repeated_check.execution_session().source().tree ==
        check_session.execution_session().source().tree);
  CHECK(repeated_check.execution_session().package().tree ==
        check_session.execution_session().package().tree);
  CHECK(repeated_check.execution_session().inputs().front().resource ==
        check_session.execution_session().inputs().front().resource);
  CHECK(check_session.execution_session().source().path !=
        construction.session().paths().build.session_root / "source");
  CHECK(check_session.execution_session().package().path !=
        construction.session().paths().build.package_output_root);

  auto started_check_run = pkgctl::start_check_dispatch(
      check_reservation.run, check_dispatch, check_session);
  auto started_check_record = reserved_check_record.successor(
      started_check_run);
  auto recovered_check = locator.check(
      started_check_record, started_check_run.progress(), check_dispatch);
  CHECK(recovered_check.identity() == check_session.identity());
}


void check_installed_input_location()
{
  test_support::temporary_directory temporary;
  const auto root = temporary.path();
  const auto collection = root / "collection";
  fs::create_directories(collection / "tool");
  fs::create_directories(collection / "dep");

  tool_source_options options;
  options.source_document =
      (collection / "tool" / "recipe.yml").generic_string();
  auto source = tool_source(sha256_text("unused source bytes"),
                            std::move(options));
  auto dependency = package_source(
      "dep", (collection / "dep" / "recipe.yml").generic_string());
  const auto binding = test_support::binding();
  auto retained_package = installed_package(dependency, binding);
  auto installed = pkgstate::snapshot::make(binding, {retained_package});
  auto transaction = transaction_session(
      source, dependency, installed, root / "state", false, false, false,
      collection);

  const auto retained_resource =
      pkgexec::resource_identity::from_sha256(std::string(64U, '7'));
  const auto retained_path = root / "retained" / "dep";
  fixed_installed_package_source installed_packages(
      retained_package.identity(), retained_package.identity(),
      retained_resource, retained_path);
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(root / "runtime"), installed_packages);

  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(3U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(3U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  const auto dispatch = *reservation.dispatch;
  CHECK(dispatch.unit().kind() ==
        pkgctl::transaction_unit_kind::construction);
  const auto* node = transaction.program().find(
      dispatch.unit().primary_node());
  CHECK(node != nullptr && node->package().name() == "tool");

  auto session = locator.construction(
      record.successor(reservation.run), reservation.run.progress(), dispatch);
  CHECK(installed_packages.calls() == 1U);
  CHECK(session.package_inputs().size() == 1U);
  CHECK(session.package_inputs().front().resource == retained_resource);
  CHECK(session.package_inputs().front().path == retained_path);
  CHECK(!fs::exists(retained_path));

  const auto retained_session = pkgctl::encode_construction_session(session);
  auto decoded_session = pkgctl::decode_construction_session(
      retained_session, transaction, dispatch.unit().primary_node());
  CHECK(decoded_session.identity() == session.identity());
  CHECK(decoded_session.package_inputs().size() == 1U);
  CHECK(decoded_session.package_inputs().front().resource == retained_resource);
  CHECK(decoded_session.package_inputs().front().path == retained_path);
  CHECK(installed_packages.calls() == 1U);

  fixed_installed_package_source foreign_package(
      retained_package.identity(),
      fixture_state_identity<pkgstate::installed_package_identity>(90U),
      retained_resource, retained_path);
  pkgctl::native_transaction_dispatch_session_source foreign_locator(
      configuration(root / "foreign-runtime"), foreign_package);
  bool foreign_rejected = false;
  try {
    (void)foreign_locator.construction(
        record.successor(reservation.run), reservation.run.progress(), dispatch);
  } catch (const pkgctl::native_session_locator_error& problem) {
    foreign_rejected = problem.code() ==
        pkgctl::native_session_locator_error_code::installed_resource_mismatch;
  }
  CHECK(foreign_rejected);

  fixed_installed_package_source root_path(
      retained_package.identity(), retained_package.identity(),
      retained_resource, fs::path("/"));
  pkgctl::native_transaction_dispatch_session_source root_path_locator(
      configuration(root / "root-path-runtime"), root_path);
  bool root_path_rejected = false;
  try {
    (void)root_path_locator.construction(
        record.successor(reservation.run), reservation.run.progress(), dispatch);
  } catch (const pkgctl::native_session_locator_error& problem) {
    root_path_rejected = problem.code() ==
        pkgctl::native_session_locator_error_code::invalid_resource_path;
  }
  CHECK(root_path_rejected);
}


void check_source_coordinate_rejection()
{
  test_support::temporary_directory temporary;
  const auto root = temporary.path();
  const auto collection = root / "collection";
  fs::create_directories(collection / "tool");
  fs::create_directories(collection / "dep");

  tool_source_options options;
  options.with_build_dependency = false;
  options.source_document =
      (root / "foreign" / "tool" / "recipe.yml").generic_string();
  auto source = tool_source(sha256_text("unused source bytes"),
                            std::move(options));
  auto dependency = package_source(
      "dep", (collection / "dep" / "recipe.yml").generic_string());
  auto installed = pkgstate::snapshot::make(test_support::binding());
  auto transaction = transaction_session(
      source, dependency, installed, root / "state", false, false, false,
      collection);

  refusing_installed_package_source installed_packages;
  pkgctl::native_transaction_dispatch_session_source locator(
      configuration(root / "runtime"), installed_packages);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(4U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(4U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;

  bool rejected = false;
  try {
    (void)locator.construction(
        record.successor(reservation.run), reservation.run.progress(),
        *reservation.dispatch);
  } catch (const pkgctl::native_session_locator_error& problem) {
    rejected = problem.code() ==
        pkgctl::native_session_locator_error_code::source_coordinate_mismatch;
  }
  CHECK(rejected);
  CHECK(installed_packages.calls() == 0U);
}

void check_configuration_rejection()
{
  test_support::temporary_directory temporary;
  const auto root = temporary.path();
  bool rejected = false;
  try {
    auto ignored = pkgctl::native_transaction_session_configuration::make(
        {
            root / "shared",
            root / "shared" / "sessions",
            root / "packages",
            root / "artifacts",
            root / "installed-resources",
            root / "check-resources",
            root / "checks",
            pkgexec::root_view_identity::from_sha256(std::string(64U, 'd')),
            root / "root-view",
        },
        {
            pkgbuild::build_policy::make(
                pkgbuild::environment_policy::hermetic(
                    1, 0022, 1700000000)),
            pkgfetch::acquisition_policy::defaults(),
            {
                pkgexec::interpreter_identity::from_sha256(
                    std::string(64U, 'e')),
                0, 0, {},
            },
            {
                pkgexec::interpreter_identity::from_sha256(
                    std::string(64U, 'f')),
                0, 0, {},
            },
        });
    (void)ignored;
  } catch (const pkgctl::native_session_locator_error& problem) {
    rejected = problem.code() ==
        pkgctl::native_session_locator_error_code::invalid_configuration;
  }
  CHECK(rejected);
}

} // namespace

int main()
{
  try {
    check_native_locator();
    check_installed_input_location();
    check_source_coordinate_rejection();
    check_configuration_rejection();
  } catch (const std::exception& problem) {
    std::cerr << "unexpected exception: " << problem.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
