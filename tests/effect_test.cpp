// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"
#include "run_execute_support.h"

#include <pkgctl/effect.h>
#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/error.h>
#include <pkgctl/preparation.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_native.h>
#include <pkgctl/run_journal_codec.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_advance.h>
#include <pkgctl/run_drive.h>
#include <pkgctl/run_commit.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_progress.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_store.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include <libpkgapply-posix/mutation_lease.h>
#include <libpkgbuild/libpkgbuild.h>
#include <libpkgbuild-image/libpkgbuild-image.h>
#include <libpkgbuild-plan/libpkgbuild-plan.h>
#include <libpkgcatalog/collection.h>
#include <libpkgimage/inspection_receipt.h>
#include <libpkgimage/package_entry.h>
#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>
#include <libpkgresolve/result.h>
#include <libpkgresolve/resolver.h>
#include <libpkgtransaction/composer.h>
#include <libpkgstate-posix/canonical_generation_store.h>
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

std::array<std::uint8_t, 32> identity_bytes(
    const pkgctl::session_identity& identity)
{
  const auto text = identity.hex();
  const auto digit = [](char value) -> std::uint8_t {
    if (value >= '0' && value <= '9')
      return static_cast<std::uint8_t>(value - '0');
    return static_cast<std::uint8_t>(value - 'a' + 10);
  };
  std::array<std::uint8_t, 32> result{};
  for (std::size_t index = 0U; index < result.size(); ++index)
  {
    result[index] = static_cast<std::uint8_t>(
        (digit(text[index * 2U]) << 4U) |
        digit(text[index * 2U + 1U]));
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
      source_origin("recipe.yml"),
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

pkgcatalog::catalog_snapshot catalog_snapshot(
    const pkgsource::source_snapshot& source);

pkgresolve::resolution_result build_resolution(
    const pkgsource::source_snapshot& source)
{
  auto catalog = catalog_snapshot(source);
  std::vector<pkgresolve::resolution_goal> goals;
  goals.emplace_back(
      pkgsource::requirement_scope::build(),
      pkgsource::requirement_subject(pkgsource::package_reference("tool")),
      "<effect-build>");
  return pkgresolve::resolve(pkgresolve::resolution_request::seal(
      std::move(catalog), pkgstate::snapshot::make(test_support::binding()),
      pkgresolve::architecture_context(
          pkgsource::architecture_reference("x86_64"),
          pkgsource::architecture_reference("x86_64")),
      std::move(goals), pkgresolve::resolution_policy()));
}

const pkgresolve::selected_package& build_subject(
    const pkgresolve::resolution_result& resolution)
{
  for (const auto& selection : resolution.selections()) {
    if (selection.environment() == pkgresolve::resolution_environment::target &&
        selection.package().name() == "tool")
      return selection;
  }
  throw std::runtime_error("effect fixture resolution lacks tool subject");
}

pkgapply::incoming_package_authority incoming_authority(
    const pkgsource::source_snapshot& source,
    std::uint8_t content_seed = 1)
{
  auto resolved = build_resolution(source);
  const pkgbuild::build_request request = pkgbuild::build_request::seal(
      resolved, build_subject(resolved).identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1, 0022, 1700000000)));
  const pkgbuild::payload_manifest payload = pkgbuild::payload_manifest::seal({
      pkgbuild::payload_entry::regular(
          pkgbuild::payload_path::parse("tool"), 0755, 0, 0, 4,
          pkgbuild::payload_time{10, 0},
          pkgbuild::sha256_digest(hex_digest(content_seed))),
  });
  pkgbuild::sealed_artifact artifact = pkgbuild::sealed_artifact::make(
      pkgbuild::artifact_encoding::package_tar,
      pkgbuild::artifact_compression::none, 4,
      pkgbuild::sha256_digest(hex_digest(static_cast<std::uint8_t>(content_seed + 30U))));
  pkgbuild::build_result result = pkgbuild::build_result::succeeded(
      request, payload, artifact,
      pkgbuild::execution_evidence_identity::from_sha256(hex_digest(61)));
  auto admitted = pkgbuild::image_adapter::build_image_authority::admit(
      std::move(result), incoming_image(content_seed));
  return pkgapply::incoming_package_authority::admit(
      pkgbuild::plan_adapter::project_artifact(admitted));
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
      imported_state_identity<pkgstate::source_snapshot_identity>(
          source.identity().hex()));

  auto control = pkgstate::installed_control::make(
      source_record, pkgstate::installation_reason::explicit_request(),
      pkgstate::build_provenance(
          source_record.identity(),
          state_identity<pkgstate::build_request_identity>(100),
          state_identity<pkgstate::build_input_set_identity>(102),
          state_identity<pkgstate::environment_policy_identity>(103),
          state_identity<pkgstate::build_policy_identity>(104),
          state_identity<pkgstate::build_result_identity>(105),
          state_identity<pkgstate::payload_manifest_identity>(106),
          state_identity<pkgstate::build_artifact_identity>(107),
          state_identity<pkgstate::artifact_content_identity>(108),
          state_identity<pkgstate::artifact_binding_identity>(109),
          state_identity<pkgstate::execution_evidence_identity>(110),
          state_identity<pkgstate::build_image_identity>(111),
          state_identity<pkgstate::artifact_image_identity>(112),
          state_identity<pkgstate::artifact_inspection_identity>(113)));
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
    pkgstate::posix::canonical_generation_store& store,
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
      incoming.candidate(), incoming.artifact(),
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
      incoming.artifact(), archive, incoming.image(),
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

class unreachable_application_backend final
    : public pkgapply::application_backend {
public:
  unreachable_application_backend()
      : mutation_(apply_identity<pkgapply::mutation_backend_identity>(121)),
        observation_(
            apply_identity<pkgapply::observation_backend_identity>(122)),
        capabilities_(
            apply_identity<pkgapply::execution_capability_profile_identity>(
                123))
  {
  }

  const pkgapply::mutation_backend_identity& identity() const noexcept override
  {
    return mutation_;
  }

  const pkgapply::observation_backend_identity&
  observation_identity() const noexcept override
  {
    return observation_;
  }

  const pkgapply::execution_capability_profile_identity&
  capabilities() const noexcept override
  {
    return capabilities_;
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  begin_with_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&,
      const pkgimage::package_image&) override
  {
    throw std::runtime_error("unexpected native application execution");
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  begin_without_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&) override
  {
    throw std::runtime_error("unexpected native removal execution");
  }

private:
  pkgapply::mutation_backend_identity mutation_;
  pkgapply::observation_backend_identity observation_;
  pkgapply::execution_capability_profile_identity capabilities_;
};

class fixed_package_archive final : public pkgimage::package_archive {
public:
  explicit fixed_package_archive(
      const pkgimage::inspected_package_image& inspected)
      : image_(inspected.image()), receipt_(inspected.receipt())
  {
  }

  fixed_package_archive(
      pkgimage::package_image image,
      pkgimage::archive_inspection_receipt receipt)
      : image_(std::move(image)), receipt_(std::move(receipt))
  {
  }

  const pkgimage::package_image& image() const noexcept override
  {
    return image_;
  }

  const pkgimage::archive_inspection_receipt&
  inspection_receipt() const noexcept override
  {
    return receipt_;
  }

  void replay(
      const pkgimage::entry_selection&,
      pkgimage::payload_sink&) const override
  {
    throw std::runtime_error("unexpected native archive replay");
  }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
};

class fixed_effect_archive_source final
    : public pkgctl::transaction_effect_archive_source {
public:
  explicit fixed_effect_archive_source(
      pkgimage::inspected_package_image inspected)
      : inspected_(std::move(inspected))
  {
  }

  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority& incoming) override
  {
    ++calls_;
    requested_ = incoming.identity();
    return std::make_unique<fixed_package_archive>(inspected_);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgapply::incoming_package_authority_identity>&
  requested() const noexcept { return requested_; }

private:
  pkgimage::inspected_package_image inspected_;
  std::size_t calls_ = 0U;
  std::optional<pkgapply::incoming_package_authority_identity> requested_;
};

class forbidden_effect_archive_source final
    : public pkgctl::transaction_effect_archive_source {
public:
  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority&) override
  {
    ++calls_;
    throw std::runtime_error("unexpected removal archive acquisition");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class missing_effect_archive_source final
    : public pkgctl::transaction_effect_archive_source {
public:
  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority&) override
  {
    ++calls_;
    return nullptr;
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class mismatched_receipt_archive_source final
    : public pkgctl::transaction_effect_archive_source {
public:
  mismatched_receipt_archive_source(
      pkgimage::package_image image,
      pkgimage::archive_inspection_receipt receipt)
      : image_(std::move(image)), receipt_(std::move(receipt))
  {
  }

  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority&) override
  {
    ++calls_;
    return std::make_unique<fixed_package_archive>(image_, receipt_);
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
  std::size_t calls_ = 0U;
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

enum class crash_point {
  never,
  pre_lifecycle,
  application,
  post_lifecycle,
  publication_before_store,
  publication_after_store,
};

class memory_effect_journal_store final : public pkgctl::effect_journal_store {
public:
  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity& attempt) const override
  {
    const auto iterator = records_.find(attempt.hex());
    if (iterator == records_.end() || iterator->second.empty())
      return std::nullopt;
    return iterator->second.back();
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record& record) override
  {
    auto& records = records_[record.attempt().hex()];
    if (records.empty())
    {
      if (record.sequence() != 0U || record.previous())
        throw std::runtime_error("invalid journal admission");
    }
    else if (record.sequence() != records.back().sequence() + 1U ||
             !record.previous() ||
             *record.previous() != records.back().identity())
      throw std::runtime_error("invalid journal successor");
    records.push_back(record);
    return records.back();
  }

  std::size_t size(const pkgctl::session_identity& attempt) const
  {
    const auto iterator = records_.find(attempt.hex());
    return iterator == records_.end() ? 0U : iterator->second.size();
  }

private:
  std::map<std::string, std::vector<pkgctl::effect_attempt_record>> records_;
};

class operation_progress_context final
    : public pkgctl::transaction_progress_rehydration_context_source {
public:
  explicit operation_progress_context(pkgctl::effect_restart_checkpoint checkpoint)
      : checkpoint_(std::move(checkpoint))
  {
  }

  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected construction progress context request");
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected check progress context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress& partial,
      const pkgctl::transaction_dispatch& dispatch,
      const pkgctl::effect_attempt_record& evidence) override
  {
    ++calls_;
    partial_ = partial.identity();
    dispatch_ = dispatch.identity();
    evidence_ = evidence.identity();
    return checkpoint_;
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& partial() const noexcept
  { return partial_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }
  const std::optional<pkgctl::session_identity>& evidence() const noexcept
  { return evidence_; }

private:
  pkgctl::effect_restart_checkpoint checkpoint_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> partial_;
  std::optional<pkgctl::session_identity> dispatch_;
  std::optional<pkgctl::session_identity> evidence_;
};

pkgctl::transaction_dispatch_nonce dispatch_nonce(std::uint8_t marker);
pkgctl::transaction_run_nonce dispatch_journal_nonce(std::uint8_t marker);

pkgctl::effect_attempt_nonce effect_nonce(std::uint8_t marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
}

class driver final
    : public pkgctl::transaction_effect_driver,
      public pkgctl::transaction_effect_publication_driver {
public:
  driver(pkgapply::lease_bound_state_projection projection,
         mutation_lease& outer_lease,
         pkgapply::application_receipt application,
         pkgstate::canonical_store& store,
         std::optional<pkgsource::lifecycle_action> fail_lifecycle =
             std::nullopt,
         lease_release_point release = lease_release_point::never,
         publication_mode publication = publication_mode::native,
         crash_point crash = crash_point::never)
      : projection_(std::move(projection)), lease_(outer_lease),
        application_(std::move(application)), store_(store),
        backend_(fail_lifecycle), release_(release),
        publication_(publication), crash_(crash)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override { return lease_; }
  const pkgapply::lease_bound_state_projection& state_projection() const noexcept override
  { return projection_; }
  pkgapply_exec::lifecycle_execution_result execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override
  {
    const auto action = session.node().action();
    trace_.push_back(std::string(pkgsource::to_string(action)));
    if ((crash_ == crash_point::pre_lifecycle &&
         action == pkgsource::lifecycle_action::pre_install) ||
        (crash_ == crash_point::post_lifecycle &&
         action == pkgsource::lifecycle_action::post_install))
      throw std::runtime_error("simulated lifecycle interruption");
    auto result = pkgapply_exec::execute(session, backend_);
    lifecycle_results_.push_back(result);
    return result;
  }
  pkgapply::application_receipt apply_application(
      const pkgapply::package_application_request&) override
  {
    trace_.push_back("apply");
    if (crash_ == crash_point::application)
      throw std::runtime_error("simulated application interruption");
    if (release_ == lease_release_point::after_application)
      lease_.release();
    return application_;
  }
  pkgapply::application_receipt resume_application(
      const pkgapply::package_application_request&,
      const pkgapply::application_journal_record&) override
  {
    trace_.push_back("resume-apply");
    ++resume_calls_;
    return application_;
  }
  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    trace_.push_back("publish");
    ++publication_calls_;
    last_publication_request_ = request;
    if (release_ == lease_release_point::during_publication)
      lease_.release();
    if (publication_ == publication_mode::rejected)
      return pkgstate::state_publication_receipt::request_rejected(
          request, store_.read(), "test/pkgctl-effect-v1");
    if (publication_ == publication_mode::indeterminate)
      return pkgstate::state_publication_receipt::indeterminate(
          request, store_.read(), std::nullopt, "test/pkgctl-effect-v1",
          pkgstate::state_storage_atomicity_boundary::immutable_generation_selection);
    if (crash_ == crash_point::publication_before_store)
      throw std::runtime_error("simulated pre-publication interruption");
    auto receipt = store_.compare_and_publish(request);
    if (crash_ == crash_point::publication_after_store)
      throw std::runtime_error("simulated publication interruption");
    return receipt;
  }
  pkgstate::snapshot read_state() const override { return store_.read(); }
  std::size_t publication_calls() const noexcept { return publication_calls_; }
  std::size_t resume_calls() const noexcept { return resume_calls_; }
  const std::vector<std::string>& trace() const noexcept { return trace_; }
  const std::vector<pkgapply_exec::lifecycle_execution_result>&
  lifecycle_results() const noexcept { return lifecycle_results_; }
  const std::optional<pkgstate::state_publication_request>&
  last_publication_request() const noexcept { return last_publication_request_; }
private:
  pkgapply::lease_bound_state_projection projection_;
  mutation_lease& lease_;
  pkgapply::application_receipt application_;
  pkgstate::canonical_store& store_;
  scripted_execution_backend backend_;
  lease_release_point release_;
  publication_mode publication_;
  crash_point crash_;
  std::size_t publication_calls_ = 0;
  std::size_t resume_calls_ = 0;
  std::vector<std::string> trace_;
  std::vector<pkgapply_exec::lifecycle_execution_result> lifecycle_results_;
  std::optional<pkgstate::state_publication_request> last_publication_request_;
};

struct fixture final {
  test_support::temporary_directory temp;
  pkgstate::posix::canonical_generation_store store;
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

pkgapply::application_journal_record
application_restart_journal(const fixture& value)
{
  pkgapply::application_attempt_nonce::byte_array nonce_bytes{};
  nonce_bytes.back() = 77U;
  const auto attempt = pkgapply::application_attempt::make(
      value.application.identity(), value.target.identity(),
      value.target.mutation_backend(),
      pkgapply::application_attempt_nonce::from_bytes(nonce_bytes));
  const auto header = pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install, value.application.identity(),
      value.application.plan().identity(), attempt, value.target.identity(),
      value.application.control().identity(), value.projection.identity(),
      value.outer_lease.identity(), value.target.mutation_backend());
  return pkgapply::application_journal_record::make(
      header, pkgapply::application_journal_state::preparing, {}, {});
}

struct upgrade_fixture final {
  test_support::temporary_directory temp;
  pkgstate::posix::canonical_generation_store store;
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
      value.transaction, value.expected,
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
  pkgstate::posix::canonical_generation_store store;
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

  explicit removal_fixture(std::string version = "1.0")
      : store(temp.path() / "state", test_support::binding()),
        source(source_snapshot(
            std::move(version),
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
      value.transaction, value.expected,
      action_node(value.transaction,
                  pkgtransaction::transaction_action_kind::remove)
          .identity(),
      pkgapply::package_application_request(value.application),
      operation_lifecycle_order(
          value.transaction,
          pkgtransaction::transaction_action_kind::remove));
}

class unused_preparation_driver final
    : public pkgctl::operation_preparation_driver {
public:
  [[nodiscard]] pkgbuild::plan_adapter::artifact_projection
  project_artifact(const pkgctl::construction_result&) override
  {
    throw std::runtime_error(
        "removal preparation attempted incoming artifact projection");
  }
};

void check_removal_preparation()
{
  removal_fixture value;
  const auto path = pkgplan::package_path::parse("tool");
  auto observations = pkgplan::target_observation_set(
      plan_identity<pkgplan::observation_set_identity>(80),
      value.target_system, pkgplan::fact_set_completeness::complete,
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(path, planner_regular(2)))});
  auto request = pkgctl::operation_preparation_request::remove(
      pkgctl::transaction_progress::begin(value.transaction),
      action_node(value.transaction,
                  pkgtransaction::transaction_action_kind::remove)
          .identity(),
      value.target, execution_control(), std::move(observations), policy(),
      operation_lifecycle_order(
          value.transaction, pkgtransaction::transaction_action_kind::remove));
  unused_preparation_driver driver;
  const auto result = pkgctl::prepare_operation(std::move(request), driver);
  CHECK(result.prepared());
  CHECK(!result.artifact());
  CHECK(!result.incoming());
  CHECK(!result.refusal());
  CHECK(result.plan() && result.plan()->identity() == value.plan.identity());
  CHECK(result.application() &&
        result.application()->identity() == value.application.identity());
  const auto expected_effect = effect_request(value);
  CHECK(result.effect() &&
        result.effect()->identity() == expected_effect.identity());
}

void check_removal_preparation_refusal()
{
  removal_fixture value;
  auto observations = pkgplan::target_observation_set(
      plan_identity<pkgplan::observation_set_identity>(81),
      value.target_system, pkgplan::fact_set_completeness::complete, {});
  auto request = pkgctl::operation_preparation_request::remove(
      pkgctl::transaction_progress::begin(value.transaction),
      action_node(value.transaction,
                  pkgtransaction::transaction_action_kind::remove)
          .identity(),
      value.target, execution_control(), std::move(observations), policy(),
      operation_lifecycle_order(
          value.transaction, pkgtransaction::transaction_action_kind::remove));
  unused_preparation_driver driver;
  const auto result = pkgctl::prepare_operation(std::move(request), driver);
  CHECK(!result.prepared());
  CHECK(result.outcome() ==
        pkgctl::operation_preparation_outcome::planning_refused);
  CHECK(result.refusal() && result.refusal()->removal());
  CHECK(result.refusal() &&
        result.refusal()->code() ==
            pkgplan::planning_refusal_code::incomplete_fact_universe);
  CHECK(!result.plan());
  CHECK(!result.application());
  CHECK(!result.effect());
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

  auto progression = pkgctl::transaction_progress::begin(value.transaction);
  const auto& action = action_node(
      value.transaction, pkgtransaction::transaction_action_kind::remove);
  CHECK(progression.status(action.identity()) ==
        pkgctl::transaction_node_status::ready);
  const auto unit = std::find_if(
      progression.ready_units().begin(), progression.ready_units().end(),
      [&](const auto& candidate) {
        return candidate.primary_node() == action.identity();
      });
  CHECK(unit != progression.ready_units().end());
  CHECK(unit != progression.ready_units().end() &&
        unit->kind() == pkgctl::transaction_unit_kind::operation);
  CHECK(unit != progression.ready_units().end() &&
        unit->members().size() == 3U);

  bool refused = false;
  try
  {
    (void)pkgctl::advance_effect(progression, result, value.expected);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(refused);

  const auto resulting_state = value.store.read();
  progression = pkgctl::advance_effect(
      std::move(progression), result, resulting_state);
  CHECK(progression.current_state().identity() == resulting_state.identity());
  CHECK(progression.status(action.identity()) ==
        pkgctl::transaction_node_status::satisfied);
  for (const auto& node : value.transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::lifecycle)
      CHECK(progression.status(node.identity()) ==
            pkgctl::transaction_node_status::satisfied);
  }
  CHECK(progression.complete());
}

void check_removal_progression_failure()
{
  removal_fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  driver actuator(
      value.projection, value.outer_lease, value.receipt, value.store,
      pkgsource::lifecycle_action::pre_remove);
  const auto result = pkgctl::execute_effectful_operation(session, actuator);
  CHECK(result.outcome() ==
        pkgctl::effectful_operation_outcome::
            lifecycle_failed_before_application);

  auto progression = pkgctl::transaction_progress::begin(value.transaction);
  const auto initial_state = progression.current_state().identity();
  progression = pkgctl::advance_effect(
      std::move(progression), result);
  const auto& action = action_node(
      value.transaction, pkgtransaction::transaction_action_kind::remove);
  const auto order = operation_lifecycle_order(
      value.transaction, pkgtransaction::transaction_action_kind::remove);
  CHECK(progression.status(action.identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(order.before().size() == 1U);
  CHECK(order.after().size() == 1U);
  CHECK(progression.status(order.before().front()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(progression.status(order.after().front()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(progression.current_state().identity() == initial_state);
  CHECK(progression.failed());
  CHECK(!progression.complete());
}

pkgctl::effectful_operation_request effect_request(const fixture& value)
{
  return pkgctl::effectful_operation_request::make(
      value.transaction, value.expected,
      install_node(value.transaction).identity(),
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
        value.transaction, value.expected, build->identity(),
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
        value.transaction, value.expected,
        install_node(value.transaction).identity(),
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
        value.transaction, value.expected,
        install_node(value.transaction).identity(),
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


void check_durable_success()
{
  fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store);
  memory_effect_journal_store journal;
  const auto result = pkgctl::execute_effectful_operation_durable(
      session, effect_nonce(1), actuator, journal);
  CHECK(result.succeeded());
  const auto admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(1));
  const auto latest = journal.load_latest(admission.attempt());
  CHECK(latest.has_value());
  CHECK(latest && latest->stage() == pkgctl::effect_attempt_stage::terminal);
  CHECK(latest && latest->terminal_outcome() &&
        *latest->terminal_outcome() ==
            pkgctl::effectful_operation_outcome::completed);
  if (latest)
  {
    const auto decoded = pkgctl::decode_effect_attempt_record(
        pkgctl::encode_effect_attempt_record(*latest));
    CHECK(decoded.identity() == latest->identity());
    CHECK(decoded.publication().has_value());
    CHECK(decoded.terminal_outcome() == latest->terminal_outcome());
  }
  CHECK(journal.size(admission.attempt()) == 10U);
}

void check_operation_progress_rehydration()
{
  // Removal is operation-ready without a predecessor construction dispatch.
  removal_fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  auto initial = pkgctl::transaction_progress::begin(value.transaction);
  auto run = pkgctl::transaction_run::begin(
      initial, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, dispatch_journal_nonce(94U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(94U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  CHECK(reservation.dispatch->unit().kind() ==
        pkgctl::transaction_unit_kind::operation);
  auto reserved = admitted.successor(reservation.run);
  auto started_authority = pkgctl::start_operation_dispatch(
      reservation.run, *reservation.dispatch, session, effect_nonce(94U));
  auto started = reserved.successor(started_authority.run);

  driver actuator(value.projection, value.outer_lease, value.receipt, value.store);
  memory_effect_journal_store journal;
  const auto result = pkgctl::execute_effectful_operation_durable(
      session, effect_nonce(94U), actuator, journal);
  CHECK(result.succeeded());
  auto completed_run = pkgctl::submit_operation_dispatch_result(
      started_authority.run, *reservation.dispatch, result, value.store.read());
  auto completed = started.successor(completed_run);

  const auto latest = journal.load_latest(started_authority.effect_attempt.attempt());
  CHECK(latest.has_value());
  if (!latest)
    return;
  const auto checkpoint = pkgctl::effect_restart_checkpoint::make(
      session, *latest, result.before(), result.application(), result.after(),
      result.publication_request(), result.publication_receipt());
  operation_progress_context context(checkpoint);
  std::vector<std::string> evidence_trace;
  run_execute_support::sequenced_evidence_store evidence(evidence_trace);
  pkgctl::stored_transaction_progress_rehydration_source source(
      value.transaction, evidence, journal, context);

  const auto journal_size = journal.size(started_authority.effect_attempt.attempt());
  const auto restored = source.rehydrate_progress(completed);
  CHECK(restored.identity() == completed_run.progress().identity());
  CHECK(restored.current_state().identity() == value.store.read().identity());
  CHECK(restored.effects().size() == 1U);
  CHECK(restored.effects().front().identity() == result.identity());
  CHECK(context.calls() == 1U);
  CHECK(context.partial() ==
        std::optional<pkgctl::session_identity>(initial.identity()));
  CHECK(context.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(context.evidence() ==
        std::optional<pkgctl::session_identity>(latest->identity()));
  CHECK(journal.size(started_authority.effect_attempt.attempt()) == journal_size);
  CHECK(evidence_trace.empty());
  CHECK(pkgctl::rehydrate_transaction_run(completed, source).run().identity() ==
        completed_run.identity());
}

void check_restart_boundaries()
{
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(value.projection, value.outer_lease,
                    value.receipt, value.store, std::nullopt,
                    lease_release_point::never, publication_mode::native,
                    crash_point::pre_lifecycle);
    memory_effect_journal_store journal;
    bool interrupted = false;
    try
    {
      (void)pkgctl::execute_effectful_operation_durable(
          session, effect_nonce(2), actuator, journal);
    }
    catch (const std::runtime_error&)
    {
      interrupted = true;
    }
    CHECK(interrupted);
    const auto admission = pkgctl::effect_attempt_record::admit(
        session.identity(), session.before().size(), session.after().size(),
        effect_nonce(2));
    const auto latest = journal.load_latest(admission.attempt());
    CHECK(latest && latest->stage() ==
                        pkgctl::effect_attempt_stage::before_lifecycle_intent);
    CHECK(latest && pkgctl::assess_effect_restart(*latest).disposition() ==
                        pkgctl::effect_restart_disposition::
                            external_resolution_required);
  }
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(value.projection, value.outer_lease,
                    value.receipt, value.store, std::nullopt,
                    lease_release_point::never, publication_mode::native,
                    crash_point::application);
    memory_effect_journal_store journal;
    try
    {
      (void)pkgctl::execute_effectful_operation_durable(
          session, effect_nonce(3), actuator, journal);
    }
    catch (const std::runtime_error&)
    {
    }
    const auto admission = pkgctl::effect_attempt_record::admit(
        session.identity(), session.before().size(), session.after().size(),
        effect_nonce(3));
    const auto latest = journal.load_latest(admission.attempt());
    CHECK(latest && latest->stage() ==
                        pkgctl::effect_attempt_stage::application_intent);
    CHECK(latest && pkgctl::assess_effect_restart(*latest).disposition() ==
                        pkgctl::effect_restart_disposition::resume_application);
    CHECK(actuator.lifecycle_results().size() == 1U);
    const auto unresolved_checkpoint = pkgctl::effect_restart_checkpoint::make(
        session, *latest, actuator.lifecycle_results(), std::nullopt, {},
        std::nullopt, std::nullopt, std::nullopt);
    const auto unresolved = pkgctl::resume_effectful_operation(
        unresolved_checkpoint, &actuator, nullptr, journal);
    CHECK(unresolved.external_resolution_required());
    CHECK(!unresolved.operation());
    CHECK(actuator.resume_calls() == 0U);

    const auto checkpoint = pkgctl::effect_restart_checkpoint::make(
        session, *latest, actuator.lifecycle_results(), std::nullopt, {},
        std::nullopt, std::nullopt, application_restart_journal(value));
    const auto restarted = pkgctl::resume_effectful_operation(
        checkpoint, &actuator, nullptr, journal);
    CHECK(!restarted.external_resolution_required());
    CHECK(restarted.terminal());
    CHECK(restarted.operation());
    CHECK(restarted.operation() && restarted.operation()->succeeded());
    CHECK(actuator.resume_calls() == 1U);
    CHECK(actuator.trace() == std::vector<std::string>({
        "pre-install", "apply", "resume-apply", "post-install", "publish"}));
  }
  {
    fixture value;
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    driver actuator(value.projection, value.outer_lease,
                    value.receipt, value.store, std::nullopt,
                    lease_release_point::never, publication_mode::native,
                    crash_point::post_lifecycle);
    memory_effect_journal_store journal;
    try
    {
      (void)pkgctl::execute_effectful_operation_durable(
          session, effect_nonce(4), actuator, journal);
    }
    catch (const std::runtime_error&)
    {
    }
    const auto admission = pkgctl::effect_attempt_record::admit(
        session.identity(), session.before().size(), session.after().size(),
        effect_nonce(4));
    const auto latest = journal.load_latest(admission.attempt());
    CHECK(latest && latest->stage() ==
                        pkgctl::effect_attempt_stage::after_lifecycle_intent);
    CHECK(latest && pkgctl::assess_effect_restart(*latest).disposition() ==
                        pkgctl::effect_restart_disposition::
                            external_resolution_required);
  }
}


void check_publication_retry()
{
  fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  driver interrupted_driver(
      value.projection, value.outer_lease, value.receipt, value.store,
      std::nullopt, lease_release_point::never, publication_mode::native,
      crash_point::publication_before_store);
  memory_effect_journal_store journal;
  bool interrupted = false;
  try
  {
    (void)pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(6), interrupted_driver, journal);
  }
  catch (const std::runtime_error&)
  {
    interrupted = true;
  }
  CHECK(interrupted);
  const auto admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(6));
  const auto latest = journal.load_latest(admission.attempt());
  CHECK(latest && latest->stage() ==
                      pkgctl::effect_attempt_stage::publication_intent);
  CHECK(interrupted_driver.lifecycle_results().size() == 2U);
  CHECK(interrupted_driver.last_publication_request().has_value());
  CHECK(value.store.read().identity() == value.expected.identity());

  std::vector<pkgapply_exec::lifecycle_execution_result> before{
      interrupted_driver.lifecycle_results().front()};
  std::vector<pkgapply_exec::lifecycle_execution_result> after{
      interrupted_driver.lifecycle_results().back()};
  const auto checkpoint = pkgctl::effect_restart_checkpoint::make(
      session, *latest, std::move(before), value.receipt, std::move(after),
      interrupted_driver.last_publication_request(), std::nullopt, std::nullopt);
  driver restart_driver(
      value.projection, value.outer_lease, value.receipt, value.store);
  const auto restarted = pkgctl::resume_effectful_operation(
      checkpoint, nullptr, &restart_driver, journal);
  CHECK(restarted.terminal());
  CHECK(!restarted.external_resolution_required());
  CHECK(restarted.operation() && restarted.operation()->succeeded());
  CHECK(restarted.operation() && !restarted.operation()->reconciled_state());
  CHECK(interrupted_driver.publication_calls() == 1U);
  CHECK(restart_driver.publication_calls() == 1U);
  CHECK(restart_driver.trace() == std::vector<std::string>({"publish"}));
  CHECK(value.store.read().size() == 1U);
}

void check_publication_reconciliation()
{
  fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store, std::nullopt,
                  lease_release_point::never, publication_mode::native,
                  crash_point::publication_after_store);
  memory_effect_journal_store journal;
  bool interrupted = false;
  try
  {
    (void)pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(5), actuator, journal);
  }
  catch (const std::runtime_error&)
  {
    interrupted = true;
  }
  CHECK(interrupted);
  const auto admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(5));
  const auto latest = journal.load_latest(admission.attempt());
  CHECK(latest && latest->stage() ==
                      pkgctl::effect_attempt_stage::publication_intent);
  CHECK(actuator.lifecycle_results().size() == 2U);
  CHECK(actuator.last_publication_request().has_value());

  std::vector<pkgapply_exec::lifecycle_execution_result> before{
      actuator.lifecycle_results().front()};
  std::vector<pkgapply_exec::lifecycle_execution_result> after{
      actuator.lifecycle_results().back()};
  const auto checkpoint = pkgctl::effect_restart_checkpoint::make(
      session, *latest, std::move(before), value.receipt, std::move(after),
      actuator.last_publication_request(), std::nullopt, std::nullopt);
  const auto restarted = pkgctl::resume_effectful_operation(
      checkpoint, nullptr, &actuator, journal);
  CHECK(restarted.terminal());
  CHECK(!restarted.external_resolution_required());
  CHECK(restarted.operation().has_value());
  CHECK(restarted.operation() && restarted.operation()->succeeded());
  CHECK(restarted.operation() && restarted.operation()->reconciled_state());
  CHECK(actuator.publication_calls() == 1U);
  CHECK(value.store.read().size() == 1U);
}

pkgctl::transaction_dispatch_nonce dispatch_nonce(std::uint8_t marker)
{
  pkgctl::transaction_dispatch_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
}

pkgctl::transaction_run_nonce dispatch_journal_nonce(
    std::uint8_t marker)
{
  pkgctl::transaction_run_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_run_nonce::from_bytes(bytes);
}




class fixed_effect_progress_source final
    : public pkgctl::transaction_progress_rehydration_source {
public:
  explicit fixed_effect_progress_source(pkgctl::transaction_progress progress)
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

class unreachable_effect_recovery_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  pkgctl::construction_result construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction recovery request");
  }

  pkgctl::transaction_check_result check(
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

class forbidden_dispatch_nonce_source final
    : public pkgctl::transaction_dispatch_nonce_source {
public:
  pkgctl::transaction_dispatch_nonce issue(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&) override
  {
    ++calls_;
    throw std::runtime_error("unexpected fresh dispatch nonce request");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class operation_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source {
public:
  operation_execution_authority_source(
      pkgctl::effectful_operation_session session,
      pkgctl::effect_attempt_nonce nonce)
      : session_(std::move(session)), nonce_(std::move(nonce))
  {
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction execution authority request");
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check execution authority request");
  }

  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_run& run,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = record.identity();
    run_ = run.identity();
    dispatch_ = dispatch.identity();
    return pkgctl::operation_dispatch_execution_authority{session_, nonce_};
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& run() const noexcept
  { return run_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::effectful_operation_session session_;
  pkgctl::effect_attempt_nonce nonce_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> run_;
  std::optional<pkgctl::session_identity> dispatch_;
};

class operation_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  explicit operation_recovery_authority_source(
      pkgctl::effect_restart_checkpoint checkpoint)
      : checkpoint_(std::move(checkpoint))
  {
  }

  pkgctl::construction_result construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected construction recovery authority request");
  }

  pkgctl::transaction_check_result check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check recovery authority request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint& checkpoint,
      const pkgctl::transaction_dispatch_restart_assessment& assessment,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = checkpoint.record().identity();
    assessment_ = assessment.dispatch();
    dispatch_ = dispatch.identity();
    return checkpoint_;
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& assessment() const noexcept
  { return assessment_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::effect_restart_checkpoint checkpoint_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> assessment_;
  std::optional<pkgctl::session_identity> dispatch_;
};


class operation_recovery_context_source final
    : public pkgctl::transaction_dispatch_recovery_context_source {
public:
  explicit operation_recovery_context_source(
      pkgctl::effect_restart_checkpoint checkpoint)
      : checkpoint_(std::move(checkpoint))
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
    throw std::runtime_error("unexpected check recovery context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    ++calls_;
    return checkpoint_;
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  pkgctl::effect_restart_checkpoint checkpoint_;
  std::size_t calls_ = 0U;
};

class recording_effect_store final : public pkgctl::effect_journal_store {
public:
  recording_effect_store(
      std::vector<std::string>& trace,
      std::optional<pkgctl::effect_attempt_record> returned = std::nullopt)
      : trace_(trace), returned_(std::move(returned))
  {
  }

  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity&) const override
  {
    return latest_;
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record& record) override
  {
    trace_.push_back("effect");
    latest_ = record;
    return returned_ ? *returned_ : record;
  }

private:
  std::vector<std::string>& trace_;
  std::optional<pkgctl::effect_attempt_record> returned_;
  std::optional<pkgctl::effect_attempt_record> latest_;
};

class recording_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  recording_run_store(
      std::vector<std::string>& trace,
      std::optional<pkgctl::transaction_run_journal_record> returned =
          std::nullopt,
      bool fail_once = false)
      : trace_(trace), returned_(std::move(returned)),
        fail_once_(fail_once)
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity&) const override
  {
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    trace_.push_back("run");
    if (fail_once_)
    {
      fail_once_ = false;
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected run-store failure");
    }
    latest_ = record;
    return returned_ ? *returned_ : record;
  }

private:
  std::vector<std::string>& trace_;
  std::optional<pkgctl::transaction_run_journal_record> returned_;
  std::optional<pkgctl::transaction_run_journal_record> latest_;
  bool fail_once_;
};


class sequenced_effect_store final : public pkgctl::effect_journal_store {
public:
  sequenced_effect_store(
      std::vector<std::string>& trace, std::size_t fail_on_append = 0U)
      : trace_(trace), fail_on_append_(fail_on_append)
  {
  }

  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity& attempt) const override
  {
    if (!latest_ || latest_->attempt() != attempt)
      return std::nullopt;
    return latest_;
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record& record) override
  {
    ++append_count_;
    trace_.push_back("effect-" + std::to_string(append_count_));
    if (append_count_ == fail_on_append_)
      throw pkgctl::effect_journal_error(
          pkgctl::effect_journal_error_code::store_write_failed,
          "injected effect-store failure");
    if (latest_ && latest_->identity() == record.identity())
      return *latest_;
    if (latest_)
      record.validate_successor_of(*latest_);
    latest_ = record;
    return record;
  }

  const pkgctl::effect_attempt_record& latest() const
  {
    if (!latest_)
      throw std::runtime_error("effect store has no latest record");
    return *latest_;
  }

private:
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  std::size_t append_count_ = 0U;
  std::optional<pkgctl::effect_attempt_record> latest_;
};

class tracing_effect_driver final : public pkgctl::transaction_effect_driver {
public:
  tracing_effect_driver(
      pkgctl::transaction_effect_driver& driver,
      std::vector<std::string>& trace)
      : driver_(driver), trace_(trace)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return driver_.lease();
  }

  const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept override
  {
    return driver_.state_projection();
  }

  pkgapply_exec::lifecycle_execution_result execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override
  {
    mark_driver();
    return driver_.execute_lifecycle(session);
  }

  pkgapply::application_receipt apply_application(
      const pkgapply::package_application_request& request) override
  {
    mark_driver();
    return driver_.apply_application(request);
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    mark_driver();
    return driver_.publish_state(request);
  }

  pkgapply::application_receipt resume_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_journal_record& journal) override
  {
    mark_driver();
    return driver_.resume_application(request, journal);
  }


private:
  void mark_driver()
  {
    if (!driver_seen_)
    {
      trace_.push_back("driver");
      driver_seen_ = true;
    }
  }

  pkgctl::transaction_effect_driver& driver_;
  std::vector<std::string>& trace_;
  bool driver_seen_ = false;
};

class tracing_effect_state_observer final
    : public pkgctl::transaction_effect_state_observer {
public:
  tracing_effect_state_observer(
      pkgctl::transaction_effect_state_observer& observer,
      std::vector<std::string>& trace)
      : observer_(observer), trace_(trace)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return observer_.lease();
  }

  pkgstate::snapshot read_state() const override
  {
    trace_.push_back("state-observer");
    return observer_.read_state();
  }

private:
  pkgctl::transaction_effect_state_observer& observer_;
  std::vector<std::string>& trace_;
};

class tracing_effect_publication_driver final
    : public pkgctl::transaction_effect_publication_driver {
public:
  tracing_effect_publication_driver(
      pkgctl::transaction_effect_publication_driver& driver,
      std::vector<std::string>& trace)
      : driver_(driver), trace_(trace)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return driver_.lease();
  }

  pkgstate::snapshot read_state() const override
  {
    trace_.push_back("publication-state");
    return driver_.read_state();
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    trace_.push_back("publication-driver");
    return driver_.publish_state(request);
  }

private:
  pkgctl::transaction_effect_publication_driver& driver_;
  std::vector<std::string>& trace_;
};

class fixed_effect_driver_source final
    : public pkgctl::transaction_effect_driver_source {
public:
  fixed_effect_driver_source(
      driver& driver,
      std::vector<std::string>& trace)
      : continuation_(driver), observer_(driver), publication_(driver),
        trace_(trace)
  {
  }

  pkgctl::transaction_effect_execution_drivers
  acquire_execution_drivers(
      const pkgctl::transaction_dispatch_execution_handoff& handoff) override
  {
    ++execution_calls_;
    execution_handoff_ = handoff.identity();
    trace_.push_back("driver-source-execution");
    return {
        std::make_unique<tracing_effect_driver>(continuation_, trace_),
        std::make_unique<tracing_effect_state_observer>(observer_, trace_)};
  }

  pkgctl::transaction_effect_recovery_drivers
  acquire_recovery_drivers(
      const pkgctl::transaction_dispatch_recovery_handoff& handoff) override
  {
    ++recovery_calls_;
    recovery_handoff_ = handoff.identity();
    trace_.push_back("driver-source-recovery");
    const auto* checkpoint = handoff.operation();
    if (checkpoint == nullptr)
      throw std::runtime_error("recovery handoff has no operation checkpoint");

    pkgctl::transaction_effect_recovery_drivers drivers;
    if (pkgctl::operation_reconciliation_requires_continuation_driver(
            *checkpoint))
      drivers.continuation =
          std::make_unique<tracing_effect_driver>(continuation_, trace_);
    if (pkgctl::operation_reconciliation_requires_state_observer(*checkpoint))
      drivers.resulting_state =
          std::make_unique<tracing_effect_state_observer>(observer_, trace_);
    if (pkgctl::operation_reconciliation_requires_publication_driver(
            *checkpoint))
      drivers.publication =
          std::make_unique<tracing_effect_publication_driver>(
              publication_, trace_);
    return drivers;
  }

  std::size_t execution_calls() const noexcept { return execution_calls_; }
  std::size_t recovery_calls() const noexcept { return recovery_calls_; }
  const std::optional<pkgctl::session_identity>&
  execution_handoff() const noexcept { return execution_handoff_; }
  const std::optional<pkgctl::session_identity>&
  recovery_handoff() const noexcept { return recovery_handoff_; }

private:
  pkgctl::transaction_effect_driver& continuation_;
  pkgctl::transaction_effect_state_observer& observer_;
  pkgctl::transaction_effect_publication_driver& publication_;
  std::vector<std::string>& trace_;
  std::size_t execution_calls_ = 0U;
  std::size_t recovery_calls_ = 0U;
  std::optional<pkgctl::session_identity> execution_handoff_;
  std::optional<pkgctl::session_identity> recovery_handoff_;
};

class rejecting_effect_driver_source final
    : public pkgctl::transaction_effect_driver_source {
public:
  explicit rejecting_effect_driver_source(std::vector<std::string>& trace)
      : trace_(trace)
  {
  }

  pkgctl::transaction_effect_execution_drivers
  acquire_execution_drivers(
      const pkgctl::transaction_dispatch_execution_handoff&) override
  {
    ++execution_calls_;
    trace_.push_back("driver-source-rejected");
    throw std::runtime_error("injected effect-driver source refusal");
  }

  pkgctl::transaction_effect_recovery_drivers
  acquire_recovery_drivers(
      const pkgctl::transaction_dispatch_recovery_handoff&) override
  {
    ++recovery_calls_;
    throw std::runtime_error("unexpected recovery driver request");
  }

  std::size_t execution_calls() const noexcept { return execution_calls_; }
  std::size_t recovery_calls() const noexcept { return recovery_calls_; }

private:
  std::vector<std::string>& trace_;
  std::size_t execution_calls_ = 0U;
  std::size_t recovery_calls_ = 0U;
};

pkgctl::transaction_dispatch_execution_handoff
make_operation_execution_handoff(
    const pkgctl::effectful_operation_session& session,
    std::uint8_t marker)
{
  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(session.request().transaction()),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, dispatch_journal_nonce(marker));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
  if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
          pkgctl::transaction_unit_kind::operation)
    throw std::runtime_error("native source fixture did not reserve operation");
  auto reserved = admitted.successor(reservation.run);
  operation_execution_authority_source source(
      session, effect_nonce(marker));
  return pkgctl::acquire_transaction_dispatch_execution_authority(
      std::move(reserved), std::move(reservation.run),
      *reservation.dispatch, source);
}

pkgctl::transaction_dispatch_recovery_handoff
make_operation_recovery_handoff(
    const pkgctl::effect_restart_checkpoint& effect,
    std::uint8_t marker)
{
  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(
          effect.session().request().transaction()),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, dispatch_journal_nonce(marker));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
  if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
          pkgctl::transaction_unit_kind::operation)
    throw std::runtime_error("native recovery fixture did not reserve operation");
  auto reserved = admitted.successor(reservation.run);
  auto started = pkgctl::start_operation_dispatch(
      reservation.run, *reservation.dispatch, effect.session(),
      effect.record().nonce());
  auto started_record = reserved.successor(started.run);
  operation_recovery_authority_source source(effect);
  return pkgctl::acquire_transaction_dispatch_recovery_authority(
      pkgctl::transaction_run_restart_checkpoint::make(
          started.run.progress(), std::move(started_record)),
      *reservation.dispatch, source);
}

int open_directory(const std::filesystem::path& path)
{
  const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
    throw std::runtime_error("cannot open native source lock directory");
  return fd;
}

void check_native_effect_archive_source()
{
  fixture value;
  fixed_effect_archive_source exact(value.incoming.image());
  auto archive = pkgctl::acquire_transaction_effect_archive(
      exact, pkgapply::package_application_request(value.application));
  CHECK(archive != nullptr);
  CHECK(exact.calls() == 1U);
  CHECK(exact.requested() ==
        std::optional<pkgapply::incoming_package_authority_identity>(
            value.incoming.identity()));
  CHECK(archive &&
        archive->image().identity() == value.incoming.image().image().identity());
  CHECK(archive && archive->inspection_receipt().identity() ==
        value.incoming.image().receipt().identity());

  fixed_effect_archive_source foreign(incoming_image(9U));
  bool foreign_refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_effect_archive(
        foreign, pkgapply::package_application_request(value.application));
  }
  catch (const pkgctl::native_effect_source_error& problem)
  {
    foreign_refused = problem.code() ==
        pkgctl::native_effect_source_error_code::archive_image_mismatch;
  }
  CHECK(foreign_refused);

  missing_effect_archive_source missing;
  bool missing_refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_effect_archive(
        missing, pkgapply::package_application_request(value.application));
  }
  catch (const pkgctl::native_effect_source_error& problem)
  {
    missing_refused = problem.code() ==
        pkgctl::native_effect_source_error_code::archive_missing;
  }
  CHECK(missing_refused);
  CHECK(missing.calls() == 1U);

  const auto alternate = incoming_image(8U);
  mismatched_receipt_archive_source mismatched_receipt(
      value.incoming.image().image(), alternate.receipt());
  bool receipt_refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_effect_archive(
        mismatched_receipt,
        pkgapply::package_application_request(value.application));
  }
  catch (const pkgctl::native_effect_source_error& problem)
  {
    receipt_refused = problem.code() ==
        pkgctl::native_effect_source_error_code::archive_receipt_mismatch;
  }
  CHECK(receipt_refused);
  CHECK(mismatched_receipt.calls() == 1U);

  removal_fixture removal;
  forbidden_effect_archive_source unused;
  auto absent = pkgctl::acquire_transaction_effect_archive(
      unused, pkgapply::package_application_request(removal.application));
  CHECK(absent == nullptr);
  CHECK(unused.calls() == 0U);
}

void check_native_effect_driver_source()
{
  removal_fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  const auto handoff = make_operation_execution_handoff(session, 101U);

  const auto locks = value.temp.path() / "native-effect-locks";
  std::filesystem::create_directory(locks);
  const int lock_fd = open_directory(locks);
  unreachable_application_backend application_backend;
  scripted_execution_backend lifecycle_backend;
  forbidden_effect_archive_source archives;
  auto source =
      pkgctl::posix_transaction_effect_driver_source::from_lock_directory_fd(
          lock_fd, application_backend, lifecycle_backend,
          value.store, archives);
  CHECK(::close(lock_fd) == 0);

  auto drivers = source->acquire_execution_drivers(handoff);
  CHECK(drivers.continuation != nullptr);
  CHECK(drivers.resulting_state != nullptr);
  CHECK(archives.calls() == 0U);
  CHECK(drivers.continuation &&
        drivers.continuation->lease().held());
  CHECK(drivers.continuation && drivers.resulting_state &&
        drivers.continuation->lease().identity() ==
            drivers.resulting_state->lease().identity());
  CHECK(drivers.continuation &&
        drivers.continuation->state_projection().snapshot().string() ==
            value.expected.identity().string());
  CHECK(drivers.continuation &&
        drivers.continuation->state_projection().ownership_inventory().string() ==
            value.expected.ownership_identity().string());
  CHECK(drivers.resulting_state &&
        drivers.resulting_state->read_state().identity() ==
            value.expected.identity());

  bool busy = false;
  try
  {
    (void)source->acquire_execution_drivers(handoff);
  }
  catch (const pkgapply::posix::target_mutation_lease_error& problem)
  {
    busy = problem.code() ==
        pkgapply::posix::target_mutation_lease_error_code::lock_busy;
  }
  CHECK(busy);

  const auto first_lease = drivers.continuation->lease().identity();
  drivers.continuation.reset();
  busy = false;
  try
  {
    (void)source->acquire_execution_drivers(handoff);
  }
  catch (const pkgapply::posix::target_mutation_lease_error& problem)
  {
    busy = problem.code() ==
        pkgapply::posix::target_mutation_lease_error_code::lock_busy;
  }
  CHECK(busy);
  drivers.resulting_state.reset();
  auto repeated = source->acquire_execution_drivers(handoff);
  CHECK(repeated.continuation != nullptr);
  CHECK(repeated.resulting_state != nullptr);
  CHECK(repeated.continuation && repeated.resulting_state &&
        repeated.continuation->lease().identity() ==
            repeated.resulting_state->lease().identity());
  CHECK(repeated.continuation &&
        repeated.continuation->lease().identity() != first_lease);

  bool invalid_descriptor = false;
  try
  {
    (void)pkgctl::posix_transaction_effect_driver_source::
        from_lock_directory_fd(
            -1, application_backend, lifecycle_backend,
            value.store, archives);
  }
  catch (const pkgctl::native_effect_source_error& problem)
  {
    invalid_descriptor = problem.code() ==
        pkgctl::native_effect_source_error_code::lock_directory_invalid;
  }
  CHECK(invalid_descriptor);
}

void check_native_effect_recovery_source()
{
  removal_fixture value;
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);
  unreachable_application_backend application_backend;
  scripted_execution_backend lifecycle_backend;
  forbidden_effect_archive_source archives;
  const auto locks = value.temp.path() / "native-recovery-locks";
  std::filesystem::create_directory(locks);
  const int lock_fd = open_directory(locks);
  auto source =
      pkgctl::posix_transaction_effect_driver_source::from_lock_directory_fd(
          lock_fd, application_backend, lifecycle_backend,
          value.store, archives);
  CHECK(::close(lock_fd) == 0);

  const auto admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(103U));
  const auto continuation_checkpoint = pkgctl::effect_restart_checkpoint::make(
      session, admission, {}, std::nullopt, {},
      std::nullopt, std::nullopt, std::nullopt);
  const auto continuation_handoff =
      make_operation_recovery_handoff(continuation_checkpoint, 103U);
  auto continuation =
      source->acquire_recovery_drivers(continuation_handoff);
  CHECK(continuation.continuation != nullptr);
  CHECK(continuation.resulting_state != nullptr);
  CHECK(continuation.publication == nullptr);
  CHECK(continuation.continuation && continuation.resulting_state &&
        continuation.continuation->lease().identity() ==
            continuation.resulting_state->lease().identity());
  CHECK(archives.calls() == 0U);
  continuation = {};

  driver actuator(value.projection, value.outer_lease,
                  value.receipt, value.store);
  memory_effect_journal_store journal;
  const auto completed = pkgctl::execute_effectful_operation_durable(
      session, effect_nonce(104U), actuator, journal);
  const auto completed_admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(104U));
  const auto completed_record = journal.load_latest(
      completed_admission.attempt());
  CHECK(completed_record.has_value());
  if (completed_record)
  {
    const auto terminal_checkpoint = pkgctl::effect_restart_checkpoint::make(
        session, *completed_record, completed.before(), completed.application(),
        completed.after(), completed.publication_request(),
        completed.publication_receipt(), std::nullopt);
    const auto terminal_handoff =
        make_operation_recovery_handoff(terminal_checkpoint, 104U);
    auto terminal = source->acquire_recovery_drivers(terminal_handoff);
    CHECK(terminal.continuation == nullptr);
    CHECK(terminal.resulting_state != nullptr);
    CHECK(terminal.publication == nullptr);
    CHECK(terminal.resulting_state &&
          terminal.resulting_state->read_state().identity() ==
              value.store.read().identity());
  }

  removal_fixture publication_value;
  const auto publication_session = pkgctl::effectful_operation_session::admit(
      effect_request(publication_value), publication_value.before,
      publication_value.after);
  driver interrupted(
      publication_value.projection, publication_value.outer_lease,
      publication_value.receipt, publication_value.store,
      std::nullopt, lease_release_point::never, publication_mode::native,
      crash_point::publication_before_store);
  memory_effect_journal_store publication_journal;
  try
  {
    (void)pkgctl::execute_effectful_operation_durable(
        publication_session, effect_nonce(105U), interrupted,
        publication_journal);
  }
  catch (const std::runtime_error&)
  {
  }
  const auto publication_admission = pkgctl::effect_attempt_record::admit(
      publication_session.identity(), publication_session.before().size(),
      publication_session.after().size(), effect_nonce(105U));
  const auto publication_record = publication_journal.load_latest(
      publication_admission.attempt());
  CHECK(publication_record.has_value());
  if (publication_record)
  {
    std::vector<pkgapply_exec::lifecycle_execution_result> before{
        interrupted.lifecycle_results().front()};
    std::vector<pkgapply_exec::lifecycle_execution_result> after{
        interrupted.lifecycle_results().back()};
    const auto publication_checkpoint =
        pkgctl::effect_restart_checkpoint::make(
            publication_session, *publication_record, std::move(before),
            publication_value.receipt, std::move(after),
            interrupted.last_publication_request(), std::nullopt,
            std::nullopt);
    const auto publication_handoff =
        make_operation_recovery_handoff(publication_checkpoint, 105U);

    forbidden_effect_archive_source publication_archives;
    const auto publication_locks =
        publication_value.temp.path() / "native-recovery-locks";
    std::filesystem::create_directory(publication_locks);
    const int publication_fd = open_directory(publication_locks);
    auto publication_source =
        pkgctl::posix_transaction_effect_driver_source::
            from_lock_directory_fd(
                publication_fd, application_backend, lifecycle_backend,
                publication_value.store, publication_archives);
    CHECK(::close(publication_fd) == 0);
    auto publication =
        publication_source->acquire_recovery_drivers(publication_handoff);
    CHECK(publication.continuation == nullptr);
    CHECK(publication.resulting_state == nullptr);
    CHECK(publication.publication != nullptr);
    CHECK(publication_archives.calls() == 0U);
  }
}

void check_operation_start_commit_protocol()
{
  removal_fixture value;
  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(value.transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, dispatch_journal_nonce(21U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(21U));
  auto reserved = admitted.successor(reservation.run);
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);

  std::vector<std::string> trace;
  recording_effect_store effect_store(trace);
  recording_run_store run_store(trace);
  const auto committed = pkgctl::commit_operation_dispatch_start(
      reserved, reservation.run, *reservation.dispatch, session,
      effect_nonce(21U), effect_store, run_store);
  CHECK(trace == std::vector<std::string>({"effect", "run"}));
  CHECK(committed.run_record.sequence() == reserved.sequence() + 1U);
  CHECK(committed.run_record.run() == committed.run.identity());
  CHECK(committed.run.records().front().effect_attempt() ==
        committed.effect_attempt.attempt());

  trace.clear();
  const auto repeated = pkgctl::commit_operation_dispatch_start(
      reserved, reservation.run, *reservation.dispatch, session,
      effect_nonce(21U), effect_store, run_store);
  CHECK(trace == std::vector<std::string>({"effect", "run"}));
  CHECK(repeated.effect_attempt.identity() ==
        committed.effect_attempt.identity());
  CHECK(repeated.run_record.identity() == committed.run_record.identity());
  CHECK(repeated.run.identity() == committed.run.identity());

  {
    trace.clear();
    const auto foreign = pkgctl::effect_attempt_record::admit(
        session.identity(), session.before().size(), session.after().size(),
        effect_nonce(22U));
    recording_effect_store foreign_effect(trace, foreign);
    recording_run_store untouched_run(trace);
    bool refused = false;
    try
    {
      (void)pkgctl::commit_operation_dispatch_start(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(21U), foreign_effect, untouched_run);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::
              store_contract_violation;
    }
    CHECK(refused);
    CHECK(trace == std::vector<std::string>({"effect"}));
  }

  {
    trace.clear();
    recording_effect_store exact_effect(trace);
    recording_run_store foreign_run(trace, reserved);
    bool refused = false;
    try
    {
      (void)pkgctl::commit_operation_dispatch_start(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(21U), exact_effect, foreign_run);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::
              store_contract_violation;
    }
    CHECK(refused);
    CHECK(trace == std::vector<std::string>({"effect", "run"}));
  }

  {
    trace.clear();
    recording_effect_store exact_effect(trace);
    recording_run_store interrupted_run(trace, std::nullopt, true);
    bool interrupted = false;
    try
    {
      (void)pkgctl::commit_operation_dispatch_start(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(23U), exact_effect, interrupted_run);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      interrupted = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(interrupted);
    CHECK(trace == std::vector<std::string>({"effect", "run"}));

    trace.clear();
    const auto retried = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(23U), exact_effect, interrupted_run);
    CHECK(trace == std::vector<std::string>({"effect", "run"}));
    CHECK(retried.run.records().front().effect_attempt() ==
          retried.effect_attempt.attempt());
  }
}

void check_operation_dispatch_ledger()
{
  {
    removal_fixture value;
    const auto run_directory = value.temp.path() / "dispatch-run";
    const auto effect_directory = value.temp.path() / "dispatch-effect";
    std::filesystem::create_directories(run_directory);
    std::filesystem::create_directories(effect_directory);
    auto run_store = pkgctl::posix_transaction_run_journal_store::open(
        run_directory.string());
    auto effect_store = pkgctl::posix_effect_journal_store::open(
        effect_directory.string());

    auto progress = pkgctl::transaction_progress::begin(value.transaction);
    auto run = pkgctl::transaction_run::begin(
        progress, pkgctl::transaction_dispatch_policy::make(2U, 2U));
    auto journal = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(31U));
    CHECK(run_store.append(journal).identity() == journal.identity());
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(31U));
    auto reserved_journal = journal.successor(reservation.run);
    CHECK(run_store.append(reserved_journal).identity() ==
          reserved_journal.identity());
    CHECK(reservation.dispatch.has_value());
    CHECK(reservation.dispatch &&
          reservation.dispatch->unit().kind() ==
              pkgctl::transaction_unit_kind::operation);
    CHECK(reservation.run.active_count(
              pkgctl::transaction_unit_kind::operation) == 1U);

    auto exhausted = pkgctl::reserve_next(
        reservation.run, dispatch_nonce(32U));
    CHECK(!exhausted.dispatch.has_value());

    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    removal_fixture foreign_value("1.1");
    const auto foreign_session = pkgctl::effectful_operation_session::admit(
        effect_request(foreign_value), foreign_value.before,
        foreign_value.after);
    bool foreign_session_refused = false;
    try
    {
      (void)pkgctl::start_operation_dispatch(
          reservation.run, *reservation.dispatch, foreign_session,
          effect_nonce(30U));
    }
    catch (const pkgctl::error& problem)
    {
      foreign_session_refused =
          problem.code() == pkgctl::error_code::invalid_dispatch;
    }
    CHECK(foreign_session_refused);

    auto start = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session,
        effect_nonce(31U));
    CHECK(start.effect_attempt.session() == session.identity());
    CHECK(start.effect_attempt.sequence() == 0U);
    CHECK(start.run.records().front().effect_attempt() ==
          start.effect_attempt.attempt());

    // The effect-attempt admission is the first durable write.  Until the
    // started run snapshot is committed, restart still owns only a releasable
    // reservation and must not infer that target mutation began.
    CHECK(effect_store.append(start.effect_attempt).identity() ==
          start.effect_attempt.identity());
    const auto before_started_commit =
        pkgctl::transaction_run_restart_checkpoint::make(
            reservation.run.progress(),
            *run_store.load_latest(journal.journal()));
    CHECK(before_started_commit.assessment().active().size() == 1U);
    CHECK(before_started_commit.assessment().active().front().disposition() ==
          pkgctl::transaction_dispatch_restart_disposition::release_reserved);
    CHECK(!before_started_commit.assessment().active().front().effect_attempt());
    CHECK(effect_store.load_latest(start.effect_attempt.attempt())->identity() ==
          start.effect_attempt.identity());

    auto started_journal = reserved_journal.successor(start.run);
    CHECK(run_store.append(started_journal).identity() ==
          started_journal.identity());
    auto started = std::move(start.run);
    const auto restart =
        pkgctl::transaction_run_restart_checkpoint::make(
            started.progress(), started_journal).assessment();
    CHECK(restart.active().size() == 1U);
    CHECK(restart.external_evidence_required());
    CHECK(restart.active().front().disposition() ==
          pkgctl::transaction_dispatch_restart_disposition::
              inspect_effect_journal);
    CHECK(restart.active().front().attempt_session() == session.identity());
    CHECK(restart.active().front().effect_attempt() ==
          start.effect_attempt.attempt());
    CHECK(effect_store.load_latest(start.effect_attempt.attempt())->identity() ==
          start.effect_attempt.identity());
    const auto encoded_started =
        pkgctl::encode_transaction_run_record(started_journal);
    const auto decoded_started =
        pkgctl::decode_transaction_run_record(encoded_started);
    CHECK(decoded_started.dispatches().front().effect_attempt() ==
          start.effect_attempt.attempt());
    CHECK(pkgctl::encode_transaction_run_record(decoded_started) ==
          encoded_started);
    const auto effect_attempt_bytes =
        identity_bytes(start.effect_attempt.attempt());
    const auto effect_attempt_position = std::search(
        encoded_started.begin(), encoded_started.end(),
        effect_attempt_bytes.begin(), effect_attempt_bytes.end());
    CHECK(effect_attempt_position != encoded_started.end());
    if (effect_attempt_position != encoded_started.end())
    {
      auto forged_started = encoded_started;
      const auto offset = static_cast<std::size_t>(
          std::distance(encoded_started.begin(), effect_attempt_position));
      forged_started[offset] ^= 0x01U;
      bool forged_refused = false;
      try
      {
        (void)pkgctl::decode_transaction_run_record(forged_started);
      }
      catch (const pkgctl::transaction_run_journal_error&)
      {
        forged_refused = true;
      }
      CHECK(forged_refused);
    }
    const auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
        started.progress(), started_journal);
    CHECK(checkpoint.run().identity() == started.identity());
    CHECK(started.records().front().state() ==
          pkgctl::transaction_dispatch_state::started);

    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    const auto result = pkgctl::execute_effectful_operation_durable(
        session, start.effect_attempt.nonce(), actuator, effect_store);
    CHECK(result.succeeded());
    const auto resulting_state = value.store.read();

    auto completed = pkgctl::submit_operation_dispatch_result(
        started, *reservation.dispatch, result, resulting_state);
    auto completed_journal = started_journal.successor(completed);
    CHECK(run_store.append(completed_journal).identity() ==
          completed_journal.identity());
    CHECK(pkgctl::transaction_run_restart_checkpoint::make(
              completed.progress(), completed_journal).assessment().quiescent());
    CHECK(completed.records().front().state() ==
          pkgctl::transaction_dispatch_state::completed);
    CHECK(completed.records().front().terminal_evidence() &&
          *completed.records().front().terminal_evidence() ==
              result.identity());
    CHECK(completed.active_count(pkgctl::transaction_unit_kind::operation) ==
          0U);
    CHECK(completed.progress().complete());
    CHECK(completed.progress().current_state().identity() ==
          resulting_state.identity());

    bool duplicate_refused = false;
    try
    {
      (void)pkgctl::submit_operation_dispatch_result(
          completed, *reservation.dispatch, result, resulting_state);
    }
    catch (const pkgctl::error& problem)
    {
      duplicate_refused =
          problem.code() == pkgctl::error_code::invalid_dispatch;
    }
    CHECK(duplicate_refused);
  }

  {
    // A committed started snapshot with a missing effect journal is not a
    // terminal fact.  Restart identifies the exact missing authority and
    // remains conservative.
    removal_fixture value;
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto journal = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(36U));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(36U));
    journal = journal.successor(reservation.run);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    auto first = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session, effect_nonce(36U));
    auto repeated = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session, effect_nonce(36U));
    auto second = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session, effect_nonce(37U));
    CHECK(first.effect_attempt.identity() == repeated.effect_attempt.identity());
    CHECK(first.run.identity() == repeated.run.identity());
    CHECK(first.effect_attempt.attempt() != second.effect_attempt.attempt());
    CHECK(first.run.identity() != second.run.identity());
    CHECK(first.run.records().front().identity() !=
          second.run.records().front().identity());

    auto started_journal = journal.successor(first.run);
    const auto restart = pkgctl::transaction_run_restart_checkpoint::make(
        first.run.progress(), started_journal).assessment();
    CHECK(restart.active().size() == 1U);
    CHECK(restart.active().front().disposition() ==
          pkgctl::transaction_dispatch_restart_disposition::
              inspect_effect_journal);
    CHECK(restart.active().front().effect_attempt() ==
          first.effect_attempt.attempt());

    const auto empty_effect_directory = value.temp.path() / "missing-effect";
    std::filesystem::create_directories(empty_effect_directory);
    auto empty_effect_store = pkgctl::posix_effect_journal_store::open(
        empty_effect_directory.string());
    CHECK(!empty_effect_store.load_latest(first.effect_attempt.attempt()));
  }

  {
    removal_fixture value;
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto journal = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(41U));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(41U));
    auto reserved_journal = journal.successor(reservation.run);
    CHECK(reservation.dispatch.has_value());

    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    auto start = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session,
        effect_nonce(41U));
    auto started = std::move(start.run);
    auto started_journal = reserved_journal.successor(started);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::never,
        publication_mode::indeterminate);
    const auto observation =
        pkgctl::execute_effectful_operation(session, actuator);
    CHECK(observation.outcome() ==
          pkgctl::effectful_operation_outcome::
              state_publication_indeterminate);

    bool state_on_observation_refused = false;
    try
    {
      (void)pkgctl::submit_operation_dispatch_result(
          started, *reservation.dispatch, observation, value.store.read());
    }
    catch (const pkgctl::error& problem)
    {
      state_on_observation_refused =
          problem.code() == pkgctl::error_code::invalid_dispatch;
    }
    CHECK(state_on_observation_refused);

    auto observed = pkgctl::submit_operation_dispatch_result(
        started, *reservation.dispatch, observation);
    auto observed_journal = started_journal.successor(observed);
    const auto observed_restart =
        pkgctl::transaction_run_restart_checkpoint::make(
            observed.progress(), observed_journal).assessment();
    CHECK(observed_restart.active().size() == 1U);
    CHECK(observed_restart.active().front().disposition() ==
          pkgctl::transaction_dispatch_restart_disposition::
              inspect_effect_journal);
    CHECK(observed_restart.active().front().effect_attempt() ==
          start.effect_attempt.attempt());
    CHECK(observed_restart.active().front().observations().size() == 1U);
    CHECK(observed_restart.active().front().observations().front() ==
          observation.identity());
    CHECK(observed.records().front().state() ==
          pkgctl::transaction_dispatch_state::started);
    CHECK(observed.records().front().observations().size() == 1U);
    CHECK(observed.records().front().observations().front() ==
          observation.identity());
    CHECK(observed.active_count(pkgctl::transaction_unit_kind::operation) ==
          1U);
    CHECK(observed.progress().identity() == started.progress().identity());

    auto no_second_operation = pkgctl::reserve_next(
        observed, dispatch_nonce(42U));
    CHECK(!no_second_operation.dispatch.has_value());

    bool duplicate_observation_refused = false;
    try
    {
      (void)pkgctl::submit_operation_dispatch_result(
          observed, *reservation.dispatch, observation);
    }
    catch (const pkgctl::error& problem)
    {
      duplicate_observation_refused =
          problem.code() == pkgctl::error_code::invalid_dispatch;
    }
    CHECK(duplicate_observation_refused);

    bool release_refused = false;
    try
    {
      (void)pkgctl::release_unstarted_dispatch(
          observed, *reservation.dispatch);
    }
    catch (const pkgctl::error& problem)
    {
      release_refused =
          problem.code() == pkgctl::error_code::invalid_dispatch;
    }
    CHECK(release_refused);
  }

  {
    removal_fixture value;
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(51U));
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    auto start = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session,
        effect_nonce(51U));
    auto started = std::move(start.run);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        pkgsource::lifecycle_action::pre_remove);
    const auto failure = pkgctl::execute_effectful_operation(session, actuator);
    CHECK(failure.outcome() ==
          pkgctl::effectful_operation_outcome::
              lifecycle_failed_before_application);
    auto completed = pkgctl::submit_operation_dispatch_result(
        started, *reservation.dispatch, failure);
    CHECK(completed.records().front().state() ==
          pkgctl::transaction_dispatch_state::completed);
    CHECK(completed.progress().failed());
    CHECK(completed.progress().current_state().identity() ==
          value.expected.identity());
  }

  {
    removal_fixture value;
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(61U));
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    auto start = pkgctl::start_operation_dispatch(
        reservation.run, *reservation.dispatch, session,
        effect_nonce(61U));
    auto started = std::move(start.run);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::after_application);
    const auto observation =
        pkgctl::execute_effectful_operation(session, actuator);
    CHECK(observation.outcome() ==
          pkgctl::effectful_operation_outcome::outer_lease_lost);
    auto observed = pkgctl::submit_operation_dispatch_result(
        started, *reservation.dispatch, observation);
    CHECK(observed.records().front().state() ==
          pkgctl::transaction_dispatch_state::started);
    CHECK(observed.records().front().observations().size() == 1U);
    CHECK(observed.active_count(pkgctl::transaction_unit_kind::operation) ==
          1U);
  }
}


void check_durable_operation_execution()
{
  const auto reserve_operation = [](const removal_fixture& value,
                                    std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::operation)
      throw std::runtime_error("fixture did not reserve an operation dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 71U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    tracing_effect_driver traced_driver(actuator, trace);

    const auto completed = pkgctl::execute_operation_dispatch_durable(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(71U), traced_driver, actuator, effect_store, run_store);

    CHECK(trace.size() >= 4U);
    if (trace.size() >= 4U)
    {
      CHECK(trace[0] == "effect-1");
      CHECK(trace[1] == "run-1");
      const auto driver_position = std::find(
          trace.begin(), trace.end(), "driver");
      CHECK(driver_position != trace.end());
      if (driver_position != trace.end())
        CHECK(driver_position > trace.begin() + 1);
      CHECK(trace.back() == "run-2");
    }
    CHECK(completed.result.succeeded());
    CHECK(completed.record.sequence() == reserved.sequence() + 2U);
    CHECK(completed.run.records().size() == 1U);
    if (completed.run.records().size() == 1U)
    {
      CHECK(completed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
      CHECK(completed.run.records().front().effect_attempt() ==
            std::optional<pkgctl::session_identity>(
                completed.admission.attempt()));
    }
    CHECK(effect_store.latest().stage() == pkgctl::effect_attempt_stage::terminal);
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 75U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace, 1U);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    tracing_effect_driver traced_driver(actuator, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(75U), traced_driver, actuator, effect_store, run_store);
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::effect_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"effect-1"}));
    CHECK(actuator.trace().empty());
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 72U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace, 1U);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    tracing_effect_driver traced_driver(actuator, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(72U), traced_driver, actuator, effect_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"effect-1", "run-1"}));
    CHECK(actuator.trace().empty());
    CHECK(run_store.latest().identity() == reserved.identity());
    CHECK(effect_store.latest().stage() ==
          pkgctl::effect_attempt_stage::admitted);
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 73U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace, 2U);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    tracing_effect_driver traced_driver(actuator, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(73U), traced_driver, actuator, effect_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace.size() >= 3U);
    if (trace.size() >= 3U)
    {
      CHECK(trace[0] == "effect-1");
      CHECK(trace[1] == "run-1");
      CHECK(trace.back() == "run-2");
    }
    CHECK(!actuator.trace().empty());
    CHECK(effect_store.latest().stage() == pkgctl::effect_attempt_stage::terminal);
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                inspect_effect_journal);
    }
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 76U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::never, publication_mode::native,
        crash_point::application);
    tracing_effect_driver traced_driver(actuator, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::execute_operation_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch, session,
          effect_nonce(76U), traced_driver, actuator, effect_store, run_store);
    }
    catch (const std::runtime_error&)
    {
      failed = true;
    }
    CHECK(failed);
    CHECK(trace.size() >= 3U);
    if (trace.size() >= 3U)
    {
      CHECK(trace[0] == "effect-1");
      CHECK(trace[1] == "run-1");
      const auto driver_position = std::find(
          trace.begin(), trace.end(), "driver");
      CHECK(driver_position != trace.end());
      if (driver_position != trace.end())
        CHECK(driver_position > trace.begin() + 1);
    }
    CHECK(std::find(trace.begin(), trace.end(), "run-2") == trace.end());
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    CHECK(effect_store.latest().stage() ==
          pkgctl::effect_attempt_stage::application_intent);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                inspect_effect_journal);
    }
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 74U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        std::nullopt, lease_release_point::after_application);
    tracing_effect_driver traced_driver(actuator, trace);

    const auto observed = pkgctl::execute_operation_dispatch_durable(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(74U), traced_driver, actuator, effect_store, run_store);
    CHECK(observed.result.outcome() ==
          pkgctl::effectful_operation_outcome::outer_lease_lost);
    CHECK(observed.run.records().size() == 1U);
    if (observed.run.records().size() == 1U)
    {
      CHECK(observed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::started);
      CHECK(observed.run.records().front().observations().size() == 1U);
    }
    CHECK(observed.record.sequence() == reserved.sequence() + 2U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        observed.run.progress(), observed.record).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                inspect_effect_journal);
    }
  }
}


void check_durable_operation_reconciliation()
{
  const auto reserve_operation = [](const removal_fixture& value,
                                    std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::operation)
      throw std::runtime_error("fixture did not reserve an operation dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  const auto restart_checkpoint = [](
      const pkgctl::effectful_operation_session& session,
      const pkgctl::effect_attempt_record& record,
      const pkgctl::effectful_operation_result& result) {
    return pkgctl::effect_restart_checkpoint::make(
        session, record, result.before(), result.application(),
        result.after(), result.publication_request(),
        result.publication_receipt());
  };

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 81U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(81U), effect_store, run_store);

    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    const auto result = pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(81U), actuator, effect_store);
    CHECK(result.succeeded());
    const auto driver_trace = actuator.trace();
    const auto publication_calls = actuator.publication_calls();

    const auto reconciled = pkgctl::reconcile_operation_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started.run.progress(), started.run_record),
        *reservation.dispatch,
        restart_checkpoint(session, effect_store.latest(), result),
        nullptr, &actuator, nullptr, effect_store, run_store);

    CHECK(reconciled.run_advanced);
    CHECK(reconciled.disposition ==
          pkgctl::effect_restart_disposition::terminal);
    CHECK(reconciled.result.has_value());
    CHECK(reconciled.result && reconciled.result->identity() == result.identity());
    CHECK(reconciled.effect_record.identity() == effect_store.latest().identity());
    CHECK(reconciled.record.sequence() == started.run_record.sequence() + 1U);
    CHECK(reconciled.run.records().size() == 1U);
    if (reconciled.run.records().size() == 1U)
      CHECK(reconciled.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
    CHECK(actuator.trace() == driver_trace);
    CHECK(actuator.publication_calls() == publication_calls);
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 82U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace, 2U);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(82U), effect_store, run_store);

    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    const auto result = pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(82U), actuator, effect_store);
    const auto driver_trace = actuator.trace();
    const auto effect_checkpoint = restart_checkpoint(
        session, effect_store.latest(), result);

    bool failed = false;
    try
    {
      (void)pkgctl::reconcile_operation_dispatch_durable(
          pkgctl::transaction_run_restart_checkpoint::make(
              started.run.progress(), started.run_record),
          *reservation.dispatch, effect_checkpoint,
          nullptr, &actuator, nullptr, effect_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(run_store.latest().identity() == started.run_record.identity());
    CHECK(actuator.trace() == driver_trace);

    const auto reconciled = pkgctl::reconcile_operation_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started.run.progress(), started.run_record),
        *reservation.dispatch, effect_checkpoint,
        nullptr, &actuator, nullptr, effect_store, run_store);
    CHECK(reconciled.run_advanced);
    CHECK(reconciled.record.sequence() == started.run_record.sequence() + 1U);
    CHECK(actuator.trace() == driver_trace);
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 83U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(83U), effect_store, run_store);

    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    const auto reconciled = pkgctl::reconcile_operation_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started.run.progress(), started.run_record),
        *reservation.dispatch,
        pkgctl::effect_restart_checkpoint::make(
            session, effect_store.latest(), {}, std::nullopt, {},
            std::nullopt, std::nullopt),
        &actuator, &actuator, nullptr, effect_store, run_store);

    CHECK(reconciled.run_advanced);
    CHECK(reconciled.result && reconciled.result->succeeded());
    CHECK(reconciled.effect_record.stage() ==
          pkgctl::effect_attempt_stage::terminal);
    CHECK(!actuator.trace().empty());
    CHECK(reconciled.record.sequence() == started.run_record.sequence() + 1U);
  }

  {
    removal_fixture value;
    auto [reservation, reserved] = reserve_operation(value, 84U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(84U), effect_store, run_store);
    const auto intent = effect_store.append(
        effect_store.latest().begin_before(0U));
    const auto trace_before = trace;

    const auto unresolved = pkgctl::reconcile_operation_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started.run.progress(), started.run_record),
        *reservation.dispatch,
        pkgctl::effect_restart_checkpoint::make(
            session, intent, {}, std::nullopt, {},
            std::nullopt, std::nullopt),
        nullptr, nullptr, nullptr, effect_store, run_store);

    CHECK(!unresolved.run_advanced);
    CHECK(unresolved.disposition ==
          pkgctl::effect_restart_disposition::external_resolution_required);
    CHECK(!unresolved.result);
    CHECK(unresolved.record.identity() == started.run_record.identity());
    CHECK(unresolved.effect_record.identity() == intent.identity());
    CHECK(trace == trace_before);
  }
}


void check_single_step_operation_advancement()
{
  const auto make_admitted = [](const removal_fixture& value,
                                std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  const auto restart_checkpoint = [](
      const pkgctl::effectful_operation_session& session,
      const pkgctl::effect_attempt_record& record,
      const pkgctl::effectful_operation_result& result) {
    return pkgctl::effect_restart_checkpoint::make(
        session, record, result.before(), result.application(),
        result.after(), result.publication_request(),
        result.publication_receipt());
  };

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 101U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    fixed_effect_progress_source progress_source(run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(101U));
    unreachable_effect_recovery_source recovery_source;
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    fixed_effect_driver_source driver_source(actuator, trace);

    const auto advanced = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(101U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});

    CHECK(advanced.disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_operation);
    CHECK(advanced.durable_transition_committed());
    CHECK(!advanced.external_resolution_required());
    CHECK(advanced.dispatch().has_value());
    CHECK(advanced.construction() == nullptr);
    CHECK(advanced.check() == nullptr);
    CHECK(advanced.operation() != nullptr);
    if (advanced.operation())
    {
      CHECK(advanced.operation()->result.has_value());
      CHECK(advanced.operation()->result &&
            advanced.operation()->result->succeeded());
      CHECK(!advanced.operation()->restart_disposition.has_value());
      CHECK(advanced.operation()->record.stage() ==
            pkgctl::effect_attempt_stage::admitted);
    }
    CHECK(advanced.record().sequence() == 3U);
    CHECK(advanced.record().identity() == run_store.latest().identity());
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 1U);
    CHECK(driver_source.execution_calls() == 1U);
    CHECK(driver_source.recovery_calls() == 0U);
    CHECK(driver_source.execution_handoff().has_value());
    CHECK(!trace.empty());
    CHECK(trace.front() == "run-1");
    const auto driver_source_call = std::find(
        trace.begin(), trace.end(), "driver-source-execution");
    const auto effect_admission = std::find(
        trace.begin(), trace.end(), "effect-1");
    const auto started_run = std::find(trace.begin(), trace.end(), "run-2");
    const auto driver_call = std::find(trace.begin(), trace.end(), "driver");
    const auto state_observation = std::find(
        trace.begin(), trace.end(), "state-observer");
    CHECK(driver_source_call != trace.end());
    CHECK(effect_admission != trace.end());
    CHECK(started_run != trace.end());
    CHECK(driver_call != trace.end());
    CHECK(state_observation != trace.end());
    if (driver_source_call != trace.end() && effect_admission != trace.end() &&
        started_run != trace.end() && driver_call != trace.end() &&
        state_observation != trace.end())
    {
      CHECK(trace.begin() < driver_source_call);
      CHECK(driver_source_call < effect_admission);
      CHECK(effect_admission < started_run);
      CHECK(started_run < driver_call);
      CHECK(driver_call < state_observation);
      CHECK(state_observation < trace.end() - 1);
    }
    CHECK(trace.back() == "run-3");
  }

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 106U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    fixed_effect_progress_source progress_source(run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(106U));
    unreachable_effect_recovery_source recovery_source;
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    rejecting_effect_driver_source driver_source(trace);

    bool rejected = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(106U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});
    }
    catch (const std::runtime_error& problem)
    {
      rejected = std::string(problem.what()) ==
          "injected effect-driver source refusal";
    }
    CHECK(rejected);
    CHECK(execution_source.calls() == 1U);
    CHECK(driver_source.execution_calls() == 1U);
    CHECK(driver_source.recovery_calls() == 0U);
    CHECK(trace == std::vector<std::string>(
        {"run-1", "driver-source-rejected"}));
    CHECK(run_store.latest().sequence() == admitted.sequence() + 1U);
    CHECK(run_store.latest().reopen(run.progress()).records().size() == 1U);
    if (run_store.latest().reopen(run.progress()).records().size() == 1U)
    {
      CHECK(run_store.latest().reopen(run.progress()).records().front().state() ==
            pkgctl::transaction_dispatch_state::reserved);
    }
  }

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 107U);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    fixed_effect_progress_source progress_source(run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(107U));
    unreachable_effect_recovery_source recovery_source;
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    value.outer_lease.release();
    fixed_effect_driver_source driver_source(actuator, trace);

    bool rejected = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(107U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(rejected);
    CHECK(driver_source.execution_calls() == 1U);
    CHECK(trace == std::vector<std::string>(
        {"run-1", "driver-source-execution"}));
    CHECK(run_store.latest().sequence() == admitted.sequence() + 1U);
  }

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 102U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(102U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("operation recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(102U), effect_store, run_store);
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    const auto result = pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(102U), actuator, effect_store);
    const auto driver_trace = actuator.trace();
    const auto trace_before = trace;

    fixed_effect_progress_source progress_source(started.run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(103U));
    operation_recovery_authority_source recovery_source(
        restart_checkpoint(session, effect_store.latest(), result));
    fixed_effect_driver_source driver_source(actuator, trace);

    const auto reconciled = pkgctl::advance_transaction_run_once(
        started.run_record.journal(), dispatch_nonce(103U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});

    CHECK(reconciled.disposition() ==
          pkgctl::transaction_run_advance_disposition::reconciled_operation);
    CHECK(reconciled.operation() != nullptr);
    if (reconciled.operation())
    {
      CHECK(reconciled.operation()->result.has_value());
      CHECK(reconciled.operation()->result &&
            reconciled.operation()->result->identity() == result.identity());
      CHECK(reconciled.operation()->restart_disposition ==
            std::optional<pkgctl::effect_restart_disposition>(
                pkgctl::effect_restart_disposition::terminal));
      CHECK(reconciled.operation()->record.identity() ==
            effect_store.latest().identity());
    }
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(driver_source.execution_calls() == 0U);
    CHECK(driver_source.recovery_calls() == 1U);
    CHECK(driver_source.recovery_handoff().has_value());
    CHECK(actuator.trace() == driver_trace);
    CHECK(trace.size() == trace_before.size() + 3U);
    CHECK(trace[trace_before.size()] == "driver-source-recovery");
    CHECK(trace[trace_before.size() + 1U] == "state-observer");
    CHECK(trace.back() == "run-2");
  }

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 108U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(108U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("operation failure fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(108U), effect_store, run_store);
    if (value.before.empty())
      throw std::runtime_error("operation failure fixture has no pre-lifecycle");
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store,
        value.before.front().node().action());
    const auto result = pkgctl::execute_effectful_operation_durable(
        session, effect_nonce(108U), actuator, effect_store);
    CHECK(!result.succeeded());
    CHECK(result.outcome() ==
          pkgctl::effectful_operation_outcome::
              lifecycle_failed_before_application);
    const auto trace_before = trace;

    fixed_effect_progress_source progress_source(started.run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(109U));
    operation_recovery_authority_source recovery_source(
        restart_checkpoint(session, effect_store.latest(), result));
    rejecting_effect_driver_source driver_source(trace);

    const auto reconciled = pkgctl::advance_transaction_run_once(
        started.run_record.journal(), dispatch_nonce(109U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});

    CHECK(reconciled.disposition() ==
          pkgctl::transaction_run_advance_disposition::reconciled_operation);
    CHECK(reconciled.operation() != nullptr);
    CHECK(reconciled.operation() && reconciled.operation()->result &&
          !reconciled.operation()->result->succeeded());
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(driver_source.execution_calls() == 0U);
    CHECK(driver_source.recovery_calls() == 0U);
    CHECK(trace.size() == trace_before.size() + 1U);
    CHECK(trace.back() == "run-2");
  }

  {
    removal_fixture value;
    auto [run, admitted] = make_admitted(value, 104U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(104U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("operation external fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    const auto session = pkgctl::effectful_operation_session::admit(
        effect_request(value), value.before, value.after);
    std::vector<std::string> trace;
    sequenced_effect_store effect_store(trace);
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    const auto started = pkgctl::commit_operation_dispatch_start(
        reserved, reservation.run, *reservation.dispatch, session,
        effect_nonce(104U), effect_store, run_store);
    const auto intent = effect_store.append(
        effect_store.latest().begin_before(0U));
    const auto trace_before = trace;

    fixed_effect_progress_source progress_source(started.run.progress());
    operation_execution_authority_source execution_source(
        session, effect_nonce(105U));
    operation_recovery_authority_source recovery_source(
        pkgctl::effect_restart_checkpoint::make(
            session, intent, {}, std::nullopt, {},
            std::nullopt, std::nullopt));
    driver actuator(
        value.projection, value.outer_lease, value.receipt, value.store);
    rejecting_effect_driver_source driver_source(trace);

    const auto unresolved = pkgctl::advance_transaction_run_once(
        started.run_record.journal(), dispatch_nonce(105U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});

    CHECK(unresolved.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              external_resolution_required);
    CHECK(!unresolved.durable_transition_committed());
    CHECK(unresolved.external_resolution_required());
    CHECK(unresolved.record().identity() == started.run_record.identity());
    CHECK(unresolved.operation() != nullptr);
    if (unresolved.operation())
    {
      CHECK(!unresolved.operation()->result.has_value());
      CHECK(unresolved.operation()->restart_disposition ==
            std::optional<pkgctl::effect_restart_disposition>(
                pkgctl::effect_restart_disposition::
                    external_resolution_required));
      CHECK(unresolved.operation()->record.identity() == intent.identity());
    }
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(driver_source.execution_calls() == 0U);
    CHECK(driver_source.recovery_calls() == 0U);
    CHECK(trace == trace_before);
    CHECK(actuator.trace().empty());

    forbidden_dispatch_nonce_source nonces;
    const auto driven = pkgctl::drive_transaction_run(
        started.run_record.journal(),
        pkgctl::transaction_run_drive_policy::make(4U), nonces,
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, &driver_source}, {run_store, evidence_store, &effect_store});
    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::
              external_resolution_required);
    CHECK(driven.external_resolution_required());
    CHECK(!driven.terminal());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.durable_step_count() == 0U);
    CHECK(driven.record().identity() == started.run_record.identity());
    CHECK(nonces.calls() == 0U);
    CHECK(recovery_source.calls() == 2U);
    CHECK(driver_source.execution_calls() == 0U);
    CHECK(driver_source.recovery_calls() == 0U);
    CHECK(trace == trace_before);
    CHECK(actuator.trace().empty());
  }
}


void check_run_authority_rehydration()
{
  const auto reserve_operation = [](const removal_fixture& value,
                                    std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(value.transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, dispatch_journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::operation)
      throw std::runtime_error("fixture did not reserve an operation dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  removal_fixture value;
  auto [reservation, reserved] = reserve_operation(value, 91U);
  const auto session = pkgctl::effectful_operation_session::admit(
      effect_request(value), value.before, value.after);

  operation_execution_authority_source execution_source(
      session, effect_nonce(91U));
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
  CHECK(execution.kind() == pkgctl::transaction_unit_kind::operation);
  CHECK(execution.construction() == nullptr);
  CHECK(execution.check() == nullptr);
  CHECK(execution.operation() != nullptr);
  if (execution.operation())
  {
    CHECK(execution.operation()->session.identity() == session.identity());
    CHECK(execution.operation()->nonce == effect_nonce(91U));
  }

  operation_execution_authority_source alternate_nonce(
      session, effect_nonce(92U));
  const auto alternate =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch,
          alternate_nonce);
  CHECK(alternate.operation() != nullptr);
  CHECK(alternate.identity() != execution.identity());

  auto start = pkgctl::start_operation_dispatch(
      reservation.run, *reservation.dispatch, session,
      effect_nonce(91U));
  auto started_record = reserved.successor(start.run);
  auto effect_checkpoint = pkgctl::effect_restart_checkpoint::make(
      session, start.effect_attempt, {}, std::nullopt, {},
      std::nullopt, std::nullopt);
  operation_recovery_authority_source recovery_source(effect_checkpoint);
  const auto recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              start.run.progress(), started_record),
          *reservation.dispatch, recovery_source);
  CHECK(recovery_source.calls() == 1U);
  CHECK(recovery_source.record() ==
        std::optional<pkgctl::session_identity>(started_record.identity()));
  CHECK(recovery_source.assessment() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery.disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::
            inspect_effect_journal);
  CHECK(!recovery.releases_reserved());
  CHECK(recovery.construction() == nullptr);
  CHECK(recovery.check() == nullptr);
  CHECK(recovery.operation() != nullptr);
  if (recovery.operation())
  {
    CHECK(recovery.operation()->record().identity() ==
          start.effect_attempt.identity());
    CHECK(recovery.operation()->session().identity() == session.identity());
  }

  std::vector<std::string> recovery_trace;
  run_execute_support::sequenced_evidence_store unused_evidence(recovery_trace);
  operation_recovery_context_source operation_context(effect_checkpoint);
  pkgctl::stored_transaction_dispatch_recovery_authority_source stored_recovery(
      unused_evidence, operation_context);
  const auto delegated =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              start.run.progress(), started_record),
          *reservation.dispatch, stored_recovery);
  CHECK(operation_context.calls() == 1U);
  CHECK(delegated.operation() != nullptr);
  CHECK(delegated.operation() &&
        delegated.operation()->record().identity() ==
            start.effect_attempt.identity());

  auto foreign_admission = pkgctl::effect_attempt_record::admit(
      session.identity(), session.before().size(), session.after().size(),
      effect_nonce(93U));
  operation_recovery_authority_source foreign_recovery(
      pkgctl::effect_restart_checkpoint::make(
          session, foreign_admission, {}, std::nullopt, {},
          std::nullopt, std::nullopt));
  bool refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            start.run.progress(), started_record),
        *reservation.dispatch, foreign_recovery);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
}


} // namespace

int main()
{
  check_success();
  check_upgrade_success();
  check_removal_preparation();
  check_removal_preparation_refusal();
  check_removal_success();
  check_removal_progression_failure();
  check_request_refusal();
  check_application_failure();
  check_lifecycle_failures();
  check_publication_failures();
  check_lease_loss();
  check_durable_success();
  check_operation_progress_rehydration();
  check_restart_boundaries();
  check_publication_retry();
  check_publication_reconciliation();
  check_operation_start_commit_protocol();
  check_operation_dispatch_ledger();
  check_durable_operation_execution();
  check_durable_operation_reconciliation();
  check_run_authority_rehydration();
  check_native_effect_archive_source();
  check_native_effect_driver_source();
  check_native_effect_recovery_source();
  check_single_step_operation_advancement();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
