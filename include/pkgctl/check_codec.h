// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file check_codec.h
 *  \brief Versioned durable encoding for admitted transaction-check sessions.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/check.h>

namespace pkgctl {

/*! \brief Current canonical controller check-session encoding. */
inline constexpr std::uint16_t check_session_encoding_version = 1;
/*! \brief Hard refusal bound for one durable check-session record. */
inline constexpr std::size_t maximum_check_session_encoding_size =
    16U * 1024U * 1024U;

using check_session_encoding = std::vector<std::uint8_t>;

enum class check_codec_error_code : std::uint8_t {
  corrupt_encoding = 1,
  unsupported_encoding = 2,
  authority_mismatch = 3,
};

class check_codec_error final : public std::runtime_error {
public:
  check_codec_error(check_codec_error_code code, std::string message);

  [[nodiscard]] check_codec_error_code code() const noexcept;

private:
  check_codec_error_code code_;
};

/*! \brief Encode one exact admitted controller check session. */
[[nodiscard]] check_session_encoding encode_check_session(
    const transaction_check_session& session);

/*! \brief Decode retained check-session authority under an exact request.
 *
 * The request is caller-supplied retained controller authority. Decoding
 * restores the source/package/input resources, paths, execution identity, and
 * resource limits without consulting the fresh native session locator,
 * current check configuration, installed-package resources, or the host
 * filesystem.
 */
[[nodiscard]] transaction_check_session decode_check_session(
    const check_session_encoding& encoding,
    transaction_check_request request);

} // namespace pkgctl
