// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file error.h
 *  \brief Stable controller-owned validation failures.
 */
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace pkgctl {

enum class error_code : std::uint8_t {
  invalid_request = 0,
  invalid_session = 1,
  invalid_effect_request = 2,
  invalid_effect_session = 3,
  invalid_construction_request = 4,
  invalid_construction_session = 5,
  invalid_check_request = 6,
  invalid_check_session = 7,
  invalid_preparation_request = 8,
  invalid_progression = 9,
  construction_driver_contract_violation = 10,
  check_driver_contract_violation = 11,
  preparation_driver_contract_violation = 12,
  driver_contract_violation = 13,
  identity_failure = 14,
  invalid_dispatch_policy = 15,
  invalid_transaction_run = 16,
  invalid_dispatch = 17,
};

class error final : public std::invalid_argument {
public:
  error(error_code code, std::string message);
  [[nodiscard]] error_code code() const noexcept;
private:
  error_code code_;
};

} // namespace pkgctl
