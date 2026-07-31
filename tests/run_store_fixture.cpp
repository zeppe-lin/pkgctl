// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "construction_fixture.h"

#include <pkgctl/run_journal.h>
#include <pkgctl/run_store.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

pkgctl::transaction_run_nonce run_nonce(std::uint8_t marker)
{
  pkgctl::transaction_run_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_run_nonce::from_bytes(bytes);
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cerr << "usage: run-store-fixture DIRECTORY\n";
    return EXIT_FAILURE;
  }

  try
  {
    namespace fs = std::filesystem;
    using namespace construction_fixture;

    const fs::path run_store_path(argv[1]);
    const fs::path state_path = run_store_path.parent_path() / "run-state";
    fs::create_directories(run_store_path);

    pkgstate::canonical_generation_store state(
        state_path, test_support::binding());
    const std::string payload("source payload\n");
    auto source = tool_source(
        sha256_text(payload),
        tool_source_options{"1.0", false, {}, std::nullopt});
    auto transaction = transaction_session(
        source,
        std::vector<pkgsource::source_snapshot>{dependency_source()},
        state.read(), state_path, false, true, false);
    auto progress = pkgctl::transaction_progress::begin(transaction);
    auto run = pkgctl::transaction_run::begin(
        std::move(progress),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, run_nonce(1U));
    auto store = pkgctl::posix_transaction_run_journal_store::open(
        run_store_path.string());
    auto committed = store.append(admitted);
    std::cout << committed.journal().hex() << '\n';
  }
  catch (const std::exception& problem)
  {
    std::cerr << "run-store-fixture: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
