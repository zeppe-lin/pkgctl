// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_nonce.h
 *  \brief Canonical nonce authority for one committed transaction-run head.
 */
#pragma once

#include <pkgctl/run_advance.h>

namespace pkgctl {

/*! \brief Derive one replay-safe dispatch nonce from exact committed authority.
 *
 * The committed record and reopened run define one issuance domain. Exact
 * retries against that head return the same nonce; every legal successor head
 * derives another nonce. The derivation is domain-separated controller
 * identity construction, not randomness or caller intent.
 */
[[nodiscard]] transaction_dispatch_nonce
canonical_transaction_dispatch_nonce(
    const transaction_run_journal_record& record,
    const transaction_run& run);

/*! \brief Stateless source using canonical committed-head derivation. */
class canonical_transaction_dispatch_nonce_source final
    : public transaction_dispatch_nonce_source {
public:
  [[nodiscard]] transaction_dispatch_nonce issue(
      const transaction_run_journal_record& record,
      const transaction_run& run) override;
};

} // namespace pkgctl
