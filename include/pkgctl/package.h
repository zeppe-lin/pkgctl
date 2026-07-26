// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file package.h
 *  \brief Orchestrator-owned package selector values.
 */
#pragma once

#include <string>
#include <string_view>

namespace pkgctl {

/*! \brief One validated package name used for selection and graph identity. */
class package_name final {
public:
  [[nodiscard]] static package_name parse(std::string_view value);
  [[nodiscard]] const std::string& string() const noexcept;

  friend bool operator==(const package_name& lhs,
                         const package_name& rhs) noexcept;
  friend bool operator!=(const package_name& lhs,
                         const package_name& rhs) noexcept;
  friend bool operator<(const package_name& lhs,
                        const package_name& rhs) noexcept;
private:
  explicit package_name(std::string value);
  std::string value_;
};

} // namespace pkgctl
