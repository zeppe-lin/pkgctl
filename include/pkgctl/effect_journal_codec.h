// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file effect_journal_codec.h
 *  \brief Versioned durable encoding for one effect-attempt snapshot.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <pkgctl/effect_journal.h>

namespace pkgctl {

/*! \brief Record-only encoding accepted for pre-head journal histories. */
inline constexpr std::uint16_t effect_attempt_legacy_encoding_version = 1;
/*! \brief Current encoding whose POSIX store requires a durable head. */
inline constexpr std::uint16_t effect_attempt_encoding_version = 2;
/*! \brief Hard refusal bound for one effect-attempt snapshot. */
inline constexpr std::size_t maximum_effect_attempt_encoding_size =
    1024U * 1024U;

using effect_attempt_encoding = std::vector<std::uint8_t>;

[[nodiscard]] effect_attempt_encoding encode_effect_attempt_record(
    const effect_attempt_record& record);

[[nodiscard]] effect_attempt_record decode_effect_attempt_record(
    const effect_attempt_encoding& encoding);

} // namespace pkgctl
