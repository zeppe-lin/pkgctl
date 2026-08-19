// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"

#include <cstdlib>
#include <string_view>

namespace {

using namespace construction_fixture;

#define CHECK(condition) \
  do \
  { \
    if (!(condition)) \
      std::abort(); \
  } while (false)

bool has_build_node(
    const pkgctl::transaction_session& transaction,
    std::string_view package)
{
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == package)
      return true;
  }
  return false;
}

pkgctl::transaction_session make_transaction(
    const pkgsource::source_snapshot& tool,
    const pkgsource::source_snapshot& dependency,
    const pkgstate::snapshot& installed,
    pkgresolve::installed_preference preference)
{
  return transaction_session(
      tool, dependency, installed, "/state", false, false, true,
      "/collection", pkgresolve::resolution_policy(preference));
}

} // namespace

int main()
{
  tool_source_options options;
  options.check_dependencies = {"dep"};
  options.check_program = pkgsource::program(
      pkgsource::program_language::posix_shell, "true\n");
  const auto tool = tool_source(sha256_text("tool source\n"), std::move(options));
  const auto dependency = dependency_source();
  const auto binding = test_support::binding();
  const auto retained_tool = installed_package(tool, binding);
  const auto retained_dependency = installed_package(dependency, binding);
  const auto installed = pkgstate::snapshot::make(
      binding, {retained_tool, retained_dependency});

  const auto transaction = make_transaction(
      tool, dependency, installed,
      pkgresolve::installed_preference::retain_compatible);
  CHECK(transaction.request().resolution().policy().preference() ==
        pkgresolve::installed_preference::retain_compatible);

  const auto& resolution = transaction.resolution().resolution();
  const auto* selected_tool = resolution.find(
      pkgsource::package_reference("tool"),
      pkgresolve::resolution_environment::target,
      pkgresolve::selection_authority_kind::catalog_candidate);
  CHECK(selected_tool != nullptr);
  CHECK(resolution.find(
            pkgsource::package_reference("tool"),
            pkgresolve::resolution_environment::target,
            pkgresolve::selection_authority_kind::installed_package) == nullptr);
  const auto* selected_dependency = resolution.find(
      pkgsource::package_reference("dep"),
      pkgresolve::resolution_environment::build,
      pkgresolve::selection_authority_kind::installed_package);
  CHECK(selected_dependency != nullptr);
  CHECK(selected_dependency && selected_dependency->installed() != nullptr);
  CHECK(selected_dependency && selected_dependency->candidate() == nullptr);
  CHECK(resolution.find(
            pkgsource::package_reference("dep"),
            pkgresolve::resolution_environment::build,
            pkgresolve::selection_authority_kind::catalog_candidate) == nullptr);

  CHECK(has_build_node(transaction, "tool"));
  CHECK(!has_build_node(transaction, "dep"));

  const auto& node = build_node(transaction);
  CHECK(node.selection() != nullptr);
  CHECK(node.selection() && node.selection()->candidate() != nullptr);
  auto construction = pkgctl::construction_request::make(
      transaction, node.identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1, 0022, 0)));

  const auto build_inputs = construction.build().inputs().for_scope(
      pkgbuild::input_scope::build);
  CHECK(build_inputs.size() == 1U);
  CHECK(build_inputs.front().selection().installed() != nullptr);
  CHECK(build_inputs.front().selection().candidate() == nullptr);
  CHECK(build_inputs.front().selection().installed()->identity() ==
        retained_dependency.identity());

  const auto check_inputs = construction.build().inputs().for_scope(
      pkgbuild::input_scope::check);
  CHECK(check_inputs.size() == 1U);
  CHECK(check_inputs.front().selection().installed() != nullptr);
  CHECK(check_inputs.front().selection().candidate() == nullptr);
  CHECK(check_inputs.front().selection().installed()->identity() ==
        retained_dependency.identity());

  const auto globally_preferred = make_transaction(
      tool, dependency, installed,
      pkgresolve::installed_preference::prefer_catalog);
  CHECK(has_build_node(globally_preferred, "tool"));
  CHECK(has_build_node(globally_preferred, "dep"));
  CHECK(globally_preferred.resolution().resolution().find(
            pkgsource::package_reference("dep"),
            pkgresolve::resolution_environment::build,
            pkgresolve::selection_authority_kind::installed_package) == nullptr);
  return 0;
}
