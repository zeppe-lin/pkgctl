// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file error.h
 *  \brief Typed pkgctl model failures.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace pkgctl {

/*! \brief Stable construction and validation failure classes. */
enum class error_code {
  invalid_package_name,
  invalid_intent,
  invalid_constraint,
  invalid_outcome,
  invalid_operation,
  duplicate_operation,
  missing_prerequisite,
  cyclic_operation_graph,
};

/*! \brief Exception carrying a stable pkgctl error category. */
class error : public std::invalid_argument {
public:
  error(error_code code, std::string message);
  [[nodiscard]] error_code code() const noexcept;
private:
  error_code code_;
};

} // namespace pkgctl
