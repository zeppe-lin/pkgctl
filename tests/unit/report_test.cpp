// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <pkgctl/controller.h>
#include <pkgctl/report.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)
}

int main()
{
  const test_support::temporary_directory temp;
  const auto collection = temp.path() / "collection";
  const auto state = temp.path() / "state";
  test_support::create_collection(collection);
  test_support::initialize_state(state);

  const auto catalog = pkgctl::acquire_catalog(
      test_support::catalog_request(collection));
  const std::string catalog_report = pkgctl::render_report(catalog);
  CHECK(catalog_report.find("session.kind=catalog\n") == 0);
  CHECK(catalog_report.find("catalog.candidates=3\n") != std::string::npos);
  CHECK(catalog_report.find(collection.string()) == std::string::npos);

  const auto resolution = pkgctl::resolve_packages(
      test_support::resolution_request(collection, state));
  const std::string resolution_report = pkgctl::render_report(resolution);
  CHECK(resolution_report.find("session.kind=resolution\n") == 0);
  CHECK(resolution_report.find("goal.0.subject=@base\n") != std::string::npos);
  CHECK(resolution_report.find("state.packages=0\n") != std::string::npos);

  const auto transaction = pkgctl::compose_transaction(
      pkgctl::transaction_request::make(
          test_support::resolution_request(collection, state)));
  const std::string transaction_report = pkgctl::render_report(transaction);
  CHECK(transaction_report.find("session.kind=transaction\n") == 0);
  CHECK(transaction_report.find("transaction.convergence=preserve-unselected\n") !=
        std::string::npos);
  CHECK(transaction_report.find("node.0.identity=") != std::string::npos);
  CHECK(transaction_report == pkgctl::render_report(transaction));

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
