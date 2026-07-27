// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file controller.h
 *  \brief Read-only authority orchestration entry points.
 */
#pragma once

#include <pkgctl/session.h>

namespace pkgctl {

[[nodiscard]] catalog_session acquire_catalog(catalog_request request);
[[nodiscard]] resolution_session resolve_packages(resolution_request request);
[[nodiscard]] transaction_session compose_transaction(
    transaction_request request);

} // namespace pkgctl
