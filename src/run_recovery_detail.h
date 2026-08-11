// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pkgctl/run_recovery.h>

namespace pkgctl::detail {

[[nodiscard]] construction_result rehydrate_construction_dispatch_evidence(
    const construction_dispatch_evidence_record& evidence,
    construction_dispatch_recovery_context context);

[[nodiscard]] transaction_check_result rehydrate_check_dispatch_evidence(
    const check_dispatch_evidence_record& evidence,
    check_dispatch_recovery_context context);

[[nodiscard]] construction_dispatch_recovery_context
native_construction_recovery_context(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const construction_dispatch_evidence_record& evidence,
    transaction_dispatch_session_source& sessions,
    pkgexec::backend_capability_profile backend);

[[nodiscard]] check_dispatch_recovery_context native_check_recovery_context(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const check_dispatch_evidence_record& evidence,
    transaction_dispatch_session_source& sessions,
    pkgexec::backend_capability_profile backend);

} // namespace pkgctl::detail
