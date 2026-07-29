// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"

#include <pkgctl/construction.h>
#include <pkgctl/preparation.h>
#include <pkgctl/error.h>

#include <libpkgresolve/resolver.h>
#include <libpkgimage/libarchive_backend.h>
#include <libpkgtransaction/composer.h>

#include <openssl/evp.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace {

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

std::string sha256_text(std::string_view bytes)
{
  std::array<unsigned char, 32> output{};
  unsigned int size = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), output.data(), &size,
                 EVP_sha256(), nullptr) != 1 || size != output.size())
    throw std::runtime_error("cannot hash fixture bytes");
  static constexpr char hex[] = "0123456789abcdef";
  std::string result(64U, '0');
  for (std::size_t index = 0; index < output.size(); ++index)
  {
    result[index * 2U] = hex[output[index] >> 4U];
    result[index * 2U + 1U] = hex[output[index] & 0x0fU];
  }
  return result;
}

template<typename Identity>
Identity source_identity(char value)
{
  return Identity::from_sha256(std::string(64U, value));
}

pkgsource::source_snapshot tool_source(std::string_view digest,
                                      std::string version = "1.0")
{
  using namespace pkgsource;
  return seal_source(
      source_origin("tool/recipe.yml"), source_syntax::recipe_yaml_v1,
      recipe_declaration(
          package_release(package_reference("tool"), std::move(version), 1),
          package_metadata("Tool", std::nullopt, std::nullopt,
                           {"GPL-3.0-or-later"}),
          {source_input::local(
              "payload", "payload",
              pkgsource::digest(pkgsource::digest_algorithm::sha256,
                                std::string(digest)))},
          program(program_language::posix_shell, "true\n"),
          {requirement_declaration(
              requirement_scope::build(),
              requirement_subject(package_reference("dep")),
              declaration_provenance(
                  "tool/recipe.yml", "requirements.build[0]", 12, 5))},
          {},
          architecture_requirements(
              {architecture_reference("x86_64")},
              {architecture_reference("x86_64")}),
          declaration_provenance("tool/recipe.yml", "$", 1, 1)),
      profile_catalog::seal({}));
}

pkgsource::source_snapshot dependency_source()
{
  using namespace pkgsource;
  return seal_source(
      source_origin("dep/recipe.yml"), source_syntax::recipe_yaml_v1,
      recipe_declaration(
          package_release(package_reference("dep"), "1.0", 1),
          package_metadata("Dependency", std::nullopt, std::nullopt,
                           {"GPL-3.0-or-later"}),
          {}, program(program_language::posix_shell, "true\n"), {}, {},
          architecture_requirements(
              {architecture_reference("x86_64")},
              {architecture_reference("x86_64")}),
          declaration_provenance("dep/recipe.yml", "$", 1, 1)),
      profile_catalog::seal({}));
}

pkgcatalog::catalog_snapshot catalog_snapshot(
    const pkgsource::source_snapshot& source,
    const pkgsource::source_snapshot& dependency)
{
  pkgcatalog::collection_declaration declaration(
      pkgcatalog::collection_reference("core"),
      pkgcatalog::collection_provenance(
          "/collection", std::nullopt,
          pkgsource::declaration_provenance(
              "<test>", "collections[0]", 1, 1)),
      {source, dependency});
  std::vector<pkgcatalog::catalog_collection> collections;
  collections.emplace_back(0, pkgcatalog::seal_collection(std::move(declaration)));
  return pkgcatalog::catalog_snapshot::seal(
      pkgsource::profile_catalog::seal({}), std::move(collections));
}

pkgctl::transaction_session transaction_session(
    const pkgsource::source_snapshot& source,
    const pkgsource::source_snapshot& dependency,
    const pkgstate::snapshot& installed,
    const fs::path& state_path,
    bool include_target = false)
{
  auto catalog = catalog_snapshot(source, dependency);
  std::vector<pkgcatalog::acquire::collection_specification> specifications;
  specifications.emplace_back(
      0, pkgcatalog::collection_reference("core"), fs::path("/collection"),
      std::nullopt,
      pkgsource::declaration_provenance(
          "<test>", "collections[0]", 1, 1));
  auto catalog_request = pkgctl::catalog_request::make(specifications);
  auto catalog_session = pkgctl::catalog_session::seal(catalog_request, catalog);

  std::vector<pkgresolve::resolution_goal> goals;
  goals.emplace_back(
      pkgsource::requirement_scope::build(),
      pkgsource::requirement_subject(pkgsource::package_reference("tool")),
      "<test>");
  if (include_target)
    goals.emplace_back(
        pkgsource::requirement_scope::run(),
        pkgsource::requirement_subject(pkgsource::package_reference("tool")),
        "<test-target>");
  pkgresolve::architecture_context architectures(
      pkgsource::architecture_reference("x86_64"),
      pkgsource::architecture_reference("x86_64"));
  pkgresolve::resolution_policy resolver_policy;
  auto controller_request = pkgctl::resolution_request::make(
      catalog_request,
      pkgctl::state_location::make(state_path, installed.target_binding()),
      architectures, goals, resolver_policy);
  auto native_request = pkgresolve::resolution_request::seal(
      catalog, installed, architectures, goals, resolver_policy);
  auto result = pkgresolve::resolve(std::move(native_request));
  auto resolution_session = pkgctl::resolution_session::seal(
      controller_request, catalog_session, installed, result);

  auto request = pkgctl::transaction_request::make(controller_request);
  auto native_transaction = pkgtransaction::transaction_request::seal(
      result, request.convergence());
  auto program = pkgtransaction::compose(std::move(native_transaction));
  return pkgctl::transaction_session::seal(
      std::move(request), std::move(resolution_session), std::move(program));
}

const pkgtransaction::transaction_node& build_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks build node");
}

const pkgtransaction::transaction_node& install_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::install &&
        node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks install node");
}

template<typename Identity>
Identity plan_identity(std::uint8_t value)
{
  pkgplan::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return Identity::from_sha256(bytes);
}

template<typename Identity>
Identity apply_identity(std::uint8_t value)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string text = "v1:sha256:";
  for (std::size_t index = 0; index < 32U; ++index)
  {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text.push_back(digits[(byte >> 4U) & 0x0fU]);
    text.push_back(digits[byte & 0x0fU]);
  }
  return Identity::parse(text);
}

template<typename Destination, typename Source>
Destination translate_identity(const Source& value)
{
  return Destination::parse(value.string());
}

pkgapply::application_target_context application_target(
    const pkgstate::state_target_binding& state_target,
    const pkgplan::target_system_context_identity& target)
{
  return pkgapply::application_target_context::make(
      target,
      translate_identity<pkgapply::managed_target_identity>(
          state_target.managed_target()),
      translate_identity<pkgapply::root_view_identity>(
          state_target.root_view()),
      apply_identity<pkgapply::observation_backend_identity>(11),
      apply_identity<pkgapply::mutation_backend_identity>(12),
      apply_identity<pkgapply::mutation_exclusion_domain_identity>(13),
      apply_identity<pkgapply::active_object_namespace_identity>(14),
      apply_identity<pkgapply::rejected_object_store_identity>(15),
      apply_identity<pkgapply::staging_namespace_identity>(16),
      apply_identity<pkgapply::journal_namespace_identity>(17),
      apply_identity<pkgapply::execution_capability_profile_identity>(18));
}

pkgapply::application_execution_control execution_control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::exact_prior_state,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

pkgplan::package_policy_snapshot package_policy()
{
  return pkgplan::package_policy_snapshot(
      plan_identity<pkgplan::policy_snapshot_identity>(50),
      pkgplan::normalized_path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(),
          pkgplan::shared_ownership_policy::forbid,
          pkgplan::directory_cleanup_policy::remove_if_empty),
      {});
}

pkgplan::target_observation_set empty_target_observations(
    const pkgplan::target_system_context_identity& target)
{
  std::vector<pkgplan::target_path_observation> observations;
  for (const char* path : {"usr", "usr/bin", "usr/bin/tool"})
    observations.push_back(pkgplan::target_path_observation::absent(
        pkgplan::package_path::parse(path)));
  return pkgplan::target_observation_set(
      plan_identity<pkgplan::observation_set_identity>(51), target,
      pkgplan::fact_set_completeness::complete, std::move(observations));
}

const pkgresolve::selected_package& dependency_selection(
    const pkgctl::transaction_session& session)
{
  for (const auto& selection : session.resolution().resolution().selections())
    if (selection.environment() == pkgresolve::resolution_environment::build &&
        selection.package().name() == "dep")
      return selection;
  throw std::runtime_error("transaction fixture lacks dependency selection");
}

pkgbuild::materialized_package_input dependency_input(
    const pkgctl::transaction_session& session)
{
  const auto& selection = dependency_selection(session);
  return pkgbuild::materialized_package_input(
      pkgbuild::resolved_package_input::make(
          pkgbuild::input_scope::build,
          pkgsource::package_reference("dep"), selection.release(),
          selection.source_snapshot(),
          source_identity<pkgbuild::build_result_identity>('d'),
          source_identity<pkgbuild::artifact_identity>('e')),
      source_identity<pkgbuild::input_tree_identity>('f'));
}

pkgexec::backend_capability_profile capabilities()
{
  return pkgexec::backend_capability_profile::seal(
      pkgexec::backend_identity::from_sha256(std::string(64U, 'a')),
      {
          pkgexec::execution_guarantee::exact_interpreter,
          pkgexec::execution_guarantee::closed_environment,
          pkgexec::execution_guarantee::root_view,
          pkgexec::execution_guarantee::read_only_resources,
          pkgexec::execution_guarantee::writable_resources,
          pkgexec::execution_guarantee::fixed_credentials,
          pkgexec::execution_guarantee::network_denied,
          pkgexec::execution_guarantee::complete_stdout_capture,
          pkgexec::execution_guarantee::complete_stderr_capture,
          pkgexec::execution_guarantee::cleanup_verified,
      });
}

enum class backend_mode { succeed, fail };

class fixture_backend final : public pkgexec::execution_backend {
public:
  explicit fixture_backend(backend_mode mode) : mode_(mode) {}

  pkgexec::backend_capability_profile capabilities() const override
  { return ::capabilities(); }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override
  {
    if (mode_ == backend_mode::fail)
      return pkgexec::execution_result::failed_before_start(
          request, capabilities(),
          pkgexec::execution_failure_kind::backend_unsupported, {},
          "fixture build failure");

    CHECK(request.purpose().kind() == pkgexec::execution_purpose_kind::build);
    CHECK(request.environment().network() == pkgexec::network_policy::denied);
    const auto slot = pkgexec::resource_slot::singleton(
        pkgexec::resource_role::package_output_root);
    const auto& binding = request.resources().binding(slot);
    const fs::path output =
        resources.materialization(binding.resource()).host_path();
    fs::create_directories(output / "usr/bin");
    test_support::write(output / "usr/bin/tool", "tool\n");
    if (::chmod((output / "usr/bin/tool").c_str(), 0755) != 0)
      throw std::runtime_error("cannot chmod fixture payload");

    return pkgexec::execution_result::succeeded(
        request, capabilities(), request.interpreter(),
        pkgexec::stream_capture::retained("fixture stdout\n"),
        pkgexec::stream_capture::retained("fixture stderr\n"),
        capabilities().guarantees(), "fixture success");
  }

private:
  backend_mode mode_;
};


class mismatched_materialization_driver final : public pkgctl::construction_driver {
public:
  explicit mismatched_materialization_driver(pkgsource::source_snapshot source)
      : source_(std::move(source)) {}

  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) override
  {
    return pkgfetch::materialize(pkgfetch::materialization_request::seal(
        source_, request.local_source_root(), request.content_store_root(),
        request.policy()));
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session&) override
  {
    throw std::runtime_error("mismatched materialization reached build execution");
  }

private:
  pkgsource::source_snapshot source_;
};

class mismatched_build_driver final : public pkgctl::construction_driver {
public:
  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) override
  {
    return pkgfetch::materialize(request);
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session& session) override
  {
    auto alternate = pkgbuild::build_request::seal(
        session.request().source(), session.request().sources().materials(),
        session.request().inputs().inputs(),
        session.request().architectures().build(),
        session.request().architectures().target(),
        pkgbuild::build_policy::make(
            pkgbuild::environment_policy::hermetic(3, 0022, 1700000000)));
    auto admitted = pkgbuild_exec::admitted_build_session::admit(
        std::move(alternate), session.sources(), session.package_inputs(),
        session.paths(), session.identity(), session.compression());
    return pkgbuild_exec::execute(admitted, backend_);
  }

private:
  fixture_backend backend_{backend_mode::fail};
};

pkgctl::construction_session construction_session(
    const pkgctl::transaction_session& transaction,
    const fs::path& root)
{
  const auto& node = build_node(transaction);
  auto input = dependency_input(transaction);
  auto request = pkgctl::construction_request::make(
      transaction, node.identity(), {input},
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(2, 0022, 1700000000)));
  pkgctl::construction_paths paths{
      root / "sources",
      root / "store",
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          root / "root-view",
          root / "session",
          root / "package",
          root / "artifact" / "tool.tar",
      },
  };
  fs::create_directories(paths.local_source_root);
  fs::create_directories(paths.build.root_view_path);
  const auto input_path = root / "inputs" / "dep";
  test_support::write(input_path / "dep", "dependency tree\n");
  if (::chmod(input_path.c_str(), 0555) != 0 ||
      ::chmod((input_path / "dep").c_str(), 0444) != 0)
    throw std::runtime_error("cannot make dependency fixture read-only");
  std::vector<pkgbuild_exec::package_input_tree> trees{
      {input.resolved().identity(), input.tree(), input_path},
  };
  return pkgctl::construction_session::admit(
      std::move(request), std::move(paths), std::move(trees),
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
}

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
}

void check_install_preparation()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true);
  auto session = construction_session(transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(backend);
  auto construction = pkgctl::execute_construction(session, construction_driver);

  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(52);
  auto request = pkgctl::operation_preparation_request::install(
      transaction, install_node(transaction).identity(), construction,
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
  CHECK(result.effect() && result.application() &&
        result.effect()->application().identity() ==
            result.application()->identity());
  CHECK(result.artifact() && construction.build().artifact_inspection() &&
        result.artifact()->image().receipt().archive_digest() ==
            construction.build().artifact_inspection()->archive_digest());
  CHECK(result.artifact()->image().receipt().image_identity() ==
        construction.build().artifact_inspection()->image_identity());
  CHECK(result.artifact()->image().receipt().entry_count() ==
        construction.build().artifact_inspection()->entry_count());
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

} // namespace

int main()
{
  try
  {
    check_success();
    check_install_preparation();
    check_failed_build();
    check_admission();
    check_identity_and_driver_contract();
  }
  catch (const std::exception& value)
  {
    std::cerr << "unexpected exception: " << value.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
