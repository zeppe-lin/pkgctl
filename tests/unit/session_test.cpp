// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <pkgctl/controller.h>

#include <libpkgstate/error.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)
}

int main()
{
  const test_support::temporary_directory temp;
  const auto collection = temp.path() / "collection";
  const auto second_collection = temp.path() / "same-content";
  const auto state = temp.path() / "state";
  test_support::create_collection(collection);
  std::filesystem::copy(collection, second_collection,
                        std::filesystem::copy_options::recursive);
  test_support::initialize_state(state);

  const auto first_catalog = pkgctl::acquire_catalog(
      test_support::catalog_request(collection));
  const auto second_catalog = pkgctl::acquire_catalog(
      test_support::catalog_request(second_collection));
  CHECK(first_catalog.catalog().identity() == second_catalog.catalog().identity());
  CHECK(first_catalog.identity() == second_catalog.identity());
  CHECK(first_catalog.catalog().profiles().profiles().size() == 1);
  CHECK(first_catalog.catalog().candidates().size() == 3);

  const auto first_resolution = pkgctl::resolve_packages(
      test_support::resolution_request(collection, state));
  const auto second_resolution = pkgctl::resolve_packages(
      test_support::resolution_request(collection, state));
  CHECK(first_resolution.identity() == second_resolution.identity());
  CHECK(first_resolution.installed().packages().empty());
  CHECK(first_resolution.resolution().selections().size() >= 3);
  CHECK(first_resolution.resolution().goals().size() == 2);

  const auto first_transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(
          test_support::resolution_request(collection, state)));
  const auto second_transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(
          test_support::resolution_request(collection, state)));
  CHECK(first_transaction.identity() == second_transaction.identity());
  CHECK(!first_transaction.program().nodes().empty());

  bool saw_app_install = false;
  bool saw_tool_build = false;
  for (const auto& node : first_transaction.program().nodes())
  {
    if (node.package().name() == "app" &&
        node.action() == pkgtransaction::transaction_action_kind::install)
      saw_app_install = true;
    if (node.package().name() == "tool" &&
        node.action() == pkgtransaction::transaction_action_kind::build)
      saw_tool_build = true;
  }
  CHECK(saw_app_install);
  CHECK(saw_tool_build);

  const auto missing = temp.path() / "missing-state";
  bool refused = false;
  try
  {
    (void)pkgctl::resolve_packages(
        test_support::resolution_request(collection, missing));
  }
  catch (const pkgstate::store_error&)
  {
    refused = true;
  }
  CHECK(refused);
  CHECK(!std::filesystem::exists(missing));

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
