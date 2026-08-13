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

#include <pkgctl/error.h>

namespace pkgctl::cli {
namespace {

enum class command_kind {
  catalog,
  resolve,
  transaction,
  run,
  build,
  inspect_run,
  inspect_effect,
};

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
  bool max_document_bytes_explicit = false;
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
  std::optional<transaction_run_command_intent> run_intent;
  std::optional<std::string> run_nonce;
  std::optional<std::string> runtime_root;
  std::optional<std::string> build_root_path;
  std::optional<std::string> artifact_root_path;
  std::optional<std::string> lifecycle_root_path;
  std::optional<std::string> target_root_path;
  std::optional<std::string> interpreter;
  std::optional<std::uint64_t> build_user_id;
  std::optional<std::uint64_t> build_group_id;
  std::vector<std::uint64_t> build_supplementary_groups;
  std::optional<std::uint64_t> lifecycle_user_id;
  std::optional<std::uint64_t> lifecycle_group_id;
  std::vector<std::uint64_t> lifecycle_supplementary_groups;
  std::optional<std::uint64_t> source_date_epoch;
  std::optional<std::uint64_t> maximum_steps;
  std::vector<installed_tree_option> installed_trees;
  std::optional<std::string> build_subject;
  std::optional<int> build_subject_argument_index;
  bool build_check = false;
  std::optional<int> build_check_argument_index;
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

std::uint64_t decimal(const std::string& value, const std::string& description,
                      bool allow_zero = false)
{
  if (value.empty())
    fail("empty " + description);
  if (value.size() > 1U && value.front() == '0')
    fail(description + " is not canonical decimal");

  std::uint64_t result = 0;
  for (const char character : value)
  {
    if (character < '0' || character > '9')
      fail(description + " is not canonical decimal");
    const std::uint64_t digit = static_cast<unsigned>(character - '0');
    if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
      fail(description + " is too large");
    result = result * 10U + digit;
  }
  if (!allow_zero && result == 0)
    fail(description + " must be greater than zero");
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
  if (value == "run") return command_kind::run;
  if (value == "build") return command_kind::build;
  if (value == "inspect-run") return command_kind::inspect_run;
  if (value == "inspect-effect") return command_kind::inspect_effect;
  fail("unknown command: " + std::string(value));
}

template<typename Optional>
void set_once(Optional& target, std::string value, const std::string& option)
{
  if (target)
    fail(option + " specified more than once");
  target = std::move(value);
}

run_inspection_command parse_run_inspection(int argc, char** argv)
{
  std::optional<std::string> store;
  std::optional<std::string> journal;
  for (int index = 2; index < argc; ++index)
  {
    const std::string argument(argv[index]);
    if (argument == "--run-store")
    {
      set_once(store, require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--journal")
    {
      set_once(journal, require_value(argc, argv, index, argument), argument);
      continue;
    }
    fail("unknown inspect-run option: " + argument);
  }

  if (!store || store->empty())
    fail("--run-store is required and must not be empty");
  if (!journal)
    fail("--journal is required");

  try
  {
    return run_inspection_command{
        std::move(*store), session_identity::from_hex(std::move(*journal))};
  }
  catch (const pkgctl::error& problem)
  {
    fail(std::string("invalid --journal: ") + problem.what());
  }
}

effect_inspection_command parse_effect_inspection(int argc, char** argv)
{
  std::optional<std::string> store;
  std::optional<std::string> attempt;
  for (int index = 2; index < argc; ++index)
  {
    const std::string argument(argv[index]);
    if (argument == "--effect-store")
    {
      set_once(store, require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--attempt")
    {
      set_once(attempt, require_value(argc, argv, index, argument), argument);
      continue;
    }
    fail("unknown inspect-effect option: " + argument);
  }

  if (!store || store->empty())
    fail("--effect-store is required and must not be empty");
  if (!attempt)
    fail("--attempt is required");

  try
  {
    return effect_inspection_command{
        std::move(*store), session_identity::from_hex(std::move(*attempt))};
  }
  catch (const pkgctl::error& problem)
  {
    fail(std::string("invalid --attempt: ") + problem.what());
  }
}

installed_tree_option installed_tree(std::string value)
{
  const auto [package_text, resource_and_path] = assignment(
      std::move(value), "--installed-tree");
  const std::size_t comma = resource_and_path.find(',');
  if (comma == std::string::npos || comma == 0U ||
      comma + 1U == resource_and_path.size())
    fail("--installed-tree requires PACKAGE_ID=RESOURCE_SHA256,PATH");
  try
  {
    return installed_tree_option{
        pkgstate::installed_package_identity::parse(package_text),
        pkgexec::resource_identity::from_sha256(
            resource_and_path.substr(0U, comma)),
        resource_and_path.substr(comma + 1U)};
  }
  catch (const std::exception& problem)
  {
    fail(std::string("invalid --installed-tree: ") + problem.what());
  }
}

void set_run_intent(raw_options& parsed,
                    transaction_run_command_intent intent,
                    std::string nonce,
                    const std::string& option)
{
  if (parsed.run_intent)
    fail("--start and --resume are mutually exclusive");
  parsed.run_intent = intent;
  set_once(parsed.run_nonce, std::move(nonce), option);
}

raw_options parse_raw(command_kind kind, int argc, char** argv)
{
  raw_options parsed;
  for (int index = 2; index < argc; ++index)
  {
    const std::string argument(argv[index]);
    if (kind == command_kind::build && !argument.empty() &&
        argument.front() != '-')
    {
      if (parsed.build_subject)
        fail("build accepts exactly one package subject");
      parsed.build_subject = argument;
      parsed.build_subject_argument_index = index;
      continue;
    }
    if (argument == "--check")
    {
      if (kind != command_kind::build)
        fail("--check is valid only for build");
      if (parsed.build_check)
        fail("--check specified more than once");
      parsed.build_check = true;
      parsed.build_check_argument_index = index;
      continue;
    }
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
      if (parsed.max_document_bytes_explicit)
        fail("--max-document-bytes specified more than once");
      parsed.max_document_bytes = decimal(
          require_value(argc, argv, index, argument), "document byte limit");
      parsed.max_document_bytes_explicit = true;
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
    if (argument == "--start")
    {
      set_run_intent(parsed, transaction_run_command_intent::start,
                     require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--resume")
    {
      set_run_intent(parsed, transaction_run_command_intent::resume,
                     require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--runtime-root")
    {
      set_once(parsed.runtime_root,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--build-root")
    {
      set_once(parsed.build_root_path,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--artifact-root")
    {
      set_once(parsed.artifact_root_path,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--lifecycle-root")
    {
      set_once(parsed.lifecycle_root_path,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--target-root")
    {
      set_once(parsed.target_root_path,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--interpreter")
    {
      set_once(parsed.interpreter,
               require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--build-user-id")
    {
      if (parsed.build_user_id)
        fail("--build-user-id specified more than once");
      parsed.build_user_id = decimal(
          require_value(argc, argv, index, argument), "build user id", true);
      continue;
    }
    if (argument == "--build-group-id")
    {
      if (parsed.build_group_id)
        fail("--build-group-id specified more than once");
      parsed.build_group_id = decimal(
          require_value(argc, argv, index, argument), "build group id", true);
      continue;
    }
    if (argument == "--build-supplementary-group")
    {
      parsed.build_supplementary_groups.push_back(decimal(
          require_value(argc, argv, index, argument),
          "build supplementary group id", true));
      continue;
    }
    if (argument == "--lifecycle-user-id")
    {
      if (parsed.lifecycle_user_id)
        fail("--lifecycle-user-id specified more than once");
      parsed.lifecycle_user_id = decimal(
          require_value(argc, argv, index, argument), "lifecycle user id", true);
      continue;
    }
    if (argument == "--lifecycle-group-id")
    {
      if (parsed.lifecycle_group_id)
        fail("--lifecycle-group-id specified more than once");
      parsed.lifecycle_group_id = decimal(
          require_value(argc, argv, index, argument), "lifecycle group id", true);
      continue;
    }
    if (argument == "--lifecycle-supplementary-group")
    {
      parsed.lifecycle_supplementary_groups.push_back(decimal(
          require_value(argc, argv, index, argument),
          "lifecycle supplementary group id", true));
      continue;
    }
    if (argument == "--source-date-epoch")
    {
      if (parsed.source_date_epoch)
        fail("--source-date-epoch specified more than once");
      parsed.source_date_epoch = decimal(
          require_value(argc, argv, index, argument), "source date epoch", true);
      continue;
    }
    if (argument == "--max-steps")
    {
      if (parsed.maximum_steps) fail("--max-steps specified more than once");
      parsed.maximum_steps = decimal(
          require_value(argc, argv, index, argument), "maximum step count");
      continue;
    }
    if (argument == "--installed-tree")
    {
      parsed.installed_trees.push_back(installed_tree(
          require_value(argc, argv, index, argument)));
      continue;
    }
    fail("unknown option: " + argument);
  }

  const bool run_like =
      kind == command_kind::run || kind == command_kind::build;
  if (run_like && (!parsed.run_intent || !parsed.run_nonce))
    fail(std::string(kind == command_kind::build ? "build" : "run") +
         " requires exactly one --start or --resume nonce");

  const bool resume = run_like &&
      *parsed.run_intent == transaction_run_command_intent::resume;
  const bool has_start_semantics = !parsed.collections.empty() ||
      !parsed.external_revisions.empty() || parsed.max_document_bytes_explicit ||
      parsed.managed_target || parsed.state_store || parsed.root_view ||
      parsed.state_backend || parsed.publication_domain ||
      parsed.build_architecture || parsed.target_architecture ||
      !parsed.goals.empty() || parsed.prefer_catalog || parsed.converge_exact ||
      parsed.build_subject || parsed.build_check;

  if (resume)
  {
    if (!parsed.canonical_store)
      fail("--resume requires --canonical-store");
    if (has_start_semantics)
    {
      fail("--resume uses retained transaction semantics; catalog, "
           "target-binding, architecture, goal, resolution-policy, build "
           "subject/check, and convergence options are valid only with "
           "--start");
    }
  }
  else
  {
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
          parsed.root_view || parsed.state_backend ||
          parsed.publication_domain || parsed.build_architecture ||
          parsed.target_architecture || !parsed.goals.empty() ||
          parsed.prefer_catalog || parsed.converge_exact)
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

      if (kind == command_kind::build)
      {
        if (!parsed.build_subject)
          fail("build --start requires exactly one package subject");
        if (!parsed.goals.empty())
          fail("build owns its build/check goals; --goal is invalid");
        if (parsed.prefer_catalog)
          fail("build already requires catalog authority; --prefer-catalog is invalid");
        if (parsed.converge_exact)
          fail("--converge-exact is invalid for build");
      }
      else
      {
        if (parsed.goals.empty())
          fail("at least one --goal is required");
        if (kind != command_kind::transaction && kind != command_kind::run &&
            parsed.converge_exact)
          fail("--converge-exact is valid only for transaction or run");
      }
    }
  }

  const bool has_native_execution_options = parsed.run_intent || parsed.run_nonce ||
      parsed.runtime_root || parsed.build_root_path || parsed.artifact_root_path ||
      parsed.lifecycle_root_path || parsed.target_root_path ||
      parsed.interpreter || parsed.build_user_id || parsed.build_group_id ||
      !parsed.build_supplementary_groups.empty() || parsed.lifecycle_user_id ||
      parsed.lifecycle_group_id ||
      !parsed.lifecycle_supplementary_groups.empty() ||
      parsed.source_date_epoch || parsed.maximum_steps ||
      !parsed.installed_trees.empty();
  if (kind == command_kind::run)
  {
    if (parsed.artifact_root_path)
      fail("--artifact-root is valid only for build");
    if (!parsed.runtime_root || !parsed.build_root_path ||
        !parsed.lifecycle_root_path || !parsed.target_root_path ||
        !parsed.interpreter || !parsed.build_user_id.has_value() ||
        !parsed.build_group_id.has_value() ||
        !parsed.lifecycle_user_id.has_value() ||
        !parsed.lifecycle_group_id.has_value() ||
        !parsed.source_date_epoch.has_value() || !parsed.maximum_steps)
      fail("run requires roots, interpreter, build/lifecycle credentials, epoch, and bound");
  }
  else if (kind == command_kind::build)
  {
    if (!parsed.runtime_root || !parsed.build_root_path ||
        !parsed.artifact_root_path || !parsed.interpreter ||
        !parsed.build_user_id.has_value() || !parsed.build_group_id.has_value() ||
        !parsed.source_date_epoch.has_value() || !parsed.maximum_steps)
    {
      fail("build requires runtime/build/artifact roots, interpreter, build "
           "credentials, epoch, and bound");
    }
    if (parsed.lifecycle_root_path || parsed.target_root_path ||
        parsed.lifecycle_user_id || parsed.lifecycle_group_id ||
        !parsed.lifecycle_supplementary_groups.empty())
    {
      fail("target-operation authority options are invalid for build");
    }
  }
  else if (has_native_execution_options)
    fail("native execution authority options are valid only for run or build");

  if (run_like && *parsed.maximum_steps >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    fail("maximum step count is too large for this platform");

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

transaction_run_command transaction_run_from(
    command_kind kind,
    raw_options& parsed,
    std::optional<transaction_request> transaction,
    std::filesystem::path canonical_store)
{
  const auto frontend = kind == command_kind::build
      ? transaction_run_command_frontend::build
      : transaction_run_command_frontend::run;
  std::filesystem::path runtime_root(std::move(*parsed.runtime_root));
  std::filesystem::path artifact_root = frontend == transaction_run_command_frontend::build
      ? std::filesystem::path(std::move(*parsed.artifact_root_path))
      : runtime_root / "artifacts";

  std::optional<pkgexec::credential_policy> lifecycle_credentials;
  if (parsed.lifecycle_user_id && parsed.lifecycle_group_id)
  {
    lifecycle_credentials.emplace(pkgexec::credential_policy::fixed(
        *parsed.lifecycle_user_id, *parsed.lifecycle_group_id,
        std::move(parsed.lifecycle_supplementary_groups), true));
  }

  return transaction_run_command{
      frontend, std::move(transaction), std::move(canonical_store),
      *parsed.run_intent,
      transaction_run_nonce::from_hex(std::move(*parsed.run_nonce)),
      std::move(runtime_root), std::move(*parsed.build_root_path),
      std::move(artifact_root),
      parsed.lifecycle_root_path
          ? std::optional<std::filesystem::path>(
                std::move(*parsed.lifecycle_root_path))
          : std::nullopt,
      parsed.target_root_path
          ? std::optional<std::filesystem::path>(
                std::move(*parsed.target_root_path))
          : std::nullopt,
      std::move(*parsed.interpreter),
      pkgexec::credential_policy::fixed(
          *parsed.build_user_id, *parsed.build_group_id,
          std::move(parsed.build_supplementary_groups), true),
      std::move(lifecycle_credentials), *parsed.source_date_epoch,
      static_cast<std::size_t>(*parsed.maximum_steps),
      std::move(parsed.installed_trees)};
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
  if (kind == command_kind::inspect_run)
    return parse_run_inspection(argc, argv);
  if (kind == command_kind::inspect_effect)
    return parse_effect_inspection(argc, argv);
  raw_options parsed = parse_raw(kind, argc, argv);
  if (kind == command_kind::catalog)
    return catalog_from(parsed);

  const bool run_like =
      kind == command_kind::run || kind == command_kind::build;
  if (run_like &&
      *parsed.run_intent == transaction_run_command_intent::resume)
  {
    try
    {
      return transaction_run_from(
          kind, parsed, std::nullopt,
          std::filesystem::path(*parsed.canonical_store));
    }
    catch (const std::exception& problem)
    {
      fail(std::string(kind == command_kind::build
                           ? "invalid build authority: "
                           : "invalid run authority: ") +
           problem.what());
    }
  }

  if (kind == command_kind::build)
  {
    try
    {
      const auto package = pkgsource::package_reference(*parsed.build_subject);
      parsed.goals.emplace_back(
          pkgsource::requirement_scope::build(),
          pkgsource::requirement_subject(package),
          "<command-line>:" +
              std::to_string(*parsed.build_subject_argument_index));
      if (parsed.build_check)
      {
        parsed.goals.emplace_back(
            pkgsource::requirement_scope::check(),
            pkgsource::requirement_subject(package),
            "<command-line>:" +
                std::to_string(*parsed.build_check_argument_index));
      }
      parsed.prefer_catalog = true;
    }
    catch (const std::exception& problem)
    {
      fail(std::string("invalid build subject: ") + problem.what());
    }
  }

  const std::filesystem::path canonical_store = *parsed.canonical_store;
  resolution_request resolution = resolution_from(parsed);
  if (kind == command_kind::resolve)
    return resolution;
  transaction_request transaction = transaction_request::make(
      std::move(resolution),
      parsed.converge_exact
          ? pkgtransaction::convergence_policy::converge_exact()
          : pkgtransaction::convergence_policy::preserve_unselected());
  if (kind == command_kind::transaction)
    return transaction;
  try
  {
    return transaction_run_from(
        kind, parsed, std::move(transaction), std::move(canonical_store));
  }
  catch (const std::exception& problem)
  {
    fail(std::string(kind == command_kind::build
                         ? "invalid build authority: "
                         : "invalid run authority: ") +
         problem.what());
  }

}

std::string help_text()
{
  return R"(usage:
  pkgctl catalog OPTIONS
  pkgctl resolve OPTIONS --goal SCOPE=SUBJECT [--goal ...]
  pkgctl transaction OPTIONS --goal SCOPE=SUBJECT [--goal ...]
  pkgctl run OPTIONS --goal SCOPE=SUBJECT [--goal ...] --start SHA256 RUN-AUTHORITY
  pkgctl run --canonical-store PATH --resume SHA256 RUN-AUTHORITY
  pkgctl build PACKAGE [--check] OPTIONS --start SHA256 BUILD-AUTHORITY
  pkgctl build --canonical-store PATH --resume SHA256 BUILD-AUTHORITY
  pkgctl inspect-run --run-store PATH --journal SHA256
  pkgctl inspect-effect --effect-store PATH --attempt SHA256

Inspect, compose, or boundedly execute native package authorities.

Catalog options:
  --collection NAME=ROOT          explicit collection; order is precedence
  --external-revision NAME=VALUE  diagnostic revision provenance
  --max-document-bytes N          per-document acquisition limit

State options required by resolve, transaction, run --start, and build --start:
  --canonical-store PATH           existing store path; also required by --resume
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

Build start owns its resolution goals. PACKAGE becomes an exact build goal;
--check adds the exact check goal, and catalog construction authority is
preferred over an already installed package. --goal, --prefer-catalog, and
--converge-exact are therefore invalid for build.

Transaction options:
  --converge-exact                remove installed packages outside the exact
                                  selected target closure; never the default

Run/build intent:
  --start SHA256                  admit a new explicit semantic request
  --resume SHA256                 resume retained semantic request and intent

Catalog acquisition, target-binding, architecture, goal, build subject/check,
resolution-policy, and convergence options are start-only. Resume refuses their
re-declaration and uses command-evidence retained at admission.
--canonical-store remains live resume authority naming the existing physical
canonical state store.

Shared native execution authority:
  --runtime-root PATH             existing private runtime hierarchy
  --build-root PATH               existing construction/check root view
  --interpreter PATH              interpreter coordinate; inspected when executable work remains
  --build-user-id N               construction/check execution user id
  --build-group-id N              construction/check execution group id
  --build-supplementary-group N   repeatable construction/check group id
  --source-date-epoch N           hermetic construction epoch
  --max-steps N                   positive bound for this invocation
  --installed-tree P=R,PATH       retained installed package/resource tree

Build-only authority:
  --artifact-root PATH            existing absolute public artifact hierarchy; retained
                                  as exact command authority and disjoint from
                                  the private runtime hierarchy

Additional run authority for target operations:
  --lifecycle-root PATH           existing lifecycle execution root view
  --target-root PATH              existing managed target root
  --lifecycle-user-id N           lifecycle execution user id
  --lifecycle-group-id N          lifecycle execution group id
  --lifecycle-supplementary-group N
                                  repeatable lifecycle group id

Run inspection options:
  --run-store PATH                existing POSIX transaction-run store
  --journal SHA256                exact lowercase journal identity

Effect inspection options:
  --effect-store PATH             existing POSIX effect-attempt store
  --attempt SHA256                exact lowercase effect-attempt identity

Global options:
  -h, --help
  -V, --version

The catalog, resolve, transaction, and inspection commands are read-only.
The run and build commands execute only through one explicitly retained native
runtime and perform at most --max-steps controller advances. Build admits only
construction/check authority, publishes immutable package archives beneath the
explicit --artifact-root, and never owns target mutation, lifecycle, or state
publication authority. Neither frontend initializes or scans stores, replans a
resumed transaction from live catalog/state, loops without a bound, retries on
a timer, cleans up, rolls back, or discovers a journal.
)";
}

} // namespace pkgctl::cli
