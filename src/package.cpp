// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/package.h>

#include <utility>

namespace pkgctl {
namespace {

bool
valid_package_character(unsigned char value) noexcept
{
  return value > 0x20U && value != 0x7fU && value != '/' && value != '\\';
}

} // namespace

package_name::package_name(std::string value) : value_(std::move(value))
{
}

package_name
package_name::parse(std::string_view value)
{
  if (value.empty())
    throw error(error_code::invalid_package_name, "package name is empty");

  for (const unsigned char character : value)
  {
    if (!valid_package_character(character))
    {
      throw error(error_code::invalid_package_name,
                  "package name is not a line-safe path component");
    }
  }

  return package_name(std::string(value));
}

const std::string&
package_name::string() const noexcept
{
  return value_;
}

bool
operator==(const package_name& lhs, const package_name& rhs) noexcept
{
  return lhs.value_ == rhs.value_;
}

bool
operator!=(const package_name& lhs, const package_name& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator<(const package_name& lhs, const package_name& rhs) noexcept
{
  return lhs.value_ < rhs.value_;
}

} // namespace pkgctl
