// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pkgctl/effect_restart.h>

namespace pkgctl::detail {

[[nodiscard]] effectful_operation_result rehydrate_terminal_effectful_operation(
    effect_restart_checkpoint checkpoint);

} // namespace pkgctl::detail
