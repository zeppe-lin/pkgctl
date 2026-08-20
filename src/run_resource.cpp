// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_resource.h>

#include <libpkgimage-exec/libpkgimage-exec.h>
#include <libpkgobject/libpkgobject.h>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <pkgctl/identity.h>

namespace pkgctl {
namespace fs = std::filesystem;
namespace {

class prepared_installed_package_source final
    : public retained_installed_package_tree_source {
public:
  void retain(retained_installed_package_tree tree)
  {
    const auto key = tree.package.string();
    if (!entries_.emplace(key, std::move(tree)).second)
      throw std::runtime_error(
          "installed package resource prepared more than once: " + key);
  }

  [[nodiscard]] retained_installed_package_tree locate(
      const pkgstate::installed_package& package) override
  {
    const auto found = entries_.find(package.identity().string());
    if (found == entries_.end())
      throw std::runtime_error(
          "installed package resource was not prepared: " +
          package.identity().string());
    return found->second;
  }

private:
  std::map<std::string, retained_installed_package_tree> entries_;
};

fs::path dispatch_scope(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch)
{
  return fs::path(record.journal().hex()) / dispatch.identity().hex();
}

pkgexec::resource_identity installed_resource_identity(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch,
    const pkgstate::installed_package& package)
{
  const auto& build = package.control().build();
  return pkgexec::resource_identity::from_sha256(
      make_session_identity(
          "pkgctl/native-installed-package-resource/1",
          {record.journal().hex(), dispatch.identity().hex(),
           package.identity().string(), build.artifact_content().string(),
           build.artifact_image().string()})
          .hex());
}

std::vector<const pkgstate::installed_package*> unique_installed_inputs(
    const std::vector<pkgbuild::build_input>& inputs)
{
  std::vector<const pkgstate::installed_package*> result;
  for (const auto& input : inputs)
  {
    const auto* installed = input.selection().installed();
    if (installed == nullptr)
      continue;
    const auto found = std::find_if(
        result.begin(), result.end(), [&](const auto* retained) {
          return retained->identity() == installed->identity();
        });
    if (found == result.end())
      result.push_back(installed);
  }
  return result;
}

pkgobject::store& require_package_object_authority(
    pkgobject::store* package_objects)
{
  if (package_objects == nullptr)
    throw std::runtime_error(
        "native package-object authority is unavailable for installed package resource preparation");
  return *package_objects;
}

void require_construction_package_object_authority(
    pkgobject::store* package_objects)
{
  if (package_objects == nullptr)
    throw std::runtime_error(
        "native package-object authority is unavailable for fresh construction");
}

prepared_installed_package_source prepare_installed_resources(
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch,
    const native_transaction_session_configuration& configuration,
    pkgobject::store* package_objects,
    const std::vector<pkgbuild::build_input>& inputs)
{
  prepared_installed_package_source prepared;
  const auto installed_inputs = unique_installed_inputs(inputs);
  if (installed_inputs.empty())
    return prepared;
  auto& objects = require_package_object_authority(package_objects);
  const auto scope = dispatch_scope(record, dispatch);
  for (const auto* installed : installed_inputs)
  {
    const auto& build = installed->control().build();
    const auto content = pkgimage::complete_archive_digest::parse(
        build.artifact_content().string());
    const auto image = pkgimage::package_image_identity::parse(
        build.artifact_image().string());
    const auto object = objects.require(content);
    const auto resource = installed_resource_identity(
        record, dispatch, *installed);
    const auto destination =
        configuration.roots().installed_resource_root / scope / resource.hex();
    const auto tree = pkgimage_exec::realize_package_tree(
        {object.path(), content, image, destination});
    if (tree.archive_digest != content || tree.image != image ||
        tree.path != destination)
      throw std::runtime_error(
          "installed package resource realizer returned different authority");
    prepared.retain(
        {installed->identity(), resource, std::move(tree.path)});
  }
  return prepared;
}

std::vector<pkgbuild::build_input> construction_installed_inputs(
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const native_transaction_session_configuration& configuration)
{
  auto request = construction_request::make(
      progress.transaction(), dispatch.unit().primary_node(),
      configuration.policy().build, configuration.policy().acquisition);
  return request.build().inputs().for_scope(pkgbuild::input_scope::build);
}

std::vector<pkgbuild::build_input> check_installed_inputs(
    const transaction_progress& progress,
    const transaction_dispatch& dispatch)
{
  auto request = transaction_check_request::make(
      progress, dispatch.unit().primary_node());
  return request.check().inputs().inputs();
}

} // namespace

native_transaction_resource_session_source::
native_transaction_resource_session_source(
    native_transaction_session_configuration configuration,
    pkgobject::store* package_objects)
    : configuration_(std::move(configuration)),
      package_objects_(package_objects)
{
}

construction_session native_transaction_resource_session_source::construction(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch)
{
  if (dispatch.unit().kind() != transaction_unit_kind::construction)
    throw std::invalid_argument(
        "native construction resource source received another dispatch kind");
  require_construction_package_object_authority(package_objects_);
  auto prepared = prepare_installed_resources(
      record, dispatch, configuration_, package_objects_,
      construction_installed_inputs(progress, dispatch, configuration_));
  native_transaction_dispatch_session_source locator(
      configuration_, prepared);
  return locator.construction(record, progress, dispatch);
}

transaction_check_session native_transaction_resource_session_source::check(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch)
{
  if (dispatch.unit().kind() != transaction_unit_kind::check)
    throw std::invalid_argument(
        "native check resource source received another dispatch kind");
  auto prepared = prepare_installed_resources(
      record, dispatch, configuration_, package_objects_,
      check_installed_inputs(progress, dispatch));
  native_transaction_dispatch_session_source locator(
      configuration_, prepared);
  return locator.check(record, progress, dispatch);
}

} // namespace pkgctl
