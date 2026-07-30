// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <pkgctl/run_journal.h>

namespace pkgctl {

/*! \brief Current deterministic binary encoding of a run snapshot. */
inline constexpr std::uint16_t transaction_run_encoding_version = 1;
/*! \brief Hard refusal bound for one self-contained durable snapshot. */
inline constexpr std::size_t maximum_transaction_run_encoding_size =
    16U * 1024U * 1024U;

using transaction_run_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated record into canonical endian-stable bytes. */
[[nodiscard]] transaction_run_encoding encode_transaction_run_record(
    const transaction_run_journal_record& record);

/*! \brief Decode bytes while recomputing every retained authority identity. */
[[nodiscard]] transaction_run_journal_record decode_transaction_run_record(
    const transaction_run_encoding& encoding);

} // namespace pkgctl
