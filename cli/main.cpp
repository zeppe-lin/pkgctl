// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/version.h>

#include <iostream>
#include <string_view>

namespace {

void
print_help(std::ostream& stream)
{
  stream
      << "usage: pkgctl [--help | --version]\n"
      << "\n"
      << "pkgctl 0.1.0 establishes the Zeppe-Lin package orchestration model.\n"
      << "Package transaction commands are not enabled in this release.\n";
}

} // namespace

int
main(int argc, char** argv)
{
  if (argc == 1)
  {
    print_help(std::cout);
    return 0;
  }

  if (argc == 2)
  {
    const std::string_view argument(argv[1]);
    if (argument == "--help" || argument == "-h")
    {
      print_help(std::cout);
      return 0;
    }
    if (argument == "--version")
    {
      std::cout << "pkgctl " << pkgctl::version_string << '\n';
      return 0;
    }
  }

  std::cerr << "pkgctl: transaction commands are not enabled in 0.1.0\n";
  return 2;
}
