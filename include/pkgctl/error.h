// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file error.h
 *  \brief Stable controller-owned validation failures.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace pkgctl {

enum class error_code {
  invalid_request,
  invalid_session,
  invalid_effect_request,
  invalid_effect_session,
  invalid_construction_request,
  invalid_construction_session,
  invalid_preparation_request,
  construction_driver_contract_violation,
  preparation_driver_contract_violation,
  driver_contract_violation,
  identity_failure,
};

class error final : public std::invalid_argument {
public:
  error(error_code code, std::string message);
  [[nodiscard]] error_code code() const noexcept;
private:
  error_code code_;
};

} // namespace pkgctl
