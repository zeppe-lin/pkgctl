// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"

#include <pkgctl/error.h>
#include <pkgctl/request.h>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)
#define EXPECT_ERROR(code_value, expression) do { bool caught = false; try { (void)(expression); } catch (const pkgctl::error& value) { caught = value.code() == (code_value); } CHECK(caught); } while (false)
}

int main()
{
  EXPECT_ERROR(pkgctl::error_code::invalid_request,
               pkgctl::catalog_request::make({}));
  EXPECT_ERROR(pkgctl::error_code::invalid_request,
               pkgctl::state_location::make("relative", test_support::binding()));

  const test_support::temporary_directory temp;
  auto catalog = test_support::catalog_request(temp.path());
  CHECK(catalog.collections().size() == 1);
  CHECK(catalog.collections()[0].name().name() == "core");

  EXPECT_ERROR(pkgctl::error_code::invalid_request,
               pkgctl::resolution_request::make(
                   catalog,
                   pkgctl::state_location::make(temp.path(),
                                                test_support::binding()),
                   pkgresolve::architecture_context(
                       pkgsource::architecture_reference("x86_64"),
                       pkgsource::architecture_reference("x86_64")),
                   {}));

  std::vector<pkgresolve::resolution_goal> reordered_goals;
  reordered_goals.emplace_back(
      pkgsource::requirement_scope::run(),
      pkgsource::requirement_subject(pkgsource::package_reference("app")),
      "run");
  reordered_goals.emplace_back(
      pkgsource::requirement_scope::build(),
      pkgsource::requirement_subject(pkgsource::package_reference("app")),
      "build");
  const auto normalized = pkgctl::resolution_request::make(
      catalog,
      pkgctl::state_location::make(temp.path(), test_support::binding()),
      pkgresolve::architecture_context(
          pkgsource::architecture_reference("x86_64"),
          pkgsource::architecture_reference("x86_64")),
      reordered_goals);
  CHECK(normalized.goals().front().scope().kind() ==
        pkgsource::requirement_scope_kind::build);

  std::vector<pkgresolve::resolution_goal> duplicate_goals;
  duplicate_goals.emplace_back(
      pkgsource::requirement_scope::run(),
      pkgsource::requirement_subject(pkgsource::package_reference("app")),
      "first");
  duplicate_goals.emplace_back(
      pkgsource::requirement_scope::run(),
      pkgsource::requirement_subject(pkgsource::package_reference("app")),
      "second");
  EXPECT_ERROR(pkgctl::error_code::invalid_request,
               pkgctl::resolution_request::make(
                   catalog,
                   pkgctl::state_location::make(temp.path(),
                                                test_support::binding()),
                   pkgresolve::architecture_context(
                       pkgsource::architecture_reference("x86_64"),
                       pkgsource::architecture_reference("x86_64")),
                   duplicate_goals));

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
