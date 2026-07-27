// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "options.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <libpkgstate/digest.h>

namespace pkgctl::cli {
namespace {

enum class command_kind { catalog, resolve, transaction };

struct raw_collection final {
  std::string name;
  std::string root;
  std::optional<std::string> external_revision;
  std::uint32_t argument_index;
};

struct raw_options final {
  std::vector<raw_collection> collections;
  std::map<std::string, std::string> external_revisions;
  std::uint64_t max_document_bytes = 1024U * 1024U;
  std::optional<std::string> canonical_store;
  std::optional<std::string> managed_target;
  std::optional<std::string> state_store;
  std::optional<std::string> root_view;
  std::optional<std::string> state_backend;
  std::optional<std::string> publication_domain;
  std::optional<std::string> build_architecture;
  std::optional<std::string> target_architecture;
  std::vector<pkgresolve::resolution_goal> goals;
  bool prefer_catalog = false;
  bool converge_exact = false;
};

[[noreturn]] void fail(const std::string& message)
{
  throw usage_error(message);
}

const char* require_value(int argc, char** argv, int& index,
                          const std::string& option)
{
  if (++index >= argc)
    fail(option + " requires a value");
  return argv[index];
}

std::pair<std::string, std::string> assignment(
    std::string value, const std::string& option)
{
  const std::size_t equals = value.find('=');
  if (equals == std::string::npos || equals == 0 || equals + 1 == value.size())
    fail(option + " requires NAME=VALUE");
  return {value.substr(0, equals), value.substr(equals + 1)};
}

std::uint64_t decimal(const std::string& value)
{
  if (value.empty())
    fail("empty document byte limit");
  if (value.size() > 1U && value.front() == '0')
    fail("document byte limit is not canonical decimal");

  std::uint64_t result = 0;
  for (const char character : value)
  {
    if (character < '0' || character > '9')
      fail("document byte limit is not canonical decimal");
    const std::uint64_t digit = static_cast<unsigned>(character - '0');
    if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
      fail("document byte limit is too large");
    result = result * 10U + digit;
  }
  if (result == 0)
    fail("document byte limit must be greater than zero");
  return result;
}

pkgsource::lifecycle_action lifecycle(std::string_view value)
{
  if (value == "pre-install") return pkgsource::lifecycle_action::pre_install;
  if (value == "post-install") return pkgsource::lifecycle_action::post_install;
  if (value == "pre-remove") return pkgsource::lifecycle_action::pre_remove;
  if (value == "post-remove") return pkgsource::lifecycle_action::post_remove;
  fail("unknown lifecycle action: " + std::string(value));
}

pkgsource::requirement_subject subject(std::string value)
{
  if (!value.empty() && value.front() == '@')
    return pkgsource::requirement_subject(
        pkgsource::profile_reference(std::move(value)));
  return pkgsource::requirement_subject(
      pkgsource::package_reference(std::move(value)));
}

pkgresolve::resolution_goal goal(std::string value, int argument_index)
{
  const std::size_t equals = value.find('=');
  if (equals == std::string::npos || equals == 0 || equals + 1 == value.size())
    fail("--goal requires SCOPE=SUBJECT");

  const std::string scope_text = value.substr(0, equals);
  pkgsource::requirement_scope scope = pkgsource::requirement_scope::run();
  if (scope_text == "build")
    scope = pkgsource::requirement_scope::build();
  else if (scope_text == "run")
    scope = pkgsource::requirement_scope::run();
  else if (scope_text == "check")
    scope = pkgsource::requirement_scope::check();
  else if (scope_text.rfind("lifecycle:", 0) == 0)
    scope = pkgsource::requirement_scope::lifecycle(
        lifecycle(scope_text.substr(std::string("lifecycle:").size())));
  else
    fail("unknown goal scope: " + scope_text);

  return pkgresolve::resolution_goal(
      std::move(scope), subject(value.substr(equals + 1)),
      "<command-line>:" + std::to_string(argument_index));
}

command_kind parse_kind(std::string_view value)
{
  if (value == "catalog") return command_kind::catalog;
  if (value == "resolve") return command_kind::resolve;
  if (value == "transaction") return command_kind::transaction;
  fail("unknown command: " + std::string(value));
}

template<typename Optional>
void set_once(Optional& target, std::string value, const std::string& option)
{
  if (target)
    fail(option + " specified more than once");
  target = std::move(value);
}

raw_options parse_raw(command_kind kind, int argc, char** argv)
{
  raw_options parsed;
  for (int index = 2; index < argc; ++index)
  {
    const std::string argument(argv[index]);
    if (argument == "--collection")
    {
      auto [name, root] = assignment(
          require_value(argc, argv, index, argument), argument);
      if (parsed.collections.size() >=
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        fail("too many collections");
      parsed.collections.push_back(
          raw_collection{std::move(name), std::move(root), std::nullopt,
                         static_cast<std::uint32_t>(index)});
      continue;
    }
    if (argument == "--external-revision")
    {
      auto [name, revision] = assignment(
          require_value(argc, argv, index, argument), argument);
      if (!parsed.external_revisions.emplace(std::move(name),
                                              std::move(revision)).second)
        fail("duplicate external revision for one collection");
      continue;
    }
    if (argument == "--max-document-bytes")
    {
      parsed.max_document_bytes = decimal(
          require_value(argc, argv, index, argument));
      continue;
    }
    if (argument == "--canonical-store")
    {
      set_once(parsed.canonical_store,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--managed-target")
    {
      set_once(parsed.managed_target,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--state-store")
    {
      set_once(parsed.state_store,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--root-view")
    {
      set_once(parsed.root_view,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--state-backend")
    {
      set_once(parsed.state_backend,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--publication-domain")
    {
      set_once(parsed.publication_domain,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--build-architecture")
    {
      set_once(parsed.build_architecture,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--target-architecture")
    {
      set_once(parsed.target_architecture,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--goal")
    {
      parsed.goals.push_back(goal(
          require_value(argc, argv, index, argument), index));
      continue;
    }
    if (argument == "--prefer-catalog")
    {
      if (parsed.prefer_catalog)
        fail("--prefer-catalog specified more than once");
      parsed.prefer_catalog = true;
      continue;
    }
    if (argument == "--converge-exact")
    {
      if (parsed.converge_exact)
        fail("--converge-exact specified more than once");
      parsed.converge_exact = true;
      continue;
    }
    fail("unknown option: " + argument);
  }

  if (parsed.collections.empty())
    fail("at least one --collection is required");

  for (raw_collection& collection : parsed.collections)
  {
    const auto revision = parsed.external_revisions.find(collection.name);
    if (revision != parsed.external_revisions.end())
    {
      collection.external_revision = revision->second;
      parsed.external_revisions.erase(revision);
    }
  }
  if (!parsed.external_revisions.empty())
    fail("external revision names an unknown collection: " +
         parsed.external_revisions.begin()->first);

  if (kind == command_kind::catalog)
  {
    if (parsed.canonical_store || parsed.managed_target || parsed.state_store ||
        parsed.root_view || parsed.state_backend || parsed.publication_domain ||
        parsed.build_architecture || parsed.target_architecture ||
        !parsed.goals.empty() || parsed.prefer_catalog || parsed.converge_exact)
    {
      fail("catalog command received resolution or transaction options");
    }
  }
  else
  {
    if (!parsed.canonical_store || !parsed.managed_target ||
        !parsed.state_store || !parsed.root_view || !parsed.state_backend ||
        !parsed.publication_domain)
      fail("all canonical state options are required");
    if (!parsed.build_architecture || !parsed.target_architecture)
      fail("both architecture options are required");
    if (parsed.goals.empty())
      fail("at least one --goal is required");
    if (kind != command_kind::transaction && parsed.converge_exact)
      fail("--converge-exact is valid only for transaction");
  }
  return parsed;
}

catalog_request catalog_from(raw_options& parsed)
{
  std::vector<pkgcatalog::acquire::collection_specification> specifications;
  specifications.reserve(parsed.collections.size());
  for (std::size_t index = 0; index < parsed.collections.size(); ++index)
  {
    raw_collection& collection = parsed.collections[index];
    specifications.emplace_back(
        static_cast<std::uint32_t>(index),
        pkgcatalog::collection_reference(std::move(collection.name)),
        std::move(collection.root),
        std::move(collection.external_revision),
        pkgsource::declaration_provenance(
            "<command-line>",
            "collections[" + std::to_string(index) + "]", 1,
            collection.argument_index + 1U));
  }
  return catalog_request::make(
      std::move(specifications),
      pkgcatalog::acquire::limits(parsed.max_document_bytes));
}

resolution_request resolution_from(raw_options& parsed)
{
  catalog_request catalog = catalog_from(parsed);
  pkgstate::state_target_binding binding = pkgstate::state_target_binding::make(
      pkgstate::managed_target_identity::parse(*parsed.managed_target),
      pkgstate::state_store_identity::parse(*parsed.state_store),
      pkgstate::root_view_identity::parse(*parsed.root_view),
      pkgstate::state_backend_identity::parse(*parsed.state_backend),
      pkgstate::publication_domain_identity::parse(*parsed.publication_domain));
  state_location state = state_location::make(
      std::move(*parsed.canonical_store), std::move(binding));
  pkgresolve::architecture_context architectures(
      pkgsource::architecture_reference(std::move(*parsed.build_architecture)),
      pkgsource::architecture_reference(std::move(*parsed.target_architecture)));
  pkgresolve::resolution_policy policy(
      parsed.prefer_catalog
          ? pkgresolve::installed_preference::prefer_catalog
          : pkgresolve::installed_preference::retain_compatible);
  return resolution_request::make(
      std::move(catalog), std::move(state), std::move(architectures),
      std::move(parsed.goals), std::move(policy));
}

} // namespace

usage_error::usage_error(std::string message)
    : std::invalid_argument(std::move(message))
{
}

command parse_command(int argc, char** argv)
{
  if (argc < 2)
    fail("a command is required");
  const command_kind kind = parse_kind(argv[1]);
  raw_options parsed = parse_raw(kind, argc, argv);
  if (kind == command_kind::catalog)
    return catalog_from(parsed);
  resolution_request resolution = resolution_from(parsed);
  if (kind == command_kind::resolve)
    return resolution;
  return transaction_request::make(
      std::move(resolution),
      parsed.converge_exact
          ? pkgtransaction::convergence_policy::converge_exact()
          : pkgtransaction::convergence_policy::preserve_unselected());
}

std::string help_text()
{
  return R"(usage:
  pkgctl catalog OPTIONS
  pkgctl resolve OPTIONS --goal SCOPE=SUBJECT [--goal ...]
  pkgctl transaction OPTIONS --goal SCOPE=SUBJECT [--goal ...]

Read and compose native package authorities without mutation.

Catalog options:
  --collection NAME=ROOT          explicit collection; order is precedence
  --external-revision NAME=VALUE  diagnostic revision provenance
  --max-document-bytes N          per-document acquisition limit

State options required by resolve and transaction:
  --canonical-store PATH
  --managed-target ID
  --state-store ID
  --root-view ID
  --state-backend ID
  --publication-domain ID

Resolution options:
  --build-architecture NAME
  --target-architecture NAME
  --goal SCOPE=SUBJECT
  --prefer-catalog

SCOPE is build, run, check, or lifecycle:ACTION. SUBJECT is an exact package
name or authoritative @profile. ACTION is pre-install, post-install,
pre-remove, or post-remove.

Transaction options:
  --converge-exact                remove installed packages outside the exact
                                  selected target closure; never the default

Global options:
  -h, --help
  -V, --version

The commands are read-only. pkgctl does not initialize state, build packages,
open artifacts, plan filesystem changes, execute lifecycle programs, apply
mutations, or publish installed state in this release.
)";
}

} // namespace pkgctl::cli
