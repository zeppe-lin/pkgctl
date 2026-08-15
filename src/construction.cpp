// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/construction.h>

#include <pkgctl/error.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sys/types.h>

namespace pkgctl {
namespace {

std::filesystem::path normalize_absolute(
    const std::filesystem::path& value, std::string_view field,
    bool allow_root = false)
{
  if (value.empty() || !value.is_absolute())
    throw error(error_code::invalid_construction_session,
                std::string(field) + " must be an absolute path");
  const auto normalized = value.lexically_normal();
  if (!allow_root && normalized == std::filesystem::path("/"))
    throw error(error_code::invalid_construction_session,
                std::string(field) + " cannot be the filesystem root");
  return normalized;
}

bool contains_or_equals(const std::filesystem::path& parent,
                        const std::filesystem::path& child)
{
  auto left = parent.begin();
  auto right = child.begin();
  for (; left != parent.end(); ++left, ++right)
    if (right == child.end() || *left != *right)
      return false;
  return true;
}

bool overlaps(const std::filesystem::path& first,
              const std::filesystem::path& second)
{
  return contains_or_equals(first, second) ||
         contains_or_equals(second, first);
}

void require_disjoint(const std::filesystem::path& first,
                      std::string_view first_name,
                      const std::filesystem::path& second,
                      std::string_view second_name)
{
  if (overlaps(first, second))
    throw error(error_code::invalid_construction_session,
                std::string(first_name) + " overlaps " +
                std::string(second_name));
}

void normalize_session_coordinates(
    construction_paths& paths,
    std::vector<pkgbuild_exec::package_input_resource>& package_inputs,
    pkgbuild_exec::execution_identity& identity,
    pkgbuild::artifact_compression compression,
    const construction_request& request)
{
  paths.local_source_root =
      normalize_absolute(paths.local_source_root, "local source root");
  paths.content_store_root =
      normalize_absolute(paths.content_store_root, "content store root");
  paths.build.root_view_path =
      normalize_absolute(paths.build.root_view_path, "root view");
  paths.build.session_root =
      normalize_absolute(paths.build.session_root, "session root");
  paths.build.package_output_root =
      normalize_absolute(paths.build.package_output_root, "package output root");
  paths.build.artifact_path =
      normalize_absolute(paths.build.artifact_path, "artifact path");
  if (paths.build.artifact_path.filename().empty())
    throw error(error_code::invalid_construction_session,
                "artifact path must name a destination");
  if (compression != pkgbuild::artifact_compression::none)
    throw error(error_code::invalid_construction_session,
                "construction admits only uncompressed package_tar");

  // Validate the source/store coordinate contract before any acquisition.
  (void)pkgfetch::materialization_request::seal(
      request.source(), paths.local_source_root, paths.content_store_root,
      request.acquisition_policy());

  require_disjoint(paths.build.root_view_path, "root view",
                   paths.build.session_root, "session root");
  require_disjoint(paths.build.root_view_path, "root view",
                   paths.build.package_output_root, "package output root");
  require_disjoint(paths.build.session_root, "session root",
                   paths.build.package_output_root, "package output root");
  require_disjoint(paths.local_source_root, "local source root",
                   paths.content_store_root, "content store root");
  for (const auto& root : {
           std::pair<std::filesystem::path, std::string_view>{
               paths.build.root_view_path, "root view"},
           {paths.build.session_root, "session root"},
           {paths.build.package_output_root, "package output root"}})
  {
    require_disjoint(paths.local_source_root, "local source root",
                     root.first, root.second);
    require_disjoint(paths.content_store_root, "content store root",
                     root.first, root.second);
  }
  for (const auto& root : {
           std::pair<std::filesystem::path, std::string_view>{
               paths.build.root_view_path, "root view"},
           {paths.build.session_root, "session root"},
           {paths.build.package_output_root, "package output root"},
           {paths.local_source_root, "local source root"},
           {paths.content_store_root, "content store root"}})
    require_disjoint(paths.build.artifact_path, "artifact path",
                     root.first, root.second);

  for (std::size_t index = 0; index < package_inputs.size(); ++index)
  {
    package_inputs[index].path = normalize_absolute(
        package_inputs[index].path, "package input resource");
    const auto& path = package_inputs[index].path;
    require_disjoint(path, "package input resource", paths.build.root_view_path,
                     "root view");
    require_disjoint(path, "package input resource", paths.build.session_root,
                     "session root");
    require_disjoint(path, "package input resource",
                     paths.build.package_output_root, "package output root");
    require_disjoint(path, "package input resource", paths.build.artifact_path,
                     "artifact path");
    require_disjoint(path, "package input resource", paths.local_source_root,
                     "local source root");
    require_disjoint(path, "package input resource", paths.content_store_root,
                     "content store root");
    for (std::size_t previous = 0; previous < index; ++previous)
      require_disjoint(path, "package input resource",
                       package_inputs[previous].path, "package input resource");
  }

  if (identity.user_id >=
          static_cast<std::uint64_t>(std::numeric_limits<uid_t>::max()) ||
      identity.group_id >=
          static_cast<std::uint64_t>(std::numeric_limits<gid_t>::max()) ||
      std::any_of(identity.supplementary_groups.begin(),
                  identity.supplementary_groups.end(),
                  [](std::uint64_t value) {
                    return value >= static_cast<std::uint64_t>(
                                        std::numeric_limits<gid_t>::max());
                  }))
    throw error(error_code::invalid_construction_session,
                "construction credentials exceed native identifier bounds");
  std::sort(identity.supplementary_groups.begin(),
            identity.supplementary_groups.end());
  if (std::adjacent_find(identity.supplementary_groups.begin(),
                         identity.supplementary_groups.end()) !=
      identity.supplementary_groups.end())
    throw error(error_code::invalid_construction_session,
                "supplementary construction groups must be unique");
  if (std::binary_search(identity.supplementary_groups.begin(),
                         identity.supplementary_groups.end(),
                         identity.group_id))
    throw error(error_code::invalid_construction_session,
                "primary construction group cannot be supplementary");
}

const pkgtransaction::transaction_node& require_build_node(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node_identity& identity)
{
  const auto* node = transaction.program().find(identity);
  if (node == nullptr ||
      node->action() != pkgtransaction::transaction_action_kind::build ||
      node->selection() == nullptr || node->selection()->candidate() == nullptr)
  {
    throw error(error_code::invalid_construction_request,
                "construction request does not select a catalog build node");
  }
  return *node;
}


void validate_input_resources(
    const std::vector<pkgbuild::build_input>& expected,
    const std::vector<pkgbuild_exec::package_input_resource>& supplied)
{
  if (expected.size() != supplied.size())
    throw error(error_code::invalid_construction_session,
                "construction package-input resource cardinality differs from request");

  std::vector<bool> consumed(supplied.size(), false);
  for (const auto& input : expected)
  {
    std::optional<std::size_t> match;
    for (std::size_t index = 0; index < supplied.size(); ++index)
    {
      if (supplied[index].input != input.identity())
        continue;
      if (match)
        throw error(error_code::invalid_construction_session,
                    "construction package-input resource is supplied more than once");
      match = index;
    }
    if (!match)
      throw error(error_code::invalid_construction_session,
                  "construction package input lacks its call-scoped resource");
    consumed[*match] = true;
  }
  if (std::find(consumed.begin(), consumed.end(), false) != consumed.end())
    throw error(error_code::invalid_construction_session,
                "construction session contains an undeclared package-input resource");
}

std::vector<std::string> request_identity_fields(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node_identity& build_node,
    const pkgbuild::build_request& build,
    const pkgfetch::acquisition_policy& acquisition)
{
  return {
      transaction.identity().hex(),
      build_node.hex(),
      build.identity().hex(),
      std::to_string(acquisition.max_object_bytes()),
      std::to_string(acquisition.max_redirects()),
      acquisition.allow_http() ? "1" : "0",
      acquisition.allow_https() ? "1" : "0",
  };
}

std::vector<std::string> session_identity_fields(
    const construction_request& request,
    const construction_paths& paths,
    const std::vector<pkgbuild_exec::package_input_resource>& package_inputs,
    const pkgbuild_exec::execution_identity& identity,
    pkgbuild::artifact_compression compression)
{
  std::vector<std::string> fields{
      request.identity().hex(),
      paths.local_source_root.string(),
      paths.content_store_root.string(),
      paths.build.root_view.hex(),
      paths.build.root_view_path.string(),
      paths.build.session_root.string(),
      paths.build.package_output_root.string(),
      paths.build.artifact_path.string(),
      identity.interpreter.hex(),
      std::to_string(identity.user_id),
      std::to_string(identity.group_id),
      std::to_string(identity.supplementary_groups.size()),
  };
  for (const auto group : identity.supplementary_groups)
    fields.push_back(std::to_string(group));
  fields.push_back(std::to_string(package_inputs.size()));
  for (const auto& input : package_inputs)
  {
    fields.push_back(input.input.hex());
    fields.push_back(input.resource.hex());
    fields.push_back(input.path.string());
  }
  fields.push_back(std::string(pkgbuild::to_string(compression)));
  return fields;
}

void validate_materialization(
    const construction_request& request,
    const pkgfetch::source_materialization& materialization)
{
  if (materialization.source().identity() != request.source().identity())
    throw error(error_code::construction_driver_contract_violation,
                "construction driver returned materialization for another source");
}

void validate_build_result(
    const pkgbuild::build_request& request,
    const pkgbuild_exec::build_execution_result& result)
{
  if (result.build().request().identity() != request.identity())
    throw error(error_code::construction_driver_contract_violation,
                "construction driver returned a build result for another request");

  const bool succeeded =
      result.build().outcome() == pkgbuild::build_outcome::succeeded;
  if (succeeded &&
      (result.sealing_failure().has_value() ||
       !result.image_authority().has_value()))
  {
    throw error(error_code::construction_driver_contract_violation,
                "successful construction lacks complete artifact evidence");
  }
  if (!succeeded && result.image_authority().has_value())
    throw error(error_code::construction_driver_contract_violation,
                "failed construction carries successful artifact evidence");
}

} // namespace

construction_request::construction_request(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity build_node,
    pkgbuild::build_request build,
    pkgfetch::acquisition_policy acquisition_policy,
    session_identity identity)
    : transaction_(std::move(transaction)), build_node_(std::move(build_node)),
      build_(std::move(build)),
      acquisition_policy_(std::move(acquisition_policy)),
      identity_(std::move(identity))
{
}

construction_request construction_request::make(
    transaction_session transaction,
    pkgtransaction::transaction_node_identity build_node,
    pkgbuild::build_policy build_policy,
    pkgfetch::acquisition_policy acquisition_policy)
{
  const auto& node = require_build_node(transaction, build_node);
  const auto& selection = *node.selection();
  const auto& candidate = *selection.candidate();
  if (candidate.source().identity() != selection.source_snapshot() ||
      candidate.release().identity() != selection.release().identity() ||
      candidate.package() != node.package())
  {
    throw error(error_code::invalid_construction_request,
                "transaction build node contradicts its catalog authority");
  }

  auto build = pkgbuild::build_request::seal(
      transaction.resolution().resolution(), selection.identity(),
      std::move(build_policy));
  auto identity = make_session_identity(
      "pkgctl/construction-request/1",
      request_identity_fields(transaction, build_node, build,
                              acquisition_policy));
  return construction_request(
      std::move(transaction), std::move(build_node), std::move(build),
      std::move(acquisition_policy), std::move(identity));
}

const transaction_session& construction_request::transaction() const noexcept
{ return transaction_; }
const pkgtransaction::transaction_node_identity&
construction_request::build_node() const noexcept { return build_node_; }
const pkgbuild::build_request& construction_request::build() const noexcept
{ return build_; }
const pkgsource::source_snapshot& construction_request::source() const noexcept
{ return build_.source(); }
const std::vector<pkgbuild::build_input>&
construction_request::inputs() const noexcept { return build_.inputs().inputs(); }
const pkgbuild::architecture_binding&
construction_request::architectures() const noexcept
{ return build_.architectures(); }
const pkgbuild::build_policy& construction_request::build_policy() const noexcept
{ return build_.policy(); }
const pkgfetch::acquisition_policy&
construction_request::acquisition_policy() const noexcept
{ return acquisition_policy_; }
const session_identity& construction_request::identity() const noexcept
{ return identity_; }

construction_session::construction_session(
    construction_request request,
    construction_paths paths,
    std::vector<pkgbuild_exec::package_input_resource> package_inputs,
    pkgbuild_exec::execution_identity execution_identity,
    pkgbuild::artifact_compression compression,
    session_identity identity)
    : request_(std::move(request)), paths_(std::move(paths)),
      package_inputs_(std::move(package_inputs)),
      execution_identity_(std::move(execution_identity)),
      compression_(compression), identity_(std::move(identity))
{
}

construction_session construction_session::admit(
    construction_request request,
    construction_paths paths,
    std::vector<pkgbuild_exec::package_input_resource> package_inputs,
    pkgbuild_exec::execution_identity execution_identity,
    pkgbuild::artifact_compression compression)
{
  validate_input_resources(request.inputs(), package_inputs);
  normalize_session_coordinates(paths, package_inputs,
                                execution_identity, compression, request);
  auto identity = make_session_identity(
      "pkgctl/construction-session/1",
      session_identity_fields(request, paths, package_inputs,
                              execution_identity, compression));
  return construction_session(
      std::move(request), std::move(paths), std::move(package_inputs),
      std::move(execution_identity), compression, std::move(identity));
}

const construction_request& construction_session::request() const noexcept
{ return request_; }
const construction_paths& construction_session::paths() const noexcept
{ return paths_; }
const std::vector<pkgbuild_exec::package_input_resource>&
construction_session::package_inputs() const noexcept
{ return package_inputs_; }
const pkgbuild_exec::execution_identity&
construction_session::execution_identity() const noexcept
{ return execution_identity_; }
pkgbuild::artifact_compression construction_session::compression() const noexcept
{ return compression_; }
const session_identity& construction_session::identity() const noexcept
{ return identity_; }

native_construction_driver::native_construction_driver(
    pkgexec::execution_backend& backend)
    : backend_(backend)
{
}

pkgfetch::source_materialization
native_construction_driver::materialize_source(
    const pkgfetch::materialization_request& request)
{
  return pkgfetch::materialize(request);
}

pkgbuild_exec::build_execution_result
native_construction_driver::execute_build(
    const pkgbuild_exec::admitted_build_session& session)
{
  return pkgbuild_exec::execute_sealed(session, backend_);
}

void native_construction_driver::publish_build(
    const pkgbuild_exec::admitted_build_session& session,
    const pkgbuild_exec::build_execution_result& result)
{
  pkgbuild_exec::publish_sealed_artifact(session, result);
}

construction_result::construction_result(
    construction_session session,
    pkgfetch::source_materialization materialization,
    pkgbuild_exec::build_execution_result build,
    construction_outcome outcome,
    session_identity identity)
    : session_(std::move(session)),
      materialization_(std::move(materialization)), build_(std::move(build)),
      outcome_(outcome), identity_(std::move(identity))
{
}

construction_outcome construction_result::outcome() const noexcept
{ return outcome_; }
bool construction_result::succeeded() const noexcept
{ return outcome_ == construction_outcome::completed; }
const construction_session& construction_result::session() const noexcept
{ return session_; }
const pkgfetch::source_materialization&
construction_result::materialization() const noexcept { return materialization_; }
const pkgbuild_exec::build_execution_result&
construction_result::build() const noexcept { return build_; }
const session_identity& construction_result::identity() const noexcept
{ return identity_; }

namespace {

pkgbuild_exec::admitted_build_session admit_build_session(
    const construction_session& session,
    const pkgfetch::source_materialization& materialization)
{
  return pkgbuild_exec::admitted_build_session::admit(
      session.request().build(), materialization, session.package_inputs(),
      session.paths().build, session.execution_identity(),
      session.compression());
}

} // namespace

construction_result execute_construction_unpublished(
    construction_session session,
    construction_driver& driver)
{
  auto materialization_request = pkgfetch::materialization_request::seal(
      session.request().source(), session.paths().local_source_root,
      session.paths().content_store_root,
      session.request().acquisition_policy());
  auto materialization = driver.materialize_source(materialization_request);
  validate_materialization(session.request(), materialization);

  const auto& build_request = session.request().build();
  auto admitted = admit_build_session(session, materialization);
  auto build = driver.execute_build(admitted);
  validate_build_result(build_request, build);

  const auto outcome =
      build.build().outcome() == pkgbuild::build_outcome::succeeded
          ? construction_outcome::completed
          : construction_outcome::build_failed;
  auto identity = make_session_identity(
      "pkgctl/construction-result/1",
      {session.identity().hex(), materialization.identity().hex(),
       build.execution().identity().hex(), build.build().identity().hex(),
       std::to_string(static_cast<unsigned int>(outcome))});
  return construction_result(
      std::move(session), std::move(materialization), std::move(build),
      outcome, std::move(identity));
}

void publish_construction(
    const construction_result& result,
    construction_driver& driver)
{
  if (!result.succeeded()) {
    return;
  }
  auto admitted = admit_build_session(result.session(), result.materialization());
  driver.publish_build(admitted, result.build());
}

construction_result execute_construction(
    construction_session session,
    construction_driver& driver)
{
  auto result = execute_construction_unpublished(std::move(session), driver);
  publish_construction(result, driver);
  return result;
}

} // namespace pkgctl
