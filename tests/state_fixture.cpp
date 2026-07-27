// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
  if (argc != 2)
    return EXIT_FAILURE;
  test_support::initialize_state(argv[1]);
  const auto value = test_support::binding();
  std::cout << "--managed-target " << value.managed_target().string() << ' '
            << "--state-store " << value.state_store().string() << ' '
            << "--root-view " << value.root_view().string() << ' '
            << "--state-backend " << value.state_backend().string() << ' '
            << "--publication-domain "
            << value.publication_domain().string() << '\n';
  return EXIT_SUCCESS;
}
