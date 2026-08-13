// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <libpkgaudit/audit.h>
#include <libpkgaudit/inventory.h>
#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgstate/owned_entry.h>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

pkgaudit::inventory make_inventory(const pkgstate::snapshot& state)
{
  std::vector<pkgaudit::package_facts> packages;
  packages.reserve(state.packages().size());

  for (const pkgstate::installed_package& installed : state.packages())
  {
    std::vector<pkgaudit::expected_object> objects;
    objects.reserve(installed.manifest().size());
    for (const pkgstate::owned_entry& entry : installed.manifest())
    {
      objects.push_back({
          pkgaudit::object_path::parse(entry.path().string()),
          entry.kind() == pkgstate::owned_object_kind::directory
              ? pkgaudit::expected_object_type::directory
              : pkgaudit::expected_object_type::non_directory,
      });
    }
    packages.emplace_back(installed.release().name(), std::move(objects));
  }
  return pkgaudit::inventory(std::move(packages));
}

std::string_view finding_name(pkgaudit::finding_kind kind)
{
  switch (kind)
  {
  case pkgaudit::finding_kind::missing_object:
    return "missing-object";
  case pkgaudit::finding_kind::object_class_mismatch:
    return "object-class-mismatch";
  case pkgaudit::finding_kind::dangling_symlink:
    return "dangling-symlink";
  case pkgaudit::finding_kind::symlink_loop:
    return "symlink-loop";
  case pkgaudit::finding_kind::symlink_target_outside_root:
    return "symlink-target-outside-root";
  }
  return "unknown";
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
    return 2;

  try
  {
    const auto store = pkgstate::posix::canonical_generation_store::open_existing(
        argv[1], test_support::binding());
    const pkgstate::snapshot state = store.read();
    const pkgaudit::inventory facts = make_inventory(state);
    auto filesystem = pkgaudit::make_posix_filesystem_backend({argv[2], 40});

    pkgaudit::audit_request request;
    request.packages = pkgaudit::package_selection::all();
    request.checks = pkgaudit::check_set({
        pkgaudit::check::object_state,
        pkgaudit::check::symlink_resolution,
        pkgaudit::check::symlink_ownership,
    });
    const pkgaudit::report report =
        pkgaudit::auditor().run(facts, request, *filesystem);

    std::cout << "complete " << (report.complete() ? "yes" : "no") << '\n'
              << "packages " << facts.packages().size() << '\n'
              << "findings " << report.findings().size() << '\n'
              << "relations " << report.relations().size() << '\n'
              << "failures " << report.failures().size() << '\n';
    for (const auto& finding : report.findings())
      std::cout << "finding " << finding_name(finding.kind) << ' '
                << finding.package << ' ' << finding.path.string() << '\n';

    if (!report.complete())
      return 2;
    return report.findings().empty() ? EXIT_SUCCESS : 1;
  }
  catch (const std::exception& error)
  {
    std::cerr << "rootfs-audit-fixture: " << error.what() << '\n';
    return 2;
  }
}
