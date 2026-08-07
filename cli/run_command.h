// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "options.h"

namespace pkgctl::cli {

/*! Execute one explicitly started or resumed native transaction run boundedly. */
int execute_transaction_run(transaction_run_command command);

} // namespace pkgctl::cli
