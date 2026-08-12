// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file construction_codec.h
 *  \brief Versioned durable encoding for admitted construction sessions.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/construction.h>

namespace pkgctl {

/*! \brief Current canonical controller construction-session encoding. */
inline constexpr std::uint16_t construction_session_encoding_version = 1;
/*! \brief Hard refusal bound for one durable construction-session record. */
inline constexpr std::size_t maximum_construction_session_encoding_size =
    16U * 1024U * 1024U;

using construction_session_encoding = std::vector<std::uint8_t>;

enum class construction_codec_error_code : std::uint8_t {
  corrupt_encoding = 1,
  unsupported_encoding = 2,
  authority_mismatch = 3,
};

class construction_codec_error final : public std::runtime_error {
public:
  construction_codec_error(
      construction_codec_error_code code,
      std::string message);

  [[nodiscard]] construction_codec_error_code code() const noexcept;

private:
  construction_codec_error_code code_;
};

/*! \brief Encode one exact admitted controller construction session. */
[[nodiscard]] construction_session_encoding encode_construction_session(
    const construction_session& session);

/*! \brief Decode retained construction-session authority under exact semantics.
 *
 * The transaction and build node are caller-supplied retained semantic
 * authority. Decoding restores paths, package-input resources, policies,
 * execution identity, and compression without consulting the fresh native
 * session locator or any installed-package resource source.
 */
[[nodiscard]] construction_session decode_construction_session(
    const construction_session_encoding& encoding,
    transaction_session transaction,
    pkgtransaction::transaction_node_identity build_node);

} // namespace pkgctl
