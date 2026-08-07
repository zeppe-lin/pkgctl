// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_locator.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pkgctl/error.h>

namespace pkgctl {
namespace fs = std::filesystem;
namespace {

[[noreturn]] void locator_failure(
    native_session_locator_error_code code,
    std::string message)
{
  throw native_session_locator_error(code, std::move(message));
}

fs::path normalize_absolute_path(
    fs::path path,
    std::string_view description,
    native_session_locator_error_code code =
        native_session_locator_error_code::invalid_configuration,
    bool allow_filesystem_root = false)
{
  if (path.empty() || !path.is_absolute())
    locator_failure(code, std::string(description) + " must be absolute");
  path = path.lexically_normal();
  if (!allow_filesystem_root && path == path.root_path())
    locator_failure(
        code, std::string(description) + " must not be the filesystem root");
  return path;
}

bool path_prefix(const fs::path& prefix, const fs::path& value)
{
  auto left = prefix.begin();
  auto right = value.begin();
  for (; left != prefix.end(); ++left, ++right)
    if (right == value.end() || *left != *right)
      return false;
  return true;
}

bool paths_overlap(const fs::path& first, const fs::path& second)
{
  return path_prefix(first, second) || path_prefix(second, first);
}

void require_disjoint_configuration_roots(
    const native_transaction_session_roots& roots)
{
  const std::array<std::pair<const fs::path*, const char*>, 6> values{{
      {&roots.content_store_root, "content store root"},
      {&roots.construction_session_root, "construction session root"},
      {&roots.package_output_root, "package output root"},
      {&roots.artifact_root, "artifact root"},
      {&roots.check_temporary_root, "check temporary root"},
      {&roots.root_view_path, "root view"},
  }};
  for (std::size_t first = 0; first < values.size(); ++first)
    for (std::size_t second = first + 1; second < values.size(); ++second)
      if (paths_overlap(*values[first].first, *values[second].first))
        locator_failure(
            native_session_locator_error_code::invalid_configuration,
            std::string(values[first].second) + " overlaps " +
                values[second].second);
}

fs::path dispatch_scope(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch)
{
  return fs::path(record.journal().hex()) / dispatch.identity().hex();
}

const pkgcatalog::acquire::collection_specification&
require_collection_specification(
    const transaction_session& transaction,
    const pkgcatalog::catalog_candidate& candidate)
{
  const auto& specifications =
      transaction.request().resolution().catalog().collections();
  const auto found = std::find_if(
      specifications.begin(), specifications.end(),
      [&](const auto& specification) {
        return specification.name() == candidate.collection();
      });
  if (found == specifications.end())
    locator_failure(
        native_session_locator_error_code::collection_authority_missing,
        "selected catalog candidate has no acquisition specification");
  return *found;
}

fs::path catalog_source_root(
    const construction_request& request)
{
  const auto* candidate = request.build().subject().candidate();
  if (candidate == nullptr)
    locator_failure(
        native_session_locator_error_code::source_coordinate_mismatch,
        "construction subject is not a catalog candidate");

  const auto& specification =
      require_collection_specification(request.transaction(), *candidate);
  const fs::path collection_root = normalize_absolute_path(
      specification.root(), "catalog collection root",
      native_session_locator_error_code::source_coordinate_mismatch, true);

  fs::path document(candidate->source().origin().document());
  if (document.empty() || !document.is_absolute())
    locator_failure(
        native_session_locator_error_code::source_coordinate_mismatch,
        "catalog source document is not an absolute native coordinate");
  document = document.lexically_normal();
  if (document.filename() != "recipe.yml" ||
      document.parent_path().empty() ||
      document.parent_path().parent_path() != collection_root)
    locator_failure(
        native_session_locator_error_code::source_coordinate_mismatch,
        "catalog source document is outside its exact native collection root");
  return document.parent_path();
}

const construction_result& require_predecessor_construction(
    const transaction_progress& progress,
    const pkgresolve::selected_package& selection)
{
  const construction_result* result = nullptr;
  for (const auto& construction : progress.constructions()) {
    if (construction.session().request().build().subject().identity() !=
        selection.identity())
      continue;
    if (result != nullptr)
      locator_failure(
          native_session_locator_error_code::
              predecessor_construction_ambiguous,
          "more than one construction realizes one selected package input");
    result = &construction;
  }
  if (result == nullptr || !result->succeeded())
    locator_failure(
        native_session_locator_error_code::predecessor_construction_missing,
        "catalog package input lacks a successful predecessor construction");
  return *result;
}

pkgexec::resource_identity construction_output_resource(
    const construction_result& construction)
{
  const auto slot = pkgexec::resource_slot::singleton(
      pkgexec::resource_role::package_output_root);
  return construction.build().execution().request().resources()
      .binding(slot).resource();
}

pkgexec::resource_identity construction_source_resource(
    const construction_result& construction)
{
  const auto slot = pkgexec::resource_slot::named(
      pkgexec::resource_role::source_tree, "sources");
  return construction.build().execution().request().resources()
      .binding(slot).resource();
}

fs::path normalize_retained_resource_path(fs::path path)
{
  if (path.empty() || !path.is_absolute())
    locator_failure(
        native_session_locator_error_code::invalid_resource_path,
        "retained installed package resource path must be absolute");
  path = path.lexically_normal();
  if (path == path.root_path())
    locator_failure(
        native_session_locator_error_code::invalid_resource_path,
        "retained installed package resource path must not be root");
  return path;
}

pkgbuild_exec::package_input_resource construction_input_resource(
    const pkgbuild::build_input& input,
    const transaction_progress& progress,
    retained_installed_package_tree_source& installed_packages)
{
  const auto& selection = input.selection();
  if (selection.candidate() != nullptr) {
    const auto& construction =
        require_predecessor_construction(progress, selection);
    return {
        input.identity(), construction_output_resource(construction),
        construction.session().paths().build.package_output_root,
    };
  }

  const auto* installed = selection.installed();
  if (installed == nullptr)
    locator_failure(
        native_session_locator_error_code::installed_resource_mismatch,
        "selected package input has no catalog or installed authority");

  auto retained = installed_packages.locate(*installed);
  if (retained.package != installed->identity())
    locator_failure(
        native_session_locator_error_code::installed_resource_mismatch,
        "retained installed package resource names another package");
  retained.path = normalize_retained_resource_path(std::move(retained.path));
  return {input.identity(), std::move(retained.resource),
          std::move(retained.path)};
}

std::vector<pkgbuild_exec::package_input_resource>
construction_input_resources(
    const construction_request& request,
    const transaction_progress& progress,
    retained_installed_package_tree_source& installed_packages)
{
  std::vector<pkgbuild_exec::package_input_resource> result;
  result.reserve(request.inputs().size());
  for (const auto& input : request.inputs())
    result.push_back(construction_input_resource(
        input, progress, installed_packages));
  return result;
}

pkgbuild_exec::admitted_build_session rebuild_admitted_build_session(
    const construction_result& construction)
{
  return pkgbuild_exec::admitted_build_session::admit(
      construction.session().request().build(),
      construction.materialization(),
      construction.session().package_inputs(),
      construction.session().paths().build,
      construction.session().execution_identity(),
      construction.session().compression());
}

const pkgbuild_exec::package_input_resource&
require_construction_input_resource(
    const construction_result& construction,
    const pkgbuild::build_input_identity& input)
{
  const pkgbuild_exec::package_input_resource* found = nullptr;
  for (const auto& resource : construction.session().package_inputs()) {
    if (resource.input != input)
      continue;
    if (found != nullptr)
      locator_failure(
          native_session_locator_error_code::check_session_invalid,
          "retained construction contains duplicate package-input resources");
    found = &resource;
  }
  if (found == nullptr)
    locator_failure(
        native_session_locator_error_code::check_session_invalid,
        "retained construction lacks a check input resource");
  return *found;
}

std::vector<pkgcheck_exec::package_input_resource> check_input_resources(
    const transaction_check_request& request)
{
  std::vector<pkgcheck_exec::package_input_resource> result;
  const auto& construction = request.construction();
  result.reserve(request.check().inputs().inputs().size());
  for (const auto& input : request.check().inputs().inputs()) {
    const auto& retained =
        require_construction_input_resource(construction, input.identity());
    result.push_back({input.identity(), retained.resource, retained.path});
  }
  return result;
}

} // namespace

native_transaction_session_configuration::
native_transaction_session_configuration(
    native_transaction_session_roots roots,
    native_transaction_session_policy policy)
    : roots_(std::move(roots)), policy_(std::move(policy))
{
}

native_transaction_session_configuration
native_transaction_session_configuration::make(
    native_transaction_session_roots roots,
    native_transaction_session_policy policy)
{
  roots.content_store_root = normalize_absolute_path(
      std::move(roots.content_store_root), "content store root");
  roots.construction_session_root = normalize_absolute_path(
      std::move(roots.construction_session_root),
      "construction session root");
  roots.package_output_root = normalize_absolute_path(
      std::move(roots.package_output_root), "package output root");
  roots.artifact_root = normalize_absolute_path(
      std::move(roots.artifact_root), "artifact root");
  roots.check_temporary_root = normalize_absolute_path(
      std::move(roots.check_temporary_root), "check temporary root");
  roots.root_view_path = normalize_absolute_path(
      std::move(roots.root_view_path), "root view");
  require_disjoint_configuration_roots(roots);
  if (policy.compression != pkgbuild::artifact_compression::none)
    locator_failure(
        native_session_locator_error_code::invalid_configuration,
        "native construction supports only uncompressed package_tar");
  return native_transaction_session_configuration(
      std::move(roots), std::move(policy));
}

const native_transaction_session_roots&
native_transaction_session_configuration::roots() const noexcept
{
  return roots_;
}

const native_transaction_session_policy&
native_transaction_session_configuration::policy() const noexcept
{
  return policy_;
}

native_session_locator_error::native_session_locator_error(
    native_session_locator_error_code code,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code)
{
}

native_session_locator_error_code
native_session_locator_error::code() const noexcept
{
  return code_;
}

native_transaction_dispatch_session_source::
native_transaction_dispatch_session_source(
    native_transaction_session_configuration configuration,
    retained_installed_package_tree_source& installed_packages)
    : configuration_(std::move(configuration)),
      installed_packages_(installed_packages)
{
}

construction_session
native_transaction_dispatch_session_source::construction(
    const transaction_run_journal_record& record,
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  if (dispatch.unit().kind() != transaction_unit_kind::construction)
    locator_failure(
        native_session_locator_error_code::unsupported_dispatch,
        "native construction locator received another dispatch kind");

  try {
    const auto& roots = configuration_.roots();
    const auto& policy = configuration_.policy();
    auto request = construction_request::make(
        run.progress().transaction(), dispatch.unit().primary_node(),
        policy.build, policy.acquisition);
    const auto scope = dispatch_scope(record, dispatch);
    construction_paths paths{
        catalog_source_root(request),
        roots.content_store_root,
        {
            roots.root_view,
            roots.root_view_path,
            roots.construction_session_root / scope,
            roots.package_output_root / scope,
            roots.artifact_root / scope.parent_path() /
                (scope.filename().string() + ".tar"),
        },
    };
    auto inputs = construction_input_resources(
        request, run.progress(), installed_packages_);
    return construction_session::admit(
        std::move(request), std::move(paths), std::move(inputs),
        policy.construction_execution, policy.compression);
  } catch (const native_session_locator_error&) {
    throw;
  } catch (const std::exception& problem) {
    locator_failure(
        native_session_locator_error_code::construction_session_invalid,
        std::string("native construction session admission failed: ") +
            problem.what());
  }
}

transaction_check_session
native_transaction_dispatch_session_source::check(
    const transaction_run_journal_record& record,
    const transaction_run& run,
    const transaction_dispatch& dispatch)
{
  if (dispatch.unit().kind() != transaction_unit_kind::check)
    locator_failure(
        native_session_locator_error_code::unsupported_dispatch,
        "native check locator received another dispatch kind");

  try {
    const auto& roots = configuration_.roots();
    const auto& policy = configuration_.policy();
    auto request = transaction_check_request::make(
        run.progress(), dispatch.unit().primary_node());
    const auto& construction = request.construction();
    const auto admitted = rebuild_admitted_build_session(construction);
    const auto prepared_paths =
        pkgbuild_exec::project_prepared_paths(admitted);
    const auto& artifact = construction.build().build().artifact();
    if (!artifact)
      locator_failure(
          native_session_locator_error_code::check_session_invalid,
          "successful predecessor construction lacks an artifact");

    transaction_check_resources resources{
        {
            construction.session().request().source().identity(),
            construction_source_resource(construction),
            prepared_paths.source_tree,
        },
        {
            artifact->identity(),
            construction_output_resource(construction),
            construction.session().paths().build.package_output_root,
        },
        check_input_resources(request),
        {
            roots.root_view,
            roots.root_view_path,
            roots.check_temporary_root / dispatch_scope(record, dispatch),
        },
        policy.check_execution,
        policy.check_limits,
    };
    return transaction_check_session::admit(
        std::move(request), std::move(resources));
  } catch (const native_session_locator_error&) {
    throw;
  } catch (const std::exception& problem) {
    locator_failure(
        native_session_locator_error_code::check_session_invalid,
        std::string("native check session admission failed: ") +
            problem.what());
  }
}

} // namespace pkgctl
