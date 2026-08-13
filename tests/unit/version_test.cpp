// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <pkgctl/version.h>
#include <string_view>
static_assert(pkgctl::version_major == 0);
static_assert(pkgctl::version_minor == 37);
static_assert(pkgctl::version_patch == 0);
int main()
{
  return std::string_view(pkgctl::version_string) == "0.37.0" ? 0 : 1;
}
