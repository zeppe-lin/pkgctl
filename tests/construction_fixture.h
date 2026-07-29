// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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

namespace construction_fixture {
namespace fs = std::filesystem;

inline std::string sha256_text(std::string_view bytes)
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
inline Identity source_identity(char value)
{
  return Identity::from_sha256(std::string(64U, value));
}

struct tool_source_options final {
  std::string version = "1.0";
  bool with_build_dependency = true;
  std::vector<std::string> check_dependencies;
  std::optional<pkgsource::program> check_program;
};

inline pkgsource::source_snapshot tool_source(
    std::string_view digest,
    tool_source_options options)
{
  using namespace pkgsource;

  std::vector<requirement_declaration> requirements;
  if (options.with_build_dependency) {
    requirements.emplace_back(
        requirement_scope::build(),
        requirement_subject(package_reference("dep")),
        declaration_provenance(
            "tool/recipe.yml", "requirements.build[0]", 12, 5));
  }

  for (std::size_t index = 0;
       index < options.check_dependencies.size();
       ++index) {
    requirements.emplace_back(
        requirement_scope::check(),
        requirement_subject(
            package_reference(options.check_dependencies[index])),
        declaration_provenance(
            "tool/recipe.yml",
            "requirements.check[" + std::to_string(index) + "]",
            20 + static_cast<std::uint32_t>(index),
            5));
  }

  const auto syntax = options.check_program
      ? source_syntax::recipe_yaml_v2
      : source_syntax::recipe_yaml_v1;
  return seal_source(
      source_origin("tool/recipe.yml"), syntax,
      recipe_declaration(
          package_release(
              package_reference("tool"), std::move(options.version), 1),
          package_metadata(
              "Tool", std::nullopt, std::nullopt, {"GPL-3.0-or-later"}),
          {source_input::local(
              "payload", "payload",
              pkgsource::digest(
                  pkgsource::digest_algorithm::sha256,
                  std::string(digest)))},
          program(program_language::posix_shell, "true\n"),
          std::move(requirements),
          {},
          architecture_requirements(
              {architecture_reference("x86_64")},
              {architecture_reference("x86_64")}),
          declaration_provenance("tool/recipe.yml", "$", 1, 1),
          std::move(options.check_program)),
      profile_catalog::seal({}));
}

inline pkgsource::source_snapshot tool_source(
    std::string_view digest,
    std::string version = "1.0",
    bool with_dependency = true,
    std::optional<pkgsource::program> check_program = std::nullopt)
{
  tool_source_options options;
  options.version = std::move(version);
  options.with_build_dependency = with_dependency;
  options.check_program = std::move(check_program);
  return tool_source(digest, std::move(options));
}

inline pkgsource::source_snapshot package_source(std::string name)
{
  using namespace pkgsource;
  const auto origin = name + "/recipe.yml";
  return seal_source(
      source_origin(origin), source_syntax::recipe_yaml_v1,
      recipe_declaration(
          package_release(package_reference(name), "1.0", 1),
          package_metadata(
              name, std::nullopt, std::nullopt, {"GPL-3.0-or-later"}),
          {}, program(program_language::posix_shell, "true\n"), {}, {},
          architecture_requirements(
              {architecture_reference("x86_64")},
              {architecture_reference("x86_64")}),
          declaration_provenance(origin, "$", 1, 1)),
      profile_catalog::seal({}));
}

inline pkgsource::source_snapshot dependency_source()
{
  return package_source("dep");
}

inline pkgcatalog::catalog_snapshot catalog_snapshot(
    const pkgsource::source_snapshot& source,
    std::vector<pkgsource::source_snapshot> dependencies)
{
  std::vector<pkgsource::source_snapshot> entries;
  entries.reserve(dependencies.size() + 1U);
  entries.push_back(source);
  for (auto& dependency : dependencies)
    entries.push_back(std::move(dependency));

  pkgcatalog::collection_declaration declaration(
      pkgcatalog::collection_reference("core"),
      pkgcatalog::collection_provenance(
          "/collection", std::nullopt,
          pkgsource::declaration_provenance(
              "<test>", "collections[0]", 1, 1)),
      std::move(entries));
  std::vector<pkgcatalog::catalog_collection> collections;
  collections.emplace_back(
      0, pkgcatalog::seal_collection(std::move(declaration)));
  return pkgcatalog::catalog_snapshot::seal(
      pkgsource::profile_catalog::seal({}), std::move(collections));
}

inline pkgcatalog::catalog_snapshot catalog_snapshot(
    const pkgsource::source_snapshot& source,
    const pkgsource::source_snapshot& dependency)
{
  return catalog_snapshot(
      source, std::vector<pkgsource::source_snapshot>{dependency});
}

inline pkgctl::transaction_session transaction_session(
    const pkgsource::source_snapshot& source,
    std::vector<pkgsource::source_snapshot> dependencies,
    const pkgstate::snapshot& installed,
    const fs::path& state_path,
    bool include_target = false,
    bool include_unrelated_dependency = false,
    bool include_check = false)
{
  auto catalog = catalog_snapshot(source, std::move(dependencies));
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
  if (include_unrelated_dependency)
    goals.emplace_back(
        pkgsource::requirement_scope::build(),
        pkgsource::requirement_subject(pkgsource::package_reference("dep")),
        "<test-unrelated>");
  if (include_check)
    goals.emplace_back(
        pkgsource::requirement_scope::check(),
        pkgsource::requirement_subject(pkgsource::package_reference("tool")),
        "<test-check>");
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

inline pkgctl::transaction_session transaction_session(
    const pkgsource::source_snapshot& source,
    const pkgsource::source_snapshot& dependency,
    const pkgstate::snapshot& installed,
    const fs::path& state_path,
    bool include_target = false,
    bool include_unrelated_dependency = false,
    bool include_check = false)
{
  return transaction_session(
      source, std::vector<pkgsource::source_snapshot>{dependency},
      installed, state_path, include_target,
      include_unrelated_dependency, include_check);
}

inline const pkgtransaction::transaction_node& build_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks build node");
}

inline const pkgtransaction::transaction_node& install_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::install &&
        node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks install node");
}

inline const pkgtransaction::transaction_node& check_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::check &&
        node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks check node");
}

template<typename Identity>
inline Identity plan_identity(std::uint8_t value)
{
  pkgplan::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return Identity::from_sha256(bytes);
}

template<typename Identity>
inline Identity apply_identity(std::uint8_t value)
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
inline Destination translate_identity(const Source& value)
{
  return Destination::parse(value.string());
}

inline pkgapply::application_target_context application_target(
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

inline pkgapply::application_execution_control execution_control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::exact_prior_state,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

inline pkgplan::package_policy_snapshot package_policy()
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

inline pkgplan::target_observation_set empty_target_observations(
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

inline const pkgresolve::selected_package& package_selection(
    const pkgctl::transaction_session& session,
    std::string_view package)
{
  const pkgresolve::selected_package* found = nullptr;
  for (const auto& selection : session.resolution().resolution().selections()) {
    if (selection.environment() != pkgresolve::resolution_environment::build ||
        selection.package().name() != package)
      continue;
    if (found != nullptr)
      throw std::runtime_error(
          "transaction fixture contains duplicate package selections");
    found = &selection;
  }

  if (found == nullptr)
    throw std::runtime_error("transaction fixture lacks package selection");
  return *found;
}

inline const pkgresolve::selected_package& dependency_selection(
    const pkgctl::transaction_session& session)
{
  return package_selection(session, "dep");
}

inline pkgbuild::materialized_package_input package_input(
    const pkgctl::transaction_session& session,
    std::string package,
    pkgbuild::input_scope scope,
    char result_seed,
    char artifact_seed,
    char tree_seed)
{
  const auto& selection = package_selection(session, package);
  return pkgbuild::materialized_package_input(
      pkgbuild::resolved_package_input::make(
          scope, pkgsource::package_reference(std::move(package)),
          selection.release(), selection.source_snapshot(),
          source_identity<pkgbuild::build_result_identity>(result_seed),
          source_identity<pkgbuild::artifact_identity>(artifact_seed)),
      source_identity<pkgbuild::input_tree_identity>(tree_seed));
}

inline pkgbuild::materialized_package_input dependency_input(
    const pkgctl::transaction_session& session)
{
  return package_input(
      session, "dep", pkgbuild::input_scope::build, 'd', 'e', 'f');
}

inline pkgexec::backend_capability_profile capabilities()
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
  { return construction_fixture::capabilities(); }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources& resources) override
  {
    if (mode_ == backend_mode::fail)
      return pkgexec::execution_result::failed_before_start(
          request, capabilities(),
          pkgexec::execution_failure_kind::backend_unsupported, {},
          "fixture build failure");

    if (request.purpose().kind() != pkgexec::execution_purpose_kind::build)
      throw std::runtime_error("fixture backend received non-build request");
    if (request.environment().network() != pkgexec::network_policy::denied)
      throw std::runtime_error("fixture backend received networked build request");
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

inline pkgctl::construction_session construction_session_with_inputs(
    const pkgctl::transaction_session& transaction,
    const fs::path& root,
    std::vector<pkgbuild::materialized_package_input> inputs)
{
  const auto& node = build_node(transaction);
  auto request = pkgctl::construction_request::make(
      transaction, node.identity(), inputs,
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

  std::vector<pkgbuild_exec::package_input_tree> trees;
  trees.reserve(inputs.size());
  for (const auto& input : inputs) {
    const auto input_path =
        root / "inputs" / input.resolved().identity().hex();
    test_support::write(input_path / "payload", "dependency tree\n");
    if (::chmod(input_path.c_str(), 0555) != 0 ||
        ::chmod((input_path / "payload").c_str(), 0444) != 0)
      throw std::runtime_error("cannot make package input tree read-only");
    trees.push_back(
        {input.resolved().identity(), input.tree(), input_path});
  }

  return pkgctl::construction_session::admit(
      std::move(request), std::move(paths), std::move(trees),
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
}

inline pkgctl::construction_session construction_session(
    const pkgctl::transaction_session& transaction,
    const fs::path& root)
{
  return construction_session_with_inputs(
      transaction, root, {dependency_input(transaction)});
}

inline pkgctl::construction_session construction_session_without_inputs(
    const pkgctl::transaction_session& transaction,
    const fs::path& root)
{
  return construction_session_with_inputs(transaction, root, {});
}



} // namespace construction_fixture
