// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libpkgexec/model.h>

#include <iosfwd>
#include <optional>
#include <string_view>

namespace pkgctl::cli {

void render_execution_classification(
    std::ostream& output,
    std::string_view stage,
    const std::optional<pkgexec::execution_failure_kind>& failure,
    const std::optional<pkgexec::process_termination>& termination);

} // namespace pkgctl::cli
