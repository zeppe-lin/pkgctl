// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <pkgctl/run_admit.h>

namespace pkgctl {
namespace detail {

struct prepared_transaction_run_admission final {
  transaction_run run;
  transaction_run_journal_record record;
};

[[nodiscard]] prepared_transaction_run_admission
prepare_transaction_run_admission(
    transaction_progress progress,
    transaction_dispatch_policy policy,
    transaction_run_nonce_source& nonces);

[[nodiscard]] transaction_run_admission_checkpoint
commit_transaction_run_admission(
    const prepared_transaction_run_admission& prepared,
    transaction_run_journal_store& store);

void validate_existing_transaction_run_admission(
    const transaction_run_journal_record& expected,
    const transaction_run_journal_record& committed);

} // namespace detail
} // namespace pkgctl
