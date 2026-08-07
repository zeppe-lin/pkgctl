// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <libpkgstate-posix/canonical_generation_store.h>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
  if (argc != 2)
    return EXIT_FAILURE;

  const auto store = pkgstate::posix::canonical_generation_store::open_existing(
      argv[1], test_support::binding());
  const auto state = store.read();
  std::cout << "snapshot " << state.identity().string() << '\n'
            << "packages " << state.size() << '\n';
  for (const auto& package : state.packages())
  {
    std::cout << "package " << package.release().name() << ' '
              << package.release().version_release() << '\n';
  }
  return EXIT_SUCCESS;
}
