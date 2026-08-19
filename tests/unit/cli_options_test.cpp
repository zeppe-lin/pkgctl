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

} // namespace

int main()
{
  require_build_policy(false);
  require_build_policy(true);

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
