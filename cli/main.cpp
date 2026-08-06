// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "options.h"

#include <pkgctl/controller.h>
#include <pkgctl/effect_inspect.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/error.h>
#include <pkgctl/report.h>
#include <pkgctl/run_inspect.h>
#include <pkgctl/run_store.h>
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

const char* effect_journal_error_name(
    pkgctl::effect_journal_error_code code) noexcept
{
  using code_type = pkgctl::effect_journal_error_code;
  switch (code)
  {
    case code_type::invalid_nonce: return "invalid-nonce";
    case code_type::invalid_record: return "invalid-record";
    case code_type::invalid_transition: return "invalid-transition";
    case code_type::corrupt_encoding: return "corrupt-encoding";
    case code_type::unsupported_encoding: return "unsupported-encoding";
    case code_type::store_open_failed: return "store-open-failed";
    case code_type::store_read_failed: return "store-read-failed";
    case code_type::store_write_failed: return "store-write-failed";
    case code_type::store_sync_failed: return "store-sync-failed";
    case code_type::store_conflict: return "store-conflict";
    case code_type::store_corrupt: return "store-corrupt";
    case code_type::store_contract_violation:
      return "store-contract-violation";
  }
  return "unknown";
}

const char* run_journal_error_name(
    pkgctl::transaction_run_journal_error_code code) noexcept
{
  using code_type = pkgctl::transaction_run_journal_error_code;
  switch (code)
  {
    case code_type::invalid_nonce: return "invalid-nonce";
    case code_type::invalid_record: return "invalid-record";
    case code_type::invalid_transition: return "invalid-transition";
    case code_type::corrupt_encoding: return "corrupt-encoding";
    case code_type::unsupported_encoding: return "unsupported-encoding";
    case code_type::store_open_failed: return "store-open-failed";
    case code_type::store_read_failed: return "store-read-failed";
    case code_type::store_write_failed: return "store-write-failed";
    case code_type::store_sync_failed: return "store-sync-failed";
    case code_type::store_conflict: return "store-conflict";
    case code_type::store_corrupt: return "store-corrupt";
    case code_type::store_contract_violation:
      return "store-contract-violation";
  }
  return "unknown";
}

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
    else if constexpr (std::is_same_v<request_type,
                                      pkgctl::transaction_request>)
      std::cout << pkgctl::render_report(
          pkgctl::compose_transaction(std::move(request)));
    else if constexpr (std::is_same_v<
                           request_type,
                           pkgctl::cli::run_inspection_command>)
    {
      auto store = pkgctl::posix_transaction_run_journal_store::open(
          request.store);
      auto inspection = pkgctl::inspect_transaction_run(
          std::move(request.journal), store);
      std::cout << pkgctl::render_report(inspection);
    }
    else
    {
      auto store = pkgctl::posix_effect_journal_store::open(request.store);
      auto inspection = pkgctl::inspect_effect_attempt(
          std::move(request.attempt), store);
      std::cout << pkgctl::render_report(inspection);
    }
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
  catch (const pkgsource::yaml::yaml_error& value)
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
  catch (const pkgctl::effect_journal_error& value)
  {
    std::cerr << "pkgctl: effect journal: "
              << effect_journal_error_name(value.code()) << ": "
              << value.what() << '\n';
  }
  catch (const pkgctl::transaction_run_journal_error& value)
  {
    std::cerr << "pkgctl: transaction-run journal: "
              << run_journal_error_name(value.code()) << ": "
              << value.what() << '\n';
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
