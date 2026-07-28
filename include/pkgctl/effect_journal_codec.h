// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <pkgctl/effect_journal.h>
namespace pkgctl {
inline constexpr std::uint16_t effect_attempt_encoding_version = 1;
inline constexpr std::size_t maximum_effect_attempt_encoding_size = 1024U * 1024U;
using effect_attempt_encoding = std::vector<std::uint8_t>;
[[nodiscard]] effect_attempt_encoding encode_effect_attempt_record(
    const effect_attempt_record& record);
[[nodiscard]] effect_attempt_record decode_effect_attempt_record(
    const effect_attempt_encoding& encoding);
} // namespace pkgctl
