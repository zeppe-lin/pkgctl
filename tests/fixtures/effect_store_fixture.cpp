// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/identity.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

pkgctl::effect_attempt_nonce effect_nonce(std::uint8_t marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cerr << "usage: effect-store-fixture DIRECTORY\n";
    return EXIT_FAILURE;
  }

  try
  {
    namespace fs = std::filesystem;
    const fs::path store_path(argv[1]);
    fs::create_directories(store_path);

    const auto session = pkgctl::make_session_identity(
        "pkgctl/effect-store-fixture/1", {"admitted"});
    const auto admitted = pkgctl::effect_attempt_record::admit(
        session, 0U, 0U, effect_nonce(1U));
    auto store = pkgctl::posix_effect_journal_store::open(store_path.string());
    const auto committed = store.append(admitted);
    std::cout << committed.attempt().hex() << '\n';
  }
  catch (const std::exception& problem)
  {
    std::cerr << "effect-store-fixture: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
