// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../cli/options.h"
#include "support/test_support.h"

#include <cstdlib>
#include <string>
#include <variant>
#include <vector>

namespace {

#define CHECK(condition) \
  do \
  { \
    if (!(condition)) \
      std::abort(); \
  } while (false)

std::vector<std::string> build_arguments(bool check, bool global_preference)
{
  const auto binding = test_support::binding();
  std::vector<std::string> result{
      "pkgctl", "build", "tool",
      "--canonical-store", "/state",
      "--collection", "core=/collection",
      "--managed-target", binding.managed_target().string(),
      "--state-store", binding.state_store().string(),
      "--root-view", binding.root_view().string(),
      "--state-backend", binding.state_backend().string(),
      "--publication-domain", binding.publication_domain().string(),
      "--build-architecture", "x86_64",
      "--target-architecture", "x86_64",
      "--start", std::string(64U, '8'),
      "--build-parallelism", "1",
      "--build-source-date-epoch", "0",
      "--build-root-view", std::string(64U, '9'),
      "--runtime-root", "/runtime",
      "--package-object-store", "/package-objects",
      "--build-root", "/build",
      "--artifact-root", "/artifacts",
      "--interpreter", "/bin/sh",
      "--build-user-id", "1000",
      "--build-group-id", "1000",
      "--max-steps", "1",
  };
  if (check)
    result.insert(result.begin() + 3, "--check");
  if (global_preference)
    result.insert(result.begin() + 3, "--prefer-catalog");
  return result;
}

std::vector<std::string> run_arguments(bool with_package_object_store)
{
  const auto binding = test_support::binding();
  std::vector<std::string> result{
      "pkgctl", "run",
      "--canonical-store", "/state",
      "--collection", "core=/collection",
      "--managed-target", binding.managed_target().string(),
      "--state-store", binding.state_store().string(),
      "--root-view", binding.root_view().string(),
      "--state-backend", binding.state_backend().string(),
      "--publication-domain", binding.publication_domain().string(),
      "--build-architecture", "x86_64",
      "--target-architecture", "x86_64",
      "--goal", "build=tool",
      "--start", std::string(64U, '7'),
      "--build-parallelism", "1",
      "--build-source-date-epoch", "0",
      "--operation-policy", "strict-exclusive",
      "--build-root-view", std::string(64U, '9'),
      "--lifecycle-root-view", std::string(64U, '6'),
      "--runtime-root", "/runtime",
      "--build-root", "/build",
      "--lifecycle-root", "/lifecycle",
      "--target-root", "/target",
      "--interpreter", "/bin/sh",
      "--build-user-id", "1000",
      "--build-group-id", "1000",
      "--lifecycle-user-id", "0",
      "--lifecycle-group-id", "0",
      "--max-steps", "1",
  };
  if (with_package_object_store)
  {
    result.push_back("--package-object-store");
    result.push_back("/package-objects");
  }
  return result;
}

pkgctl::cli::command parse(std::vector<std::string> arguments)
{
  std::vector<char*> raw;
  raw.reserve(arguments.size());
  for (auto& argument : arguments)
    raw.push_back(argument.data());
  return pkgctl::cli::parse_command(
      static_cast<int>(raw.size()), raw.data());
}

void require_build_policy(bool check)
{
  auto parsed = parse(build_arguments(check, false));
  const auto* command = std::get_if<pkgctl::cli::transaction_run_command>(&parsed);
  CHECK(command != nullptr);
  CHECK(command->frontend == pkgctl::cli::transaction_run_command_frontend::build);
  CHECK(command->transaction.has_value());
  CHECK(command->package_object_store == "/package-objects");
  if (!command->transaction)
    return;

  const auto& resolution = command->transaction->resolution();
  CHECK(resolution.policy().preference() ==
        pkgresolve::installed_preference::retain_compatible);
  CHECK(resolution.goals().size() == (check ? 2U : 1U));
  CHECK(resolution.goals().front().scope().kind() ==
        pkgsource::requirement_scope_kind::build);
  CHECK(resolution.goals().front().subject().kind() ==
        pkgsource::requirement_subject_kind::package);
  CHECK(resolution.goals().front().subject().package().name() == "tool");
  if (check)
  {
    CHECK(resolution.goals().back().scope().kind() ==
          pkgsource::requirement_scope_kind::check);
    CHECK(resolution.goals().back().subject().package().name() == "tool");
  }
}

void allow_run_without_surplus_package_object_store()
{
  auto parsed = parse(run_arguments(false));
  const auto* command =
      std::get_if<pkgctl::cli::transaction_run_command>(&parsed);
  CHECK(command != nullptr);
  CHECK(command->frontend == pkgctl::cli::transaction_run_command_frontend::run);
  CHECK(!command->package_object_store.has_value());

  parsed = parse(run_arguments(true));
  command = std::get_if<pkgctl::cli::transaction_run_command>(&parsed);
  CHECK(command != nullptr);
  CHECK(command->package_object_store == "/package-objects");
}

void require_package_object_store_authority()
{
  auto arguments = build_arguments(false, false);
  for (auto it = arguments.begin(); it != arguments.end(); ++it)
  {
    if (*it != "--package-object-store")
      continue;
    arguments.erase(it, it + 2);
    break;
  }

  bool refused = false;
  try
  {
    (void)parse(std::move(arguments));
  }
  catch (const pkgctl::cli::usage_error& problem)
  {
    refused = std::string(problem.what()) ==
        "build requires explicit --package-object-store authority";
  }
  CHECK(refused);
}

} // namespace

int main()
{
  require_build_policy(false);
  require_build_policy(true);
  allow_run_without_surplus_package_object_store();
  require_package_object_store_authority();

  bool refused = false;
  try
  {
    (void)parse(build_arguments(false, true));
  }
  catch (const pkgctl::cli::usage_error& problem)
  {
    refused = std::string(problem.what()) ==
        "build owns direct-subject catalog authority; global --prefer-catalog is invalid";
  }
  CHECK(refused);
  return 0;
}
