// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file operation_codec.h
 *  \brief Durable controller encoding for admitted native operation sessions.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/run_operation.h>

namespace pkgctl {

/*! \brief Current canonical controller operation-session encoding. */
inline constexpr std::uint16_t operation_session_encoding_version = 1;
/*! \brief Hard refusal bound for one durable operation-session record. */
inline constexpr std::size_t maximum_operation_session_encoding_size =
    16U * 1024U * 1024U;

enum class operation_codec_error_code : std::uint8_t {
  corrupt_encoding = 1,
  unsupported_encoding = 2,
  authority_mismatch = 3,
};

class operation_codec_error final : public std::runtime_error {
public:
  operation_codec_error(operation_codec_error_code code, std::string message);

  [[nodiscard]] operation_codec_error_code code() const noexcept;

private:
  operation_codec_error_code code_;
};

/*! \brief Encode one admitted operation session and its exact planning input. */
[[nodiscard]] operation_session_encoding encode_operation_session(
    const transaction_dispatch& dispatch,
    const native_transaction_operation_specification& specification,
    const native_transaction_lifecycle_configuration& lifecycle,
    const effectful_operation_session& session);

/*! \brief Decode one retained operation session without live specification IO.
 *
 * The journal record, progress, dispatch, and complete package policy are
 * caller-supplied retained controller authority. The policy is not serialized
 * here: its selected configuration adapter owns durable policy encoding.
 * Decoding restores the exact operation specification and lifecycle admission
 * coordinates, then re-runs only pure controller projection/admission. It does
 * not consult the live operation specification source, observe the target, open
 * archives, read canonical state, or inspect host paths.
 */
[[nodiscard]] effectful_operation_session decode_operation_session(
    const operation_session_encoding& encoding,
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgplan::package_policy_snapshot& policy);

} // namespace pkgctl
