// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>

#include <utility>

namespace pkgctl {

error::error(error_code code, std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

error_code error::code() const noexcept { return code_; }

} // namespace pkgctl
