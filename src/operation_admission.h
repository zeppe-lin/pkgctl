// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <pkgctl/run_operation.h>

namespace pkgctl::detail {

[[nodiscard]] effectful_operation_session admit_native_operation_session(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const native_transaction_operation_specification& specification,
    const pkgplan::package_policy_snapshot& policy,
    const native_transaction_lifecycle_configuration& lifecycle);

} // namespace pkgctl::detail
