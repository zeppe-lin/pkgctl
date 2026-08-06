// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_evidence_codec.h
 *  \brief Canonical durable records for transaction-run execution evidence.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <pkgctl/run_evidence.h>

namespace pkgctl {

inline constexpr std::size_t maximum_transaction_run_evidence_encoding_size =
    64U * 1024U * 1024U;

using transaction_run_evidence_encoding = std::vector<std::uint8_t>;

[[nodiscard]] transaction_run_evidence_encoding
encode_construction_dispatch_evidence(
    const construction_dispatch_evidence_record& record);

[[nodiscard]] construction_dispatch_evidence_record
decode_construction_dispatch_evidence(
    const transaction_run_evidence_encoding& encoding);

[[nodiscard]] transaction_run_evidence_encoding
encode_check_dispatch_evidence(
    const check_dispatch_evidence_record& record);

[[nodiscard]] check_dispatch_evidence_record decode_check_dispatch_evidence(
    const transaction_run_evidence_encoding& encoding);

} // namespace pkgctl
