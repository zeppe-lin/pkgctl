// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_evidence.h
 *  \brief Durable construction and check evidence bound to run ownership.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <libpkgbuild-exec/result_codec.h>
#include <libpkgcheck-exec/result_codec.h>
#include <libpkgexec/profile_codec.h>
#include <libpkgfetch/materialization_codec.h>

#include <pkgctl/check_codec.h>
#include <pkgctl/construction_codec.h>
#include <pkgctl/run_journal.h>

namespace pkgctl {

struct detail_run_evidence_codec_access;

inline constexpr std::uint16_t transaction_run_evidence_schema_version = 1;


enum class transaction_run_evidence_error_code : std::uint8_t {
  invalid_record = 1,
  corrupt_encoding = 2,
  unsupported_encoding = 3,
  store_open_failed = 4,
  store_read_failed = 5,
  store_write_failed = 6,
  store_sync_failed = 7,
  store_conflict = 8,
  store_corrupt = 9,
  store_contract_violation = 10,
  evidence_missing = 11,
  recovery_context_mismatch = 12,
  recovery_decode_failed = 13,
};

class transaction_run_evidence_error final : public std::runtime_error {
public:
  transaction_run_evidence_error(
      transaction_run_evidence_error_code code,
      std::string message,
      int system_error = 0);

  [[nodiscard]] transaction_run_evidence_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;

private:
  transaction_run_evidence_error_code code_;
  int system_error_;
};

/*! \brief Durable owner evidence for one started construction dispatch.
 *
 * The record retains the exact controller-owned construction-session bytes,
 * canonical libpkgfetch materialization bytes, and canonical build-execution
 * encoding, including libpkgexec-owned backend-profile bytes. It still does
 * not reconstruct a construction_result by itself: recovery delegates each
 * retained body to its semantic owner before admitting the historical result.
 * Fresh session location and current backend capabilities are not recovery
 * authority.
 */
class construction_dispatch_evidence_record final {
public:
  [[nodiscard]] static construction_dispatch_evidence_record admit(
      const transaction_run_journal_record& started_record,
      const transaction_dispatch& dispatch,
      const construction_result& result);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
  [[nodiscard]] const session_identity& journal() const noexcept;
  [[nodiscard]] const session_identity& transaction() const noexcept;
  [[nodiscard]] const session_identity& dispatch() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  node() const noexcept;
  [[nodiscard]] const session_identity& attempt_session() const noexcept;
  [[nodiscard]] const session_identity& result() const noexcept;
  [[nodiscard]] const session_identity& controller_request() const noexcept;
  [[nodiscard]] const construction_session_encoding&
  session_encoding() const noexcept;
  [[nodiscard]] const pkgfetch::materialization_identity&
  materialization() const noexcept;
  [[nodiscard]] const pkgfetch::source_materialization_encoding&
  materialization_encoding() const noexcept;
  [[nodiscard]] const pkgbuild::build_request_identity&
  build_request() const noexcept;
  [[nodiscard]] const pkgexec::execution_request_identity&
  execution_request() const noexcept;
  [[nodiscard]] const pkgexec::backend_capability_profile_identity&
  backend() const noexcept;
  [[nodiscard]] const pkgexec::backend_capability_profile_encoding&
  backend_encoding() const noexcept;
  [[nodiscard]] const pkgexec::execution_evidence_identity&
  execution() const noexcept;
  [[nodiscard]] const pkgbuild::build_result_identity&
  build() const noexcept;
  [[nodiscard]] const pkgbuild_exec::build_execution_result_encoding&
  encoding() const noexcept;

private:
  friend struct detail_run_evidence_codec_access;

  construction_dispatch_evidence_record(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      session_identity dispatch,
      pkgtransaction::transaction_node_identity node,
      session_identity attempt_session,
      session_identity result,
      session_identity controller_request,
      construction_session_encoding session_encoding,
      pkgfetch::materialization_identity materialization,
      pkgfetch::source_materialization_encoding materialization_encoding,
      pkgbuild::build_request_identity build_request,
      pkgexec::execution_request_identity execution_request,
      pkgexec::backend_capability_profile_identity backend,
      pkgexec::backend_capability_profile_encoding backend_encoding,
      pkgexec::execution_evidence_identity execution,
      pkgbuild::build_result_identity build,
      pkgbuild_exec::build_execution_result_encoding encoding);

  std::uint16_t schema_version_ = transaction_run_evidence_schema_version;
  session_identity identity_;
  session_identity journal_;
  session_identity transaction_;
  session_identity dispatch_;
  pkgtransaction::transaction_node_identity node_;
  session_identity attempt_session_;
  session_identity result_;
  session_identity controller_request_;
  construction_session_encoding session_encoding_;
  pkgfetch::materialization_identity materialization_;
  pkgfetch::source_materialization_encoding materialization_encoding_;
  pkgbuild::build_request_identity build_request_;
  pkgexec::execution_request_identity execution_request_;
  pkgexec::backend_capability_profile_identity backend_;
  pkgexec::backend_capability_profile_encoding backend_encoding_;
  pkgexec::execution_evidence_identity execution_;
  pkgbuild::build_result_identity build_;
  pkgbuild_exec::build_execution_result_encoding encoding_;
};

/*! \brief Durable encoded check evidence for one started check dispatch. */
class check_dispatch_evidence_record final {
public:
  [[nodiscard]] static check_dispatch_evidence_record admit(
      const transaction_run_journal_record& started_record,
      const transaction_dispatch& dispatch,
      const transaction_check_result& result);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
  [[nodiscard]] const session_identity& journal() const noexcept;
  [[nodiscard]] const session_identity& transaction() const noexcept;
  [[nodiscard]] const session_identity& dispatch() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  node() const noexcept;
  [[nodiscard]] const session_identity& attempt_session() const noexcept;
  [[nodiscard]] const session_identity& result() const noexcept;
  [[nodiscard]] const session_identity& controller_request() const noexcept;
  [[nodiscard]] const session_identity& construction() const noexcept;
  [[nodiscard]] const check_session_encoding& session_encoding() const noexcept;
  [[nodiscard]] const pkgcheck::check_request_identity&
  check_request() const noexcept;
  [[nodiscard]] const pkgexec::execution_request_identity&
  execution_request() const noexcept;
  [[nodiscard]] const pkgexec::backend_capability_profile_identity&
  backend() const noexcept;
  [[nodiscard]] const pkgexec::backend_capability_profile_encoding&
  backend_encoding() const noexcept;
  [[nodiscard]] const pkgexec::execution_evidence_identity&
  execution() const noexcept;
  [[nodiscard]] const pkgcheck::check_result_identity& check() const noexcept;
  [[nodiscard]] const pkgcheck_exec::check_execution_result_encoding&
  encoding() const noexcept;

private:
  friend struct detail_run_evidence_codec_access;

  check_dispatch_evidence_record(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      session_identity dispatch,
      pkgtransaction::transaction_node_identity node,
      session_identity attempt_session,
      session_identity result,
      session_identity controller_request,
      session_identity construction,
      check_session_encoding session_encoding,
      pkgcheck::check_request_identity check_request,
      pkgexec::execution_request_identity execution_request,
      pkgexec::backend_capability_profile_identity backend,
      pkgexec::backend_capability_profile_encoding backend_encoding,
      pkgexec::execution_evidence_identity execution,
      pkgcheck::check_result_identity check,
      pkgcheck_exec::check_execution_result_encoding encoding);

  std::uint16_t schema_version_ = transaction_run_evidence_schema_version;
  session_identity identity_;
  session_identity journal_;
  session_identity transaction_;
  session_identity dispatch_;
  pkgtransaction::transaction_node_identity node_;
  session_identity attempt_session_;
  session_identity result_;
  session_identity controller_request_;
  session_identity construction_;
  check_session_encoding session_encoding_;
  pkgcheck::check_request_identity check_request_;
  pkgexec::execution_request_identity execution_request_;
  pkgexec::backend_capability_profile_identity backend_;
  pkgexec::backend_capability_profile_encoding backend_encoding_;
  pkgexec::execution_evidence_identity execution_;
  pkgcheck::check_result_identity check_;
  pkgcheck_exec::check_execution_result_encoding encoding_;
};

} // namespace pkgctl
