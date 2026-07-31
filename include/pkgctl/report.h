// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file report.h
 *  \brief Deterministic line-oriented controller reports.
 */
#pragma once

#include <string>

#include <pkgctl/effect_inspect.h>
#include <pkgctl/run_inspect.h>
#include <pkgctl/session.h>

namespace pkgctl {

[[nodiscard]] std::string render_report(const catalog_session& session);
[[nodiscard]] std::string render_report(const resolution_session& session);
[[nodiscard]] std::string render_report(const transaction_session& session);
[[nodiscard]] std::string render_report(
    const effect_attempt_inspection& inspection);
[[nodiscard]] std::string render_report(
    const transaction_run_inspection& inspection);

} // namespace pkgctl
