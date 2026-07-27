// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "options.h"

#include <pkgctl/controller.h>
#include <pkgctl/error.h>
#include <pkgctl/report.h>
#include <pkgctl/version.h>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <variant>

#include <libpkgcatalog/error.h>
#include <libpkgcatalog-acquire/acquire.h>
#include <libpkgresolve/error.h>
#include <libpkgsource/error.h>
#include <libpkgsource-yaml/parser.h>
#include <libpkgstate/error.h>
#include <libpkgtransaction/error.h>

namespace {

constexpr int usage_status = 2;

int execute(pkgctl::cli::command command)
{
  std::visit([](auto request) {
    using request_type = std::decay_t<decltype(request)>;
    if constexpr (std::is_same_v<request_type, pkgctl::catalog_request>)
      std::cout << pkgctl::render_report(
          pkgctl::acquire_catalog(std::move(request)));
    else if constexpr (std::is_same_v<request_type,
                                      pkgctl::resolution_request>)
      std::cout << pkgctl::render_report(
          pkgctl::resolve_packages(std::move(request)));
    else
      std::cout << pkgctl::render_report(
          pkgctl::compose_transaction(std::move(request)));
  }, std::move(command));
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc == 2)
  {
    const std::string_view argument(argv[1]);
    if (argument == "--help" || argument == "-h")
    {
      std::cout << pkgctl::cli::help_text();
      return EXIT_SUCCESS;
    }
    if (argument == "--version" || argument == "-V")
    {
      std::cout << "pkgctl " << pkgctl::version_string << '\n';
      return EXIT_SUCCESS;
    }
  }

  try
  {
    return execute(pkgctl::cli::parse_command(argc, argv));
  }
  catch (const pkgctl::cli::usage_error& value)
  {
    std::cerr << "pkgctl: " << value.what() << '\n'
              << pkgctl::cli::help_text();
    return usage_status;
  }
  catch (const pkgcatalog::acquire::error& value)
  {
    std::cerr << "pkgctl: catalog acquisition: "
              << pkgcatalog::acquire::to_string(value.code()) << ": "
              << value.what() << '\n';
  }
  catch (const pkgsource::yaml_adapter::yaml_error& value)
  {
    std::cerr << "pkgctl: yaml: " << value.document() << ':'
              << value.line() << ':' << value.column() << ": "
              << value.path() << ": " << value.what() << '\n';
  }
  catch (const pkgsource::error& value)
  {
    std::cerr << "pkgctl: source authority: " << value.what() << '\n';
  }
  catch (const pkgcatalog::error& value)
  {
    std::cerr << "pkgctl: catalog authority: " << value.what() << '\n';
  }
  catch (const pkgstate::error& value)
  {
    std::cerr << "pkgctl: state authority: " << value.what() << '\n';
  }
  catch (const pkgresolve::error& value)
  {
    std::cerr << "pkgctl: resolution authority: " << value.what() << '\n';
  }
  catch (const pkgtransaction::error& value)
  {
    std::cerr << "pkgctl: transaction authority: " << value.what() << '\n';
  }
  catch (const pkgctl::error& value)
  {
    std::cerr << "pkgctl: controller: " << value.what() << '\n';
  }
  catch (const std::exception& value)
  {
    std::cerr << "pkgctl: " << value.what() << '\n';
  }
  return EXIT_FAILURE;
}
