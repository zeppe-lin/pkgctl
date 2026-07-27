// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdexcept>
#include <string>
#include <variant>

#include <pkgctl/request.h>

namespace pkgctl::cli {

using command = std::variant<catalog_request,
                             resolution_request,
                             transaction_request>;

class usage_error final : public std::invalid_argument {
public:
  explicit usage_error(std::string message);
};

[[nodiscard]] command parse_command(int argc, char** argv);
[[nodiscard]] std::string help_text();

} // namespace pkgctl::cli
