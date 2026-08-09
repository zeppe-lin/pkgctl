// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <pkgctl/preparation.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace construction_fixture;

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

const pkgtransaction::transaction_node& upgrade_node(
    const pkgctl::transaction_session& session)
{
  for (const auto& node : session.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::upgrade &&
        node.package().name() == "tool")
      return node;
  }
  throw std::runtime_error("transaction fixture lacks upgrade node");
}

pkgstate::installed_object_metadata old_tool_object(
    const pkgstate::installed_regular_content_identity& content)
{
  return pkgstate::installed_object_metadata(
      pkgstate::owned_object_kind::regular,
      0755, 0, 0, pkgstate::installed_object_timestamp(1700000000, 0),
      std::uint64_t{9}, content);
}

pkgplan::filesystem_object_metadata observed_old_tool(
    const pkgstate::installed_regular_content_identity& content)
{
  return pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::regular,
      0755, 0, 0, std::uint64_t{9}, pkgplan::object_timestamp(1700000000, 0),
      translate_identity<pkgplan::filesystem_regular_content_identity>(
          content));
}

pkgplan::target_observation_set upgrade_target_observations(
    const pkgplan::target_system_context_identity& target,
    const pkgstate::installed_regular_content_identity& old_content)
{
  const auto directory = pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::directory, 0755, 0, 0);
  std::vector<pkgplan::target_path_observation> observations;
  observations.push_back(pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          pkgplan::package_path::parse("usr"), directory)));
  observations.push_back(pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          pkgplan::package_path::parse("usr/bin"), directory)));
  observations.push_back(pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          pkgplan::package_path::parse("usr/bin/tool"),
          observed_old_tool(old_content))));
  return pkgplan::target_observation_set(
      plan_identity<pkgplan::observation_set_identity>(71), target,
      pkgplan::fact_set_completeness::complete, std::move(observations));
}

pkgplan::package_policy_snapshot protected_tool_policy()
{
  const auto protected_path = pkgplan::normalized_path_policy(
      pkgplan::incoming_path_policy::retain(
          pkgplan::rejected_object_policy::stage,
          pkgplan::retained_active_ownership_policy::do_not_claim_operated_package),
      pkgplan::obsolete_path_policy::remove(),
      pkgplan::shared_ownership_policy::forbid,
      pkgplan::directory_cleanup_policy::remove_if_empty);
  return pkgplan::package_policy_snapshot(
      plan_identity<pkgplan::policy_snapshot_identity>(72),
      pkgplan::normalized_path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(),
          pkgplan::shared_ownership_policy::forbid,
          pkgplan::directory_cleanup_policy::remove_if_empty),
      {pkgplan::path_policy_override(
          pkgplan::package_path::parse("usr/bin/tool"), protected_path)});
}

const pkgplan::upgrade_path_decision& path_decision(
    const pkgplan::upgrade_plan& plan, const pkgplan::package_path& path)
{
  for (const auto& decision : plan.paths())
    if (decision.path() == path)
      return decision;
  throw std::runtime_error("prepared upgrade lacks expected path decision");
}

void check_protected_upgrade_policy_handoff()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");

  const auto state_target = test_support::binding();
  const auto old_content =
      fixture_state_identity<pkgstate::installed_regular_content_identity>(70U);
  const auto old_source = tool_source(
      sha256_text("old source payload\n"), "1.0", false);
  const auto old_package = installed_package(
      old_source, state_target, 30U,
      {pkgstate::owned_entry::make(
          pkgstate::package_path::parse("usr/bin/tool"),
          old_tool_object(old_content),
          pkgstate::active_object_origin::incoming_payload)});
  const auto installed = pkgstate::snapshot::make(
      state_target, {old_package});

  const std::string payload = "new source payload\n";
  const auto source = tool_source(sha256_text(payload), "2.0", false);
  const auto transaction = transaction_session(
      source, std::vector<pkgsource::source_snapshot>{}, installed,
      temporary.path() / "state", true, false, false, "/collection",
      pkgresolve::resolution_policy(
          pkgresolve::installed_preference::prefer_catalog));
  const auto& action = upgrade_node(transaction);

  auto construction_session_value = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(
      construction_session_value.paths().local_source_root / "payload", payload);
  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(backend);
  const auto construction = pkgctl::execute_construction(
      construction_session_value, construction_driver);

  auto progression = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.status(action.identity()) ==
        pkgctl::transaction_node_status::ready);

  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(73);
  const auto policy = protected_tool_policy();
  const auto policy_identity = policy.identity();
  const auto observations = upgrade_target_observations(
      target_system, old_content);
  const auto observation_identity = observations.identity();

  auto request = pkgctl::operation_preparation_request::upgrade(
      progression, action.identity(), construction,
      application_target(installed.target_binding(), target_system),
      execution_control(), observations,
      plan_identity<pkgplan::runtime_dependency_closure_identity>(74),
      policy, pkgctl::lifecycle_order::make({}, {}));
  CHECK(request.policy().identity() == policy_identity);

  pkgctl::native_operation_preparation_driver preparation_driver;
  const auto result = pkgctl::prepare_operation(
      std::move(request), preparation_driver);

  CHECK(result.prepared());
  CHECK(!result.refusal());
  CHECK(result.plan().has_value());
  CHECK(result.application().has_value());
  CHECK(result.effect().has_value());
  CHECK(result.request().policy().identity() == policy_identity);
  CHECK(result.plan()->kind() == pkgplan::operation_kind::upgrade);

  const auto* plan = result.plan()->upgrade();
  CHECK(plan != nullptr);
  if (plan == nullptr)
    return;

  CHECK(plan->inputs().policy() == policy_identity);
  CHECK(plan->inputs().observations() == observation_identity);
  CHECK(plan->inputs().old_package() ==
        translate_identity<pkgplan::installed_package_identity>(
            old_package.identity()));

  const auto protected_path = pkgplan::package_path::parse("usr/bin/tool");
  const auto& decision = path_decision(*plan, protected_path);
  CHECK(decision.active() == pkgplan::planned_active_outcome::retain_observed);
  CHECK(decision.rejected() == pkgplan::planned_rejected_outcome::stage_incoming);
  CHECK(decision.rejected_object().has_value());
  CHECK(decision.rejected_object()->reason() ==
        pkgplan::rejected_object_reason::upgrade_incoming_protected);
  CHECK(decision.rejected_object()->source_side() ==
        pkgplan::rejected_object_source_side::incoming);
  CHECK(decision.rejected_object()->observations() == observation_identity);
  CHECK(decision.rejected_object()->incoming_source() != nullptr);
  CHECK(decision.ownership().before_existing_owners().size() == 1U);
  CHECK(decision.ownership().after_existing_owners().empty());
  CHECK(!decision.ownership().incoming_package_owns_after());
  CHECK(std::find(plan->publication().installed_manifest().begin(),
                  plan->publication().installed_manifest().end(),
                  protected_path) ==
        plan->publication().installed_manifest().end());

  const auto* application = result.application()->upgrade();
  CHECK(application != nullptr);
  if (application != nullptr)
  {
    CHECK(application->plan().identity() == plan->identity());
    CHECK(application->plan().inputs().policy() == policy_identity);
    const auto& application_decision =
        path_decision(application->plan(), protected_path);
    CHECK(application_decision.active() ==
          pkgplan::planned_active_outcome::retain_observed);
    CHECK(application_decision.rejected() ==
          pkgplan::planned_rejected_outcome::stage_incoming);
    CHECK(application_decision.rejected_object().has_value());
    CHECK(application_decision.rejected_object()->reason() ==
          pkgplan::rejected_object_reason::upgrade_incoming_protected);
  }

  CHECK(result.effect()->application().identity() ==
        result.application()->identity());
  CHECK(result.effect()->action_node() == action.identity());
}

} // namespace

int main()
{
  try
  {
    check_protected_upgrade_policy_handoff();
  }
  catch (const std::exception& value)
  {
    std::cerr << "unexpected exception: " << value.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
