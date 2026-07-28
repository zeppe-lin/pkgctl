// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"

#include <pkgctl/effect.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include <libpkgbuild/libpkgbuild.h>
#include <libpkgcatalog/collection.h>
#include <libpkgimage/inspection_receipt.h>
#include <libpkgimage/package_entry.h>
#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>
#include <libpkgresolve/result.h>
#include <libpkgtransaction/composer.h>
#include <libpkgstate/canonical_generation_store.h>
#include <libpkgstate/installed_control.h>
#include <libpkgstate/installed_package.h>
#include <libpkgstate/owned_entry.h>
#include <libpkgstate/package_source_record.h>

namespace {

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

template<typename Identity>
Identity state_identity(std::uint8_t value)
{
  pkgstate::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return Identity::from_sha256(bytes);
}

template<typename Identity>
Identity plan_identity(std::uint8_t value)
{
  pkgplan::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return Identity::from_sha256(bytes);
}

std::string hex_digest(std::uint8_t value)
{
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (std::size_t index = 0; index < 32; ++index)
  {
    result.push_back(digits[(value >> 4U) & 0x0fU]);
    result.push_back(digits[value & 0x0fU]);
  }
  return result;
}

pkgapply_exec::lifecycle_execution_identity test_lifecycle_execution_identity()
{
  // prepare() must be realizable by the unprivileged test runner.
  return {pkgexec::interpreter_identity::from_sha256(hex_digest(91)),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {}};
}

template<typename Identity>
Identity source_identity(std::uint8_t value)
{
  return Identity::from_sha256(hex_digest(value));
}

template<typename Identity>
Identity apply_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char digits[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index)
  {
    const std::uint8_t byte = static_cast<std::uint8_t>(value + index);
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

pkgsource::source_snapshot source_snapshot(
    std::string version,
    std::vector<pkgsource::lifecycle_action> actions)
{
  using namespace pkgsource;
  std::vector<lifecycle_program> lifecycle;
  lifecycle.reserve(actions.size());
  for (const auto action : actions)
  {
    lifecycle.emplace_back(
        action,
        program(program_language::posix_shell,
                std::string(to_string(action)) + "\n"));
  }
  return seal_source(
      source_origin("recipe.yml"), source_syntax::recipe_yaml_v1,
      recipe_declaration(
          package_release(package_reference("tool"), std::move(version), 1),
          package_metadata("Tool", std::nullopt, std::nullopt,
                           {"GPL-3.0-or-later"}),
          {},
          program(program_language::posix_shell, "true\n"),
          {}, std::move(lifecycle),
          architecture_requirements(
              {architecture_reference("x86_64")},
              {architecture_reference("x86_64")}),
          declaration_provenance("recipe.yml", "$", 1, 1)),
      profile_catalog::seal({}));
}

pkgsource::source_snapshot source_snapshot()
{
  return source_snapshot(
      "1.0",
      {pkgsource::lifecycle_action::pre_install,
       pkgsource::lifecycle_action::post_install});
}

pkgimage::inspected_package_image incoming_image(std::uint8_t content_seed = 1)
{
  pkgimage::package_entry entry(
      pkgimage::package_path::parse("tool"),
      pkgimage::entry_type::regular);
  entry.mode = 0755;
  entry.uid = 0;
  entry.gid = 0;
  entry.size = 4;
  entry.mtime = 10;
  entry.mtime_nanoseconds = 0;
  pkgimage::sha256_digest_bytes content{};
  content.fill(content_seed);
  entry.regular_content =
      pkgimage::regular_content_digest::from_sha256(content);

  pkgimage::package_image image({entry});
  pkgimage::sha256_digest_bytes archive{};
  archive.fill(static_cast<std::uint8_t>(content_seed + 30U));
  pkgimage::archive_inspection_receipt receipt(
      pkgimage::archive_backend_identity::parse("test/pkgctl-effect-v1"),
      pkgimage::complete_archive_digest::from_sha256(archive),
      image.identity(), image.size());
  return pkgimage::inspected_package_image(
      std::move(image), std::move(receipt));
}

pkgapply::incoming_package_authority incoming_authority(
    const pkgsource::source_snapshot& source,
    std::uint8_t content_seed = 1)
{
  const pkgbuild::build_request request = pkgbuild::build_request::seal(
      source, {}, {}, pkgsource::architecture_reference("x86_64"),
      pkgsource::architecture_reference("x86_64"),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1, 0022, 1700000000)));
  const pkgbuild::payload_manifest payload = pkgbuild::payload_manifest::seal({
      pkgbuild::payload_entry::regular(
          pkgbuild::payload_path::parse("tool"), 0755, 0, 0, 4,
          pkgbuild::payload_time{10, 0},
          pkgbuild::sha256_digest(hex_digest(content_seed))),
  });
  pkgbuild::sealed_artifact artifact = pkgbuild::sealed_artifact::make(
      pkgbuild::artifact_encoding::package_tar_v1,
      pkgbuild::artifact_compression::none, 4,
      pkgbuild::sha256_digest(hex_digest(static_cast<std::uint8_t>(content_seed + 30U))));
  pkgbuild::build_result result = pkgbuild::build_result::succeeded(
      request, payload, artifact,
      pkgbuild::execution_evidence_identity::from_sha256(hex_digest(61)));
  return pkgapply::incoming_package_authority::admit(
      std::move(result), incoming_image(content_seed));
}


pkgstate::sha256_digest_bytes state_bytes_from_hex(const std::string& hex)
{
  pkgstate::sha256_digest_bytes result{};
  const auto digit = [](char value) -> std::uint8_t {
    if (value >= '0' && value <= '9')
      return static_cast<std::uint8_t>(value - '0');
    return static_cast<std::uint8_t>(value - 'a' + 10);
  };
  for (std::size_t index = 0; index < result.size(); ++index)
    result[index] = static_cast<std::uint8_t>(
        (digit(hex[index * 2U]) << 4U) | digit(hex[index * 2U + 1U]));
  return result;
}

template<typename Identity>
Identity imported_state_identity(const std::string& hex)
{
  return Identity::from_sha256(state_bytes_from_hex(hex));
}

std::string state_digest_hex(const std::string& value)
{
  const auto separator = value.rfind(':');
  if (separator == std::string::npos)
    throw std::runtime_error("state identity lacks digest separator");
  return value.substr(separator + 1U);
}

pkgstate::lifecycle_action state_action(pkgsource::lifecycle_action action)
{
  switch (action)
  {
    case pkgsource::lifecycle_action::pre_install:
      return pkgstate::lifecycle_action::pre_install;
    case pkgsource::lifecycle_action::post_install:
      return pkgstate::lifecycle_action::post_install;
    case pkgsource::lifecycle_action::pre_remove:
      return pkgstate::lifecycle_action::pre_remove;
    case pkgsource::lifecycle_action::post_remove:
      return pkgstate::lifecycle_action::post_remove;
  }
  return pkgstate::lifecycle_action::pre_install;
}

pkgstate::installed_object_metadata state_regular(std::uint8_t content)
{
  return pkgstate::installed_object_metadata(
      pkgstate::owned_object_kind::regular, 0755, 0, 0,
      pkgstate::installed_object_timestamp(10, 0), std::uint64_t{4},
      state_identity<pkgstate::installed_regular_content_identity>(content));
}

pkgstate::installed_package installed_package(
    const pkgsource::source_snapshot& source,
    const pkgstate::state_target_binding& target,
    std::uint8_t content = 2)
{
  const auto& recipe = source.recipe();
  std::vector<pkgstate::lifecycle_program> lifecycle;
  for (const auto action : {
           pkgsource::lifecycle_action::pre_install,
           pkgsource::lifecycle_action::post_install,
           pkgsource::lifecycle_action::pre_remove,
           pkgsource::lifecycle_action::post_remove})
  {
    const auto* program = recipe.lifecycle(action);
    if (program != nullptr)
      lifecycle.emplace_back(
          state_action(action),
          pkgstate::program(pkgstate::program_language::posix_shell,
                            program->value().material()));
  }

  std::vector<pkgstate::architecture_reference> declared_build;
  for (const auto& value : recipe.architectures().build())
    declared_build.emplace_back(value.name());
  std::vector<pkgstate::architecture_reference> declared_target;
  for (const auto& value : recipe.architectures().target())
    declared_target.emplace_back(value.name());

  pkgstate::package_release release(
      imported_state_identity<pkgstate::package_release_identity>(
          recipe.release().identity().hex()),
      pkgstate::package_reference(recipe.release().package().name()),
      recipe.release().version(), recipe.release().release());
  auto source_record = pkgstate::package_source_record::make(
      std::move(release),
      pkgstate::package_metadata(
          recipe.metadata().summary(), recipe.metadata().description(),
          recipe.metadata().homepage(), recipe.metadata().licenses()),
      {}, std::move(lifecycle), {},
      pkgstate::architecture_binding::make(
          std::move(declared_build), std::move(declared_target),
          pkgstate::architecture_reference("x86_64"),
          pkgstate::architecture_reference("x86_64")),
      {},
      imported_state_identity<pkgstate::source_recipe_identity>(
          recipe.identity().hex()),
      imported_state_identity<pkgstate::source_snapshot_identity>(
          source.identity().hex()));

  auto control = pkgstate::installed_control::make(
      source_record, pkgstate::installation_reason::explicit_request(),
      pkgstate::build_provenance(
          source_record.identity(),
          state_identity<pkgstate::build_request_identity>(100),
          state_identity<pkgstate::source_material_set_identity>(101),
          state_identity<pkgstate::build_input_set_identity>(102),
          state_identity<pkgstate::environment_policy_identity>(103),
          state_identity<pkgstate::build_policy_identity>(104),
          state_identity<pkgstate::build_result_identity>(105),
          state_identity<pkgstate::payload_manifest_identity>(106),
          state_identity<pkgstate::build_artifact_identity>(107),
          state_identity<pkgstate::artifact_content_identity>(108),
          state_identity<pkgstate::artifact_binding_identity>(109),
          state_identity<pkgstate::execution_evidence_identity>(110),
          state_identity<pkgstate::artifact_image_identity>(111),
          state_identity<pkgstate::artifact_inspection_identity>(112)));
  std::vector<pkgstate::owned_entry> manifest;
  manifest.push_back(pkgstate::owned_entry::make(
      pkgstate::package_path::parse("tool"), state_regular(content),
      pkgstate::active_object_origin::incoming_payload));
  return pkgstate::installed_package::make(
      pkgstate::installation_receipt::make(
          std::move(control), target, std::move(manifest),
          state_identity<pkgstate::operation_plan_identity>(113),
          state_identity<pkgstate::application_evidence_identity>(114)));
}

pkgstate::snapshot publish_initial_package(
    pkgstate::canonical_generation_store& store,
    const pkgstate::installed_package& package)
{
  const auto empty = store.read();
  const auto request = pkgstate::state_publication_request::make(
      empty,
      {pkgstate::package_state_delta::install(
          package, package.receipt().operation_plan(),
          package.receipt().application_evidence())});
  const auto receipt = store.compare_and_publish(request);
  if (receipt.outcome() != pkgstate::state_publication_outcome::published)
    throw std::runtime_error("cannot publish initial installed fixture");
  return store.read();
}

pkgplan::filesystem_object_metadata planner_regular(std::uint8_t content)
{
  return pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::regular, 0755, 0, 0, 4,
      pkgplan::object_timestamp(10, 0),
      plan_identity<pkgplan::filesystem_regular_content_identity>(content));
}

pkgplan::installed_control_projection planner_control(
    const pkgstate::installed_control& control)
{
  std::vector<pkgplan::removal_lifecycle_declaration> lifecycle;
  for (const auto action : {
           pkgstate::lifecycle_action::pre_remove,
           pkgstate::lifecycle_action::post_remove})
  {
    const auto* item = control.source().lifecycle(action);
    if (item == nullptr)
      continue;
    lifecycle.push_back(pkgplan::removal_lifecycle_declaration::make(
        action == pkgstate::lifecycle_action::pre_remove
            ? pkgplan::removal_lifecycle_phase::pre_remove
            : pkgplan::removal_lifecycle_phase::post_remove,
        "text/x-posix-shell", item->value().material()));
  }
  pkgplan::installed_control_completeness completeness;
  completeness.runtime_dependencies = pkgplan::control_fact_availability::known;
  completeness.removal_lifecycle = pkgplan::control_fact_availability::known;
  completeness.target_profile = pkgplan::control_fact_availability::known;
  return pkgplan::installed_control_projection(
      completeness, {}, std::move(lifecycle),
      {pkgplan::target_profile_fact::make(
          "pkgsource.target-architectures", "x86_64")});
}

pkgplan::installed_package_fact planner_installed(
    const pkgstate::snapshot& expected,
    const pkgstate::installed_package& installed)
{
  const auto& release = installed.release();
  return pkgplan::installed_package_fact(
      translate_identity<pkgplan::installed_package_identity>(
          installed.identity()),
      translate_identity<pkgplan::installed_control_identity>(
          installed.control().identity()),
      translate_identity<pkgplan::installed_state_snapshot_identity>(
          expected.identity()),
      pkgplan::package_release(
          translate_identity<pkgplan::package_release_identity>(
              release.identity()),
          release.name(), release.version(), std::to_string(release.release())),
      planner_control(installed.control()));
}

pkgplan::installed_ownership_inventory planner_ownership(
    const pkgstate::snapshot& expected,
    const pkgstate::installed_package& installed)
{
  return pkgplan::installed_ownership_inventory(
      translate_identity<pkgplan::ownership_inventory_identity>(
          expected.ownership_identity()),
      translate_identity<pkgplan::installed_state_snapshot_identity>(
          expected.identity()),
      pkgplan::fact_set_completeness::complete,
      {pkgplan::installed_ownership_claim(
          pkgplan::package_path::parse("tool"),
          translate_identity<pkgplan::installed_package_identity>(
              installed.identity()),
          planner_regular(2))});
}

pkgplan::package_policy_snapshot policy()
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


pkgplan::installation_plan installation_plan(
    const pkgstate::snapshot& expected,
    const pkgapply::incoming_package_authority& incoming,
    const pkgplan::target_system_context_identity& target)
{
  const pkgplan::package_path path = pkgplan::package_path::parse("tool");
  const auto archive = incoming.image().receipt().archive_digest();
  pkgplan::installation_request request(
      incoming.candidate(),
      pkgplan::artifact_package_fact(
          translate_identity<pkgplan::artifact_identity>(archive),
          plan_identity<pkgplan::artifact_manifest_identity>(73),
          incoming.candidate().release()),
      archive, incoming.image(),
      translate_identity<pkgplan::installed_state_snapshot_identity>(
          expected.identity()),
      pkgplan::installed_ownership_inventory(
          translate_identity<pkgplan::ownership_inventory_identity>(
              expected.ownership_identity()),
          translate_identity<pkgplan::installed_state_snapshot_identity>(
              expected.identity()),
          pkgplan::fact_set_completeness::complete, {}),
      target,
      pkgplan::target_observation_set(
          plan_identity<pkgplan::observation_set_identity>(61), target,
          pkgplan::fact_set_completeness::complete,
          {pkgplan::target_path_observation::absent(path)}),
      plan_identity<pkgplan::runtime_dependency_closure_identity>(62),
      policy());
  const auto result = pkgplan::plan_install(request);
  if (!result.plan())
    throw std::runtime_error("cannot construct installation plan fixture");
  return *result.plan();
}


pkgplan::upgrade_plan upgrade_plan(
    const pkgstate::snapshot& expected,
    const pkgstate::installed_package& installed,
    const pkgapply::incoming_package_authority& incoming,
    const pkgplan::target_system_context_identity& target)
{
  const auto path = pkgplan::package_path::parse("tool");
  const auto archive = incoming.image().receipt().archive_digest();
  pkgplan::upgrade_request request(
      planner_installed(expected, installed), incoming.candidate(),
      pkgplan::artifact_package_fact(
          translate_identity<pkgplan::artifact_identity>(archive),
          plan_identity<pkgplan::artifact_manifest_identity>(77),
          incoming.candidate().release()),
      archive, incoming.image(),
      translate_identity<pkgplan::installed_state_snapshot_identity>(
          expected.identity()),
      planner_ownership(expected, installed), target,
      pkgplan::target_observation_set(
          plan_identity<pkgplan::observation_set_identity>(78), target,
          pkgplan::fact_set_completeness::complete,
          {pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(path, planner_regular(2)))}),
      plan_identity<pkgplan::runtime_dependency_closure_identity>(79),
      policy());
  const auto result = pkgplan::plan_upgrade(request);
  if (!result.plan())
    throw std::runtime_error("cannot construct upgrade plan fixture");
  return *result.plan();
}


pkgplan::removal_plan removal_plan(
    const pkgstate::snapshot& expected,
    const pkgstate::installed_package& installed,
    const pkgplan::target_system_context_identity& target)
{
  const auto path = pkgplan::package_path::parse("tool");
  pkgplan::removal_request request(
      planner_installed(expected, installed),
      translate_identity<pkgplan::installed_state_snapshot_identity>(
          expected.identity()),
      planner_ownership(expected, installed), target,
      pkgplan::target_observation_set(
          plan_identity<pkgplan::observation_set_identity>(80), target,
          pkgplan::fact_set_completeness::complete,
          {pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(path, planner_regular(2)))}),
      policy());
  const auto result = pkgplan::plan_removal(request);
  if (!result.plan())
    throw std::runtime_error("cannot construct removal plan fixture");
  return *result.plan();
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
      apply_identity<pkgapply::execution_capability_profile_identity>(18),
      apply_identity<pkgapply::lifecycle_executor_identity>(19));
}

pkgapply::application_execution_control execution_control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::best_effort,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

std::vector<pkgplan::installed_package_identity> planner_owners(
    const pkgstate::snapshot& expected,
    const pkgplan::package_path& path)
{
  std::vector<pkgplan::installed_package_identity> owners;
  const auto state_path = pkgstate::package_path::parse(path.string());
  for (const auto* owner : expected.owners(state_path))
    owners.push_back(translate_identity<pkgplan::installed_package_identity>(
        owner->identity()));
  return owners;
}

template<typename Plan>
pkgapply::lease_bound_state_projection application_projection(
    const pkgstate::snapshot& expected,
    const Plan& plan)
{
  std::vector<pkgapply::projected_path_owners> paths;
  for (const auto& item : plan.preconditions().paths())
    paths.emplace_back(item.path(), planner_owners(expected, item.path()));
  return pkgapply::lease_bound_state_projection::make(
      apply_identity<pkgapply::mutation_lease_instance_identity>(30),
      plan.preconditions().installed_snapshot(),
      plan.preconditions().ownership_inventory(),
      pkgapply::state_projection_completeness::complete,
      std::move(paths),
      apply_identity<pkgapply::state_projection_evidence_identity>(31));
}

pkgapply::completed_object_fact completed_regular(
    const pkgplan::package_path& path,
    std::uint8_t content = 1)
{
  return pkgapply::completed_object_fact(
      path, pkgapply::completed_object_kind::regular,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(4),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::known(
          {10, 0}),
      pkgapply::qualified_fact<pkgapply::completed_regular_content_identity>::
          known(apply_identity<pkgapply::completed_regular_content_identity>(content)),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_device_number>::
          not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_hardlink_relation>::unknown(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
}

pkgapply::application_durability_profile durability()
{
  using D = pkgapply::application_durability_domain;
  using S = pkgapply::application_durability_status;
  return pkgapply::application_durability_profile({
      {D::journal, S::confirmed},
      {D::incoming_staging, S::confirmed},
      {D::recovery_staging, S::confirmed},
      {D::active_namespace, S::confirmed},
      {D::rejected_object_store, S::confirmed},
      {D::completed_evidence, S::confirmed},
  });
}

pkgapply::completed_application_evidence completed_evidence(
    const pkgapply::installation_application_request& request,
    const pkgapply::lease_bound_state_projection& projection)
{
  const auto& decision = request.plan().paths().front();
  pkgapply::application_path_consequence consequence(
      decision.path(),
      decision.role() == pkgplan::installation_path_role::incoming_entry
          ? pkgapply::application_path_role::incoming_entry
          : pkgapply::application_path_role::structural_parent,
      decision.active(), decision.rejected(), decision.incoming_entry(),
      decision.ownership(), pkgapply::application_effect_status::completed,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(decision.path()),
      pkgapply::application_path_observation::present(
          completed_regular(decision.path())),
      std::nullopt, pkgapply::ownership_publication_status::eligible);
  return pkgapply::completed_application_evidence::installation(
      request,
      apply_identity<pkgapply::application_attempt_identity>(40),
      projection.identity(),
      apply_identity<pkgapply::application_journal_identity>(41),
      {std::move(consequence)}, durability());
}


pkgapply::application_path_role application_role(
    pkgplan::upgrade_path_role role)
{
  switch (role)
  {
    case pkgplan::upgrade_path_role::incoming_entry:
      return pkgapply::application_path_role::incoming_entry;
    case pkgplan::upgrade_path_role::obsolete_old_path:
      return pkgapply::application_path_role::obsolete_old_path;
    case pkgplan::upgrade_path_role::structural_parent:
      return pkgapply::application_path_role::structural_parent;
  }
  return pkgapply::application_path_role::incoming_entry;
}

pkgapply::completed_application_evidence completed_evidence(
    const pkgapply::upgrade_application_request& request,
    const pkgapply::lease_bound_state_projection& projection)
{
  std::vector<pkgapply::application_path_consequence> consequences;
  consequences.reserve(request.plan().paths().size());
  for (const auto& decision : request.plan().paths())
  {
    consequences.emplace_back(
        decision.path(), application_role(decision.role()),
        decision.active(), decision.rejected(), decision.incoming_entry(),
        decision.ownership(), pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::present(
            completed_regular(decision.path(), 2)),
        pkgapply::application_path_observation::present(
            completed_regular(decision.path(), 3)),
        std::nullopt, pkgapply::ownership_publication_status::eligible);
  }
  return pkgapply::completed_application_evidence::upgrade(
      request, apply_identity<pkgapply::application_attempt_identity>(42),
      projection.identity(),
      apply_identity<pkgapply::application_journal_identity>(43),
      std::move(consequences), durability());
}


pkgapply::completed_application_evidence completed_evidence(
    const pkgapply::removal_application_request& request,
    const pkgapply::lease_bound_state_projection& projection)
{
  std::vector<pkgapply::application_path_consequence> consequences;
  consequences.reserve(request.plan().paths().size());
  for (const auto& decision : request.plan().paths())
  {
    consequences.emplace_back(
        decision.path(), pkgapply::application_path_role::installed_owned_path,
        decision.active(), decision.rejected(), std::nullopt,
        decision.ownership(), pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::present(
            completed_regular(decision.path(), 2)),
        pkgapply::application_path_observation::absent(decision.path()),
        std::nullopt, pkgapply::ownership_publication_status::eligible);
  }
  return pkgapply::completed_application_evidence::removal(
      request, apply_identity<pkgapply::application_attempt_identity>(44),
      projection.identity(),
      apply_identity<pkgapply::application_journal_identity>(45),
      std::move(consequences), durability());
}

pkgcatalog::catalog_snapshot catalog_snapshot(
    const pkgsource::source_snapshot& source)
{
  pkgcatalog::collection_declaration declaration(
      pkgcatalog::collection_reference("core"),
      pkgcatalog::collection_provenance(
          "/collection", std::nullopt,
          pkgsource::declaration_provenance(
              "<test>", "collections[0]", 1, 1)),
      {source});
  std::vector<pkgcatalog::catalog_collection> collections;
  collections.emplace_back(0, pkgcatalog::seal_collection(std::move(declaration)));
  return pkgcatalog::catalog_snapshot::seal(
      pkgsource::profile_catalog::seal({}), std::move(collections));
}

pkgctl::transaction_session transaction_session(
    const pkgsource::source_snapshot& source,
    const pkgstate::snapshot& installed,
    const std::filesystem::path& state_path,
    const std::vector<pkgsource::lifecycle_action>& lifecycle_actions)
{
  auto catalog = catalog_snapshot(source);
  std::vector<pkgcatalog::acquire::collection_specification> specifications;
  specifications.emplace_back(
      0, pkgcatalog::collection_reference("core"),
      std::filesystem::path("/collection"), std::nullopt,
      pkgsource::declaration_provenance(
          "<test>", "collections[0]", 1, 1));
  auto catalog_request = pkgctl::catalog_request::make(std::move(specifications));
  auto catalog_session = pkgctl::catalog_session::seal(
      catalog_request, catalog);

  std::vector<pkgresolve::resolution_goal> goals;
  goals.emplace_back(
      pkgsource::requirement_scope::run(),
      pkgsource::requirement_subject(pkgsource::package_reference("tool")),
      "<test>");
  for (const auto action : lifecycle_actions)
    goals.emplace_back(
        pkgsource::requirement_scope::lifecycle(action),
        pkgsource::requirement_subject(pkgsource::package_reference("tool")),
        "<test>");
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

  const auto& candidate = catalog.require(pkgsource::package_reference("tool"));
  const auto selection_id =
      source_identity<pkgresolve::package_selection_identity>(70);
  const auto installed_selection_id =
      source_identity<pkgresolve::package_selection_identity>(72);
  pkgresolve::selected_package selection(
      pkgresolve::resolution_environment::target, architectures,
      pkgresolve::selection_authority(candidate), candidate.release(),
      candidate.source().identity(), selection_id);

  const auto* installed_package = installed.find_package("tool");
  const bool needs_installed_lifecycle = std::any_of(
      lifecycle_actions.begin(), lifecycle_actions.end(), [](const auto action) {
        return action == pkgsource::lifecycle_action::pre_remove ||
               action == pkgsource::lifecycle_action::post_remove;
      });
  if (needs_installed_lifecycle && installed_package == nullptr)
    throw std::runtime_error("removal lifecycle fixture lacks installed authority");

  std::vector<pkgresolve::selected_package> selections;
  selections.push_back(selection);
  if (needs_installed_lifecycle)
  {
    const auto& release = installed_package->release();
    selections.emplace_back(
        pkgresolve::resolution_environment::target, architectures,
        pkgresolve::selection_authority(*installed_package),
        pkgsource::package_release(
            pkgsource::package_reference(release.name()), release.version(),
            release.release()),
        pkgsource::source_snapshot_identity::from_sha256(
            state_digest_hex(
                installed_package->control().source().snapshot().string())),
        installed_selection_id);
  }

  std::vector<pkgresolve::resolved_goal> resolved_goals;
  std::vector<pkgresolve::selection_reason> reasons;
  for (std::size_t index = 0; index < goals.size(); ++index)
  {
    const auto& scope = goals[index].scope();
    const bool removal_lifecycle =
        scope.kind() == pkgsource::requirement_scope_kind::lifecycle &&
        (*scope.action() == pkgsource::lifecycle_action::pre_remove ||
         *scope.action() == pkgsource::lifecycle_action::post_remove);
    const auto& goal_selection =
        removal_lifecycle ? installed_selection_id : selection_id;
    resolved_goals.emplace_back(
        goals[index],
        std::vector<pkgresolve::goal_member>{pkgresolve::goal_member(
            pkgsource::package_reference("tool"), goal_selection,
            std::nullopt, {})},
        std::vector<pkgresolve::package_selection_identity>{goal_selection},
        std::vector<pkgresolve::requirement_edge_identity>{},
        source_identity<pkgresolve::goal_closure_identity>(
            static_cast<std::uint8_t>(80U + index)));
    reasons.emplace_back(
        goal_selection, pkgresolve::selection_reason_kind::direct_goal,
        scope, std::nullopt, std::nullopt, std::nullopt);
  }
  pkgresolve::resolution_result result(
      native_request, std::move(selections), {}, std::move(resolved_goals),
      std::move(reasons),
      source_identity<pkgresolve::resolution_result_identity>(71));
  auto resolution_session = pkgctl::resolution_session::seal(
      controller_request, catalog_session, installed, result);

  auto transaction_request = pkgctl::transaction_request::make(
      controller_request);
  auto native_transaction_request = pkgtransaction::transaction_request::seal(
      result, transaction_request.convergence());
  auto program = pkgtransaction::compose(std::move(native_transaction_request));
  return pkgctl::transaction_session::seal(
      std::move(transaction_request), std::move(resolution_session),
      std::move(program));
}


pkgctl::transaction_session transaction_session(
    const pkgsource::source_snapshot& source,
    const pkgstate::snapshot& installed,
    const std::filesystem::path& state_path)
{
  return transaction_session(
      source, installed, state_path,
      {pkgsource::lifecycle_action::pre_install,
       pkgsource::lifecycle_action::post_install});
}


pkgctl::transaction_session removal_transaction_session(
    const pkgsource::source_snapshot& source,
    const pkgstate::snapshot& installed,
    const std::filesystem::path& state_path)
{
  auto catalog = catalog_snapshot(source);
  std::vector<pkgcatalog::acquire::collection_specification> specifications;
  specifications.emplace_back(
      0, pkgcatalog::collection_reference("core"),
      std::filesystem::path("/collection"), std::nullopt,
      pkgsource::declaration_provenance(
          "<test>", "collections[0]", 1, 1));
  auto catalog_request = pkgctl::catalog_request::make(std::move(specifications));
  auto catalog_session = pkgctl::catalog_session::seal(catalog_request, catalog);

  std::vector<pkgresolve::resolution_goal> goals;
  for (const auto action : {
           pkgsource::lifecycle_action::pre_remove,
           pkgsource::lifecycle_action::post_remove})
    goals.emplace_back(
        pkgsource::requirement_scope::lifecycle(action),
        pkgsource::requirement_subject(pkgsource::package_reference("tool")),
        "<test>");

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

  const auto* package = installed.find_package("tool");
  if (package == nullptr)
    throw std::runtime_error("removal fixture lacks installed package");
  const auto selection_id =
      source_identity<pkgresolve::package_selection_identity>(73);
  const auto& release = package->release();
  pkgresolve::selected_package selection(
      pkgresolve::resolution_environment::target, architectures,
      pkgresolve::selection_authority(*package),
      pkgsource::package_release(
          pkgsource::package_reference(release.name()), release.version(),
          release.release()),
      pkgsource::source_snapshot_identity::from_sha256(
          state_digest_hex(package->control().source().snapshot().string())),
      selection_id);

  std::vector<pkgresolve::resolved_goal> resolved_goals;
  std::vector<pkgresolve::selection_reason> reasons;
  for (std::size_t index = 0; index < goals.size(); ++index)
  {
    resolved_goals.emplace_back(
        goals[index],
        std::vector<pkgresolve::goal_member>{pkgresolve::goal_member(
            pkgsource::package_reference("tool"), selection_id,
            std::nullopt, {})},
        std::vector<pkgresolve::package_selection_identity>{selection_id},
        std::vector<pkgresolve::requirement_edge_identity>{},
        source_identity<pkgresolve::goal_closure_identity>(
            static_cast<std::uint8_t>(90U + index)));
    reasons.emplace_back(
        selection_id, pkgresolve::selection_reason_kind::direct_goal,
        goals[index].scope(), std::nullopt, std::nullopt, std::nullopt);
  }
  pkgresolve::resolution_result result(
      native_request, {selection}, {}, std::move(resolved_goals),
      std::move(reasons),
      source_identity<pkgresolve::resolution_result_identity>(74));
  auto resolution_session = pkgctl::resolution_session::seal(
      controller_request, catalog_session, installed, result);

  auto convergence = pkgtransaction::convergence_policy::remove_explicit(
      {pkgsource::package_reference("tool")});
  auto transaction_request = pkgctl::transaction_request::make(
      controller_request, convergence);
  auto native_transaction_request = pkgtransaction::transaction_request::seal(
      result, convergence);
  auto program = pkgtransaction::compose(std::move(native_transaction_request));
  return pkgctl::transaction_session::seal(
      std::move(transaction_request), std::move(resolution_session),
      std::move(program));
}

const pkgtransaction::transaction_node& action_node(
    const pkgctl::transaction_session& session,
    pkgtransaction::transaction_action_kind action)
{
  for (const auto& node : session.program().nodes())
    if (node.action() == action && node.package().name() == "tool")
      return node;
  throw std::runtime_error("transaction fixture lacks target action node");
}

const pkgtransaction::transaction_node& install_node(
    const pkgctl::transaction_session& session)
{
  return action_node(
      session, pkgtransaction::transaction_action_kind::install);
}

pkgctl::lifecycle_order operation_lifecycle_order(
    const pkgctl::transaction_session& session,
    pkgtransaction::transaction_action_kind action_kind)
{
  const auto& action = action_node(session, action_kind);
  std::vector<pkgtransaction::transaction_node_identity> before;
  std::vector<pkgtransaction::transaction_node_identity> after;
  for (const auto& edge : session.program().edges())
  {
    if (!edge.phase_order())
      continue;
    if (*edge.phase_order() ==
            pkgtransaction::phase_order_kind::pre_lifecycle_before_action &&
        edge.after() == action.identity())
      before.push_back(edge.before());
    if (*edge.phase_order() ==
            pkgtransaction::phase_order_kind::action_before_post_lifecycle &&
        edge.before() == action.identity())
      after.push_back(edge.after());
  }
  return pkgctl::lifecycle_order::make(
      std::move(before), std::move(after));
}

pkgctl::lifecycle_order operation_lifecycle_order(
    const pkgctl::transaction_session& session)
{
  return operation_lifecycle_order(
      session, pkgtransaction::transaction_action_kind::install);
}


pkgapply_exec::lifecycle_subject lifecycle_subject(
    pkgsource::lifecycle_action action)
{
  switch (action)
  {
    case pkgsource::lifecycle_action::pre_install:
    case pkgsource::lifecycle_action::post_install:
      return pkgapply_exec::lifecycle_subject::incoming;
    case pkgsource::lifecycle_action::pre_remove:
    case pkgsource::lifecycle_action::post_remove:
      return pkgapply_exec::lifecycle_subject::installed;
  }
  return pkgapply_exec::lifecycle_subject::incoming;
}

std::vector<pkgapply_exec::admitted_lifecycle_session> admit_lifecycle_sessions(
    const pkgapply::package_application_request& application,
    const pkgctl::transaction_session& transaction,
    const std::vector<pkgtransaction::transaction_node_identity>& identities,
    const std::filesystem::path& execution_root,
    const std::filesystem::path& target_root,
    const std::filesystem::path& sessions_root)
{
  std::filesystem::create_directories(sessions_root);
  const auto nodes = pkgapply_exec::derive(application);
  const auto identity = test_lifecycle_execution_identity();
  std::vector<pkgapply_exec::admitted_lifecycle_session> result;
  result.reserve(identities.size());
  for (std::size_t index = 0; index < identities.size(); ++index)
  {
    const auto* transaction_node = transaction.program().find(identities[index]);
    if (transaction_node == nullptr || !transaction_node->lifecycle())
      throw std::runtime_error("transaction lifecycle node is absent");
    const auto action = *transaction_node->lifecycle();
    const auto* node = nodes.find(lifecycle_subject(action), action);
    if (node == nullptr)
      throw std::runtime_error("application lifecycle node is absent");
    result.push_back(pkgapply_exec::admitted_lifecycle_session::admit(
        application, *node,
        {pkgexec::root_view_identity::from_sha256(hex_digest(92)),
         execution_root, application.target().root_view(), target_root,
         sessions_root / std::to_string(index)},
        identity));
  }
  return result;
}

class scripted_execution_backend final : public pkgexec::execution_backend {
public:
  explicit scripted_execution_backend(
      std::optional<pkgsource::lifecycle_action> fail = std::nullopt)
      : fail_(fail), capabilities_(pkgexec::backend_capability_profile::seal(
            pkgexec::backend_identity::from_sha256(hex_digest(90)),
            {pkgexec::execution_guarantee::exact_interpreter,
             pkgexec::execution_guarantee::closed_environment,
             pkgexec::execution_guarantee::root_view,
             pkgexec::execution_guarantee::writable_resources,
             pkgexec::execution_guarantee::fixed_credentials,
             pkgexec::execution_guarantee::network_denied,
             pkgexec::execution_guarantee::complete_stdout_capture,
             pkgexec::execution_guarantee::complete_stderr_capture,
             pkgexec::execution_guarantee::cleanup_verified}))
  {
  }

  pkgexec::backend_capability_profile capabilities() const override
  {
    return capabilities_;
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request& request,
      const pkgexec::execution_resources&) override
  {
    CHECK(capabilities_.supports(request));
    const auto action = *request.purpose().action();
    actions_.push_back(action);
    if (fail_ && *fail_ == action)
      return pkgexec::execution_result::failed_after_start(
          request, capabilities_, request.interpreter(),
          pkgexec::process_termination::exited(1),
          pkgexec::stream_capture::retained(""),
          pkgexec::stream_capture::retained("failed\n"),
          request.required_guarantees(), pkgexec::cleanup_outcome::verified,
          pkgexec::execution_failure_kind::program_exited_nonzero,
          "scripted lifecycle failure");
    return pkgexec::execution_result::succeeded(
        request, capabilities_, request.interpreter(),
        pkgexec::stream_capture::retained(""),
        pkgexec::stream_capture::retained(""),
        request.required_guarantees());
  }

  const std::vector<pkgsource::lifecycle_action>& actions() const noexcept
  {
    return actions_;
  }

private:
  std::optional<pkgsource::lifecycle_action> fail_;
  pkgexec::backend_capability_profile capabilities_;
  std::vector<pkgsource::lifecycle_action> actions_;
};

class mutation_lease final : public pkgapply::target_mutation_lease {
public:
  mutation_lease(const pkgapply::application_target_context& target,
        const pkgapply::lease_bound_state_projection& projection)
      : identity_(projection.lease()), target_(target.identity()),
        domain_(target.mutation_exclusion_domain())
  {
  }
  const pkgapply::mutation_lease_instance_identity& identity() const noexcept override
  { return identity_; }
  const pkgapply::application_target_context_identity& target() const noexcept override
  { return target_; }
  const pkgapply::mutation_exclusion_domain_identity& exclusion_domain() const noexcept override
  { return domain_; }
  bool held() const noexcept override { return held_; }
  void release() noexcept { held_ = false; }
private:
  pkgapply::mutation_lease_instance_identity identity_;
  pkgapply::application_target_context_identity target_;
  pkgapply::mutation_exclusion_domain_identity domain_;
  bool held_ = true;
};

enum class publication_mode {
  native,
  rejected,
  indeterminate,
};

enum class lease_release_point {
  never,
  after_application,
  during_publication,
};

class driver final : public pkgctl::transaction_effect_driver {
public:
  driver(pkgapply::lease_bound_state_projection projection,
         mutation_lease& outer_lease,
         pkgapply::application_receipt application,
         pkgstate::canonical_store& store,
         std::optional<pkgsource::lifecycle_action> fail_lifecycle =
             std::nullopt,
         lease_release_point release = lease_release_point::never,
         publication_mode publication = publication_mode::native)
      : projection_(std::move(projection)), lease_(outer_lease),
        application_(std::move(application)), store_(store),
        backend_(fail_lifecycle), release_(release),
        publication_(publication)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override { return lease_; }
  const pkgapply::lease_bound_state_projection& state_projection() const noexcept override
  { return projection_; }
  pkgapply_exec::lifecycle_execution_result execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override
  {
    trace_.push_back(std::string(pkgsource::to_string(session.node().action())));
    return pkgapply_exec::execute(session, backend_);
  }
  pkgapply::application_receipt apply_application(
      const pkgapply::package_application_request&) override
  {
    trace_.push_back("apply");
    if (release_ == lease_release_point::after_application)
      lease_.release();
    return application_;
  }
  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    trace_.push_back("publish");
    ++publication_calls_;
    if (release_ == lease_release_point::during_publication)
      lease_.release();
    if (publication_ == publication_mode::rejected)
      return pkgstate::state_publication_receipt::request_rejected(
          request, store_.read(), "test/pkgctl-effect-v1");
    if (publication_ == publication_mode::indeterminate)
      return pkgstate::state_publication_receipt::indeterminate(
          request, store_.read(), std::nullopt, "test/pkgctl-effect-v1",
          pkgstate::state_storage_atomicity_boundary::immutable_generation_selection);
    return store_.compare_and_publish(request);
  }
  std::size_t publication_calls() const noexcept { return publication_calls_; }
  const std::vector<std::string>& trace() const noexcept { return trace_; }
private:
  pkgapply::lease_bound_state_projection projection_;
  mutation_lease& lease_;
  pkgapply::application_receipt application_;
  pkgstate::canonical_store& store_;
  scripted_execution_backend backend_;
  lease_release_point release_;
  publication_mode publication_;
  std::size_t publication_calls_ = 0;
  std::vector<std::string> trace_;
};

struct fixture final {
  test_support::temporary_directory temp;
  pkgstate::canonical_generation_store store;
  pkgstate::snapshot expected;
  pkgsource::source_snapshot source;
  pkgapply::incoming_package_authority incoming;
  pkgctl::transaction_session transaction;
  pkgplan::target_system_context_identity target_system;
  pkgplan::installation_plan plan;
  pkgapply::application_target_context target;
  pkgapply::installation_application_request application;
  pkgapply::lease_bound_state_projection projection;
  pkgapply::completed_application_evidence evidence;
  pkgapply::application_receipt receipt;
  mutation_lease outer_lease;
  std::vector<pkgapply_exec::admitted_lifecycle_session> before;
  std::vector<pkgapply_exec::admitted_lifecycle_session> after;

  fixture()
      : store(temp.path() / "state", test_support::binding()),
        expected(store.read()), source(source_snapshot()),
        incoming(incoming_authority(source)),
        transaction(transaction_session(source, expected, temp.path() / "state")),
        target_system(plan_identity<pkgplan::target_system_context_identity>(60)),
        plan(installation_plan(expected, incoming, target_system)),
        target(application_target(expected.target_binding(), target_system)),
        application(pkgapply::installation_application_request::make(
            plan, incoming, target, execution_control())),
        projection(application_projection(expected, plan)),
        evidence(completed_evidence(application, projection)),
        receipt(pkgapply::application_receipt::completed(
            evidence, pkgapply::application_recovery_state::unchanged)),
        outer_lease(target, projection)
  {
    const auto root = temp.path();
    const auto execution_root = root / "execution-root";
    const auto target_root = root / "target-root";
    const auto sessions = root / "sessions";
    std::filesystem::create_directory(execution_root);
    std::filesystem::create_directory(target_root);
    std::filesystem::create_directory(sessions);

    const auto nodes = pkgapply_exec::derive(
        pkgapply::package_application_request(application));
    const auto* pre = nodes.find(
        pkgapply_exec::lifecycle_subject::incoming,
        pkgsource::lifecycle_action::pre_install);
    const auto* post = nodes.find(
        pkgapply_exec::lifecycle_subject::incoming,
        pkgsource::lifecycle_action::post_install);
    if (pre == nullptr || post == nullptr)
      throw std::runtime_error("lifecycle fixture lacks derived nodes");

    const auto identity = test_lifecycle_execution_identity();
    before.push_back(pkgapply_exec::admitted_lifecycle_session::admit(
        pkgapply::package_application_request(application), *pre,
        {pkgexec::root_view_identity::from_sha256(hex_digest(92)),
         execution_root, target.root_view(), target_root,
         sessions / "pre"},
        identity));
    after.push_back(pkgapply_exec::admitted_lifecycle_session::admit(
        pkgapply::package_application_request(application), *post,
        {pkgexec::root_view_identity::from_sha256(hex_digest(92)),
         execution_root, target.root_view(), target_root,
         sessions / "post"},
        identity));
  }
};


struct upgrade_fixture final {
  test_support::temporary_directory temp;
  pkgstate::canonical_generation_store store;
  pkgsource::source_snapshot old_source;
  pkgstate::installed_package old_package;
  pkgstate::snapshot expected;
  pkgsource::source_snapshot source;
  pkgapply::incoming_package_authority incoming;
  pkgctl::transaction_session transaction;
  pkgplan::target_system_context_identity target_system;
  pkgplan::upgrade_plan plan;
  pkgapply::application_target_context target;
  pkgapply::upgrade_application_request application;
  pkgapply::lease_bound_state_projection projection;
  pkgapply::completed_application_evidence evidence;
  pkgapply::application_receipt receipt;
  mutation_lease outer_lease;
  std::vector<pkgapply_exec::admitted_lifecycle_session> before;
  std::vector<pkgapply_exec::admitted_lifecycle_session> after;

  upgrade_fixture()
      : store(temp.path() / "state", test_support::binding()),
        old_source(source_snapshot(
            "1.0",
            {pkgsource::lifecycle_action::pre_remove,
             pkgsource::lifecycle_action::post_remove})),
        old_package(installed_package(
            old_source, store.read().target_binding(), 2)),
        expected(publish_initial_package(store, old_package)),
        source(source_snapshot(
            "2.0",
            {pkgsource::lifecycle_action::pre_install,
             pkgsource::lifecycle_action::post_install})),
        incoming(incoming_authority(source, 3)),
        transaction(transaction_session(
            source, expected, temp.path() / "state",
            {pkgsource::lifecycle_action::pre_remove,
             pkgsource::lifecycle_action::post_remove,
             pkgsource::lifecycle_action::pre_install,
             pkgsource::lifecycle_action::post_install})),
        target_system(plan_identity<pkgplan::target_system_context_identity>(60)),
        plan(upgrade_plan(expected, old_package, incoming, target_system)),
        target(application_target(expected.target_binding(), target_system)),
        application(pkgapply::upgrade_application_request::make(
            plan, incoming, target, execution_control())),
        projection(application_projection(expected, plan)),
        evidence(completed_evidence(application, projection)),
        receipt(pkgapply::application_receipt::completed(
            evidence, pkgapply::application_recovery_state::unchanged)),
        outer_lease(target, projection)
  {
    const auto execution_root = temp.path() / "execution-root";
    const auto target_root = temp.path() / "target-root";
    const auto sessions = temp.path() / "sessions";
    std::filesystem::create_directory(execution_root);
    std::filesystem::create_directory(target_root);
    std::filesystem::create_directory(sessions);

    const auto order = operation_lifecycle_order(
        transaction, pkgtransaction::transaction_action_kind::upgrade);
    before = admit_lifecycle_sessions(
        pkgapply::package_application_request(application), transaction,
        order.before(), execution_root, target_root, sessions / "before");
    after = admit_lifecycle_sessions(
        pkgapply::package_application_request(application), transaction,
        order.after(), execution_root, target_root, sessions / "after");
  }
};

pkgctl::effectful_operation_request effect_request(
    const upgrade_fixture& value)
{
  return pkgctl::effectful_operation_request::make(
      value.transaction,
      action_node(value.transaction,
                  pkgtransaction::transaction_action_kind::upgrade)
          .identity(),
      pkgapply::package_application_request(value.application),
      operation_lifecycle_order(
          value.transaction,
          pkgtransaction::transaction_action_kind::upgrade));
}

void check_upgrade_success()
{
  upgrade_fixture value;
  const auto request = effect_request(value);
  const auto session = pkgctl::effectful_operation_session::admit(
      request, value.before, value.after);
  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store);
  const auto result = pkgctl::execute_effectful_operation(session, actuator);
  CHECK(result.succeeded());
  CHECK(result.before().size() == 2);
  CHECK(result.after().size() == 2);
  CHECK(actuator.trace() == std::vector<std::string>(
      {"pre-remove", "pre-install", "apply",
       "post-remove", "post-install", "publish"}));
  const auto installed = value.store.read();
  CHECK(installed.size() == 1);
  const auto* package = installed.find_package("tool");
  CHECK(package != nullptr);
  CHECK(package && package->release().version() == "2.0");
  CHECK(package && package->receipt().transaction_evidence() ==
                       result.transaction_evidence());
  CHECK(package && package->control().reason() ==
                       value.old_package.control().reason());
}


struct removal_fixture final {
  test_support::temporary_directory temp;
  pkgstate::canonical_generation_store store;
  pkgsource::source_snapshot source;
  pkgstate::installed_package old_package;
  pkgstate::snapshot expected;
  pkgctl::transaction_session transaction;
  pkgplan::target_system_context_identity target_system;
  pkgplan::removal_plan plan;
  pkgapply::application_target_context target;
  pkgapply::removal_application_request application;
  pkgapply::lease_bound_state_projection projection;
  pkgapply::completed_application_evidence evidence;
  pkgapply::application_receipt receipt;
  mutation_lease outer_lease;
  std::vector<pkgapply_exec::admitted_lifecycle_session> before;
  std::vector<pkgapply_exec::admitted_lifecycle_session> after;

  removal_fixture()
      : store(temp.path() / "state", test_support::binding()),
        source(source_snapshot(
            "1.0",
            {pkgsource::lifecycle_action::pre_remove,
             pkgsource::lifecycle_action::post_remove})),
        old_package(installed_package(
            source, store.read().target_binding(), 2)),
        expected(publish_initial_package(store, old_package)),
        transaction(removal_transaction_session(
            source, expected, temp.path() / "state")),
        target_system(plan_identity<pkgplan::target_system_context_identity>(60)),
        plan(removal_plan(expected, old_package, target_system)),
        target(application_target(expected.target_binding(), target_system)),
        application(pkgapply::removal_application_request::make(
            plan, target, execution_control())),
        projection(application_projection(expected, plan)),
        evidence(completed_evidence(application, projection)),
        receipt(pkgapply::application_receipt::completed(
            evidence, pkgapply::application_recovery_state::unchanged)),
        outer_lease(target, projection)
  {
    const auto execution_root = temp.path() / "execution-root";
    const auto target_root = temp.path() / "target-root";
    const auto sessions = temp.path() / "sessions";
    std::filesystem::create_directory(execution_root);
    std::filesystem::create_directory(target_root);
    std::filesystem::create_directory(sessions);

    const auto order = operation_lifecycle_order(
        transaction, pkgtransaction::transaction_action_kind::remove);
    before = admit_lifecycle_sessions(
        pkgapply::package_application_request(application), transaction,
        order.before(), execution_root, target_root, sessions / "before");
    after = admit_lifecycle_sessions(
        pkgapply::package_application_request(application), transaction,
        order.after(), execution_root, target_root, sessions / "after");
  }
};

pkgctl::effectful_operation_request effect_request(
    const removal_fixture& value)
{
  return pkgctl::effectful_operation_request::make(
      value.transaction,
      action_node(value.transaction,
                  pkgtransaction::transaction_action_kind::remove)
          .identity(),
      pkgapply::package_application_request(value.application),
      operation_lifecycle_order(
          value.transaction,
          pkgtransaction::transaction_action_kind::remove));
}

void check_removal_success()
{
  removal_fixture value;
  const auto request = effect_request(value);
  const auto session = pkgctl::effectful_operation_session::admit(
      request, value.before, value.after);
  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store);
  const auto result = pkgctl::execute_effectful_operation(session, actuator);
  CHECK(result.succeeded());
  CHECK(result.before().size() == 1);
  CHECK(result.after().size() == 1);
  CHECK(actuator.trace() == std::vector<std::string>(
      {"pre-remove", "apply", "post-remove", "publish"}));
  CHECK(value.store.read().packages().empty());
  CHECK(result.transaction_evidence().has_value());
  CHECK(result.publication_request().has_value());
  CHECK(result.publication_request()->transaction_evidence() ==
        result.transaction_evidence());
}

pkgctl::effectful_operation_request effect_request(const fixture& value)
{
  return pkgctl::effectful_operation_request::make(
      value.transaction, install_node(value.transaction).identity(),
      pkgapply::package_application_request(value.application),
      operation_lifecycle_order(value.transaction),
      pkgstate::installation_reason::explicit_request());
}

void check_success()
{
  fixture value;
  const auto request = effect_request(value);
  const auto repeated = effect_request(value);
  CHECK(request.identity() == repeated.identity());
  const auto session = pkgctl::effectful_operation_session::admit(
      request, value.before, value.after);

  const auto relocate = [&](
      const pkgapply_exec::admitted_lifecycle_session& original,
      const std::string& label) {
    const auto& paths = original.paths();
    return pkgapply_exec::admitted_lifecycle_session::admit(
        original.request(), original.node(),
        {paths.execution_root, value.temp.path() / ("alternate-exec-" + label),
         paths.target_root, value.temp.path() / ("alternate-target-" + label),
         value.temp.path() / ("alternate-session-" + label)},
        original.execution_identity());
  };
  std::vector<pkgapply_exec::admitted_lifecycle_session> alternate_before;
  std::vector<pkgapply_exec::admitted_lifecycle_session> alternate_after;
  alternate_before.push_back(relocate(value.before.front(), "before"));
  alternate_after.push_back(relocate(value.after.front(), "after"));
  const auto alternate = pkgctl::effectful_operation_session::admit(
      request, std::move(alternate_before), std::move(alternate_after));
  CHECK(alternate.identity() == session.identity());

  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store);
  const auto result = pkgctl::execute_effectful_operation(session, actuator);
  CHECK(result.succeeded());
  CHECK(result.outcome() == pkgctl::effectful_operation_outcome::completed);
  CHECK(result.application().has_value());
  CHECK(result.transaction_evidence().has_value());
  CHECK(result.publication_request().has_value());
  CHECK(result.publication_receipt().has_value());
  CHECK(actuator.publication_calls() == 1);
  CHECK(actuator.trace() == std::vector<std::string>(
      {"pre-install", "apply", "post-install", "publish"}));
  const auto installed = value.store.read();
  CHECK(installed.size() == 1);
  const auto* package = installed.find_package("tool");
  CHECK(package != nullptr);
  CHECK(package && package->receipt().transaction_evidence() ==
                       result.transaction_evidence());
}

void check_request_refusal()
{
  fixture value;
  const auto& program = value.transaction.program();
  const pkgtransaction::transaction_node* build = nullptr;
  for (const auto& node : program.nodes())
    if (node.action() == pkgtransaction::transaction_action_kind::build)
      build = &node;
  CHECK(build != nullptr);

  bool refused = false;
  try
  {
    (void)pkgctl::effectful_operation_request::make(
        value.transaction, build->identity(),
        pkgapply::package_application_request(value.application),
        pkgctl::lifecycle_order::make({}, {}),
        pkgstate::installation_reason::explicit_request());
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_effect_request;
  }
  CHECK(refused);

  refused = false;
  try
  {
    (void)pkgctl::effectful_operation_request::make(
        value.transaction, install_node(value.transaction).identity(),
        pkgapply::package_application_request(value.application),
        pkgctl::lifecycle_order::make({}, {}),
        pkgstate::installation_reason::explicit_request());
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_effect_request;
  }
  CHECK(refused);

  refused = false;
  try
  {
    (void)pkgctl::effectful_operation_request::make(
        value.transaction, install_node(value.transaction).identity(),
        pkgapply::package_application_request(value.application),
        operation_lifecycle_order(value.transaction));
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_effect_request;
  }
  CHECK(refused);

  refused = false;
  try
  {
    (void)pkgctl::effectful_operation_session::admit(
        effect_request(value), {}, value.after);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_effect_session;
  }
  CHECK(refused);
}

void check_application_failure()
{
  fixture value;
  auto failed = pkgapply::application_receipt::failed(
      value.application,
      apply_identity<pkgapply::application_attempt_identity>(50),
      value.projection.identity(),
      pkgapply::application_attempt_outcome::failed_before_target_mutation,
      pkgapply::application_recovery_state::unchanged,
      durability(), {}, std::nullopt);
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  driver actuator(value.projection, value.outer_lease,
                  std::move(failed), value.store);
  const auto result = pkgctl::execute_effectful_operation(session, actuator);
  CHECK(result.outcome() ==
        pkgctl::effectful_operation_outcome::application_not_completed);
  CHECK(!result.succeeded());
  CHECK(actuator.publication_calls() == 0);
  CHECK(actuator.trace() == std::vector<std::string>(
      {"pre-install", "apply"}));
  CHECK(value.store.read().packages().empty());
}

void check_lifecycle_failures()
{
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        pkgsource::lifecycle_action::pre_install);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::
              lifecycle_failed_before_application);
    CHECK(!result.application().has_value());
    CHECK(actuator.publication_calls() == 0);
    CHECK(actuator.trace() == std::vector<std::string>({"pre-install"}));
  }
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        pkgsource::lifecycle_action::post_install);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::
              lifecycle_failed_after_application);
    CHECK(result.application().has_value());
    CHECK(actuator.publication_calls() == 0);
    CHECK(actuator.trace() == std::vector<std::string>(
        {"pre-install", "apply", "post-install"}));
  }
}

void check_publication_failures()
{
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::never,
        publication_mode::rejected);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::state_publication_not_completed);
    CHECK(!result.succeeded());
    CHECK(result.transaction_evidence().has_value());
    CHECK(result.publication_request().has_value());
    CHECK(result.publication_receipt().has_value());
    CHECK(actuator.publication_calls() == 1);
    CHECK(value.store.read().packages().empty());
  }
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::never,
        publication_mode::indeterminate);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::state_publication_indeterminate);
    CHECK(!result.succeeded());
    CHECK(result.publication_receipt().has_value());
    CHECK(result.publication_receipt()->outcome() ==
          pkgstate::state_publication_outcome::indeterminate);
    CHECK(actuator.publication_calls() == 1);
  }
}

void check_lease_loss()
{
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(value.projection, value.outer_lease,
                    value.receipt, value.store, std::nullopt,
                    lease_release_point::after_application);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::outer_lease_lost);
    CHECK(result.application().has_value());
    CHECK(!result.publication_request().has_value());
    CHECK(actuator.publication_calls() == 0);
    CHECK(actuator.trace() == std::vector<std::string>(
        {"pre-install", "apply"}));
    CHECK(value.store.read().packages().empty());
  }
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(value.projection, value.outer_lease,
                    value.receipt, value.store, std::nullopt,
                    lease_release_point::during_publication);
    const auto result = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::outer_lease_lost);
    CHECK(result.publication_request().has_value());
    CHECK(result.publication_receipt().has_value());
    CHECK(result.publication_receipt()->outcome() ==
          pkgstate::state_publication_outcome::published);
    CHECK(actuator.publication_calls() == 1);
  }
}

} // namespace

int main()
{
  check_success();
  check_upgrade_success();
  check_removal_success();
  check_request_refusal();
  check_application_failure();
  check_lifecycle_failures();
  check_publication_failures();
  check_lease_loss();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
