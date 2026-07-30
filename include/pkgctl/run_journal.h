// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_journal.h
 *  \brief Durable append-only snapshots of immutable transaction runs.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/dispatch.h>

namespace pkgctl {

struct detail_run_journal_codec_access;

inline constexpr std::uint16_t transaction_run_record_schema_version = 1;
inline constexpr std::size_t transaction_run_nonce_size = 32U;
inline constexpr std::size_t maximum_transaction_run_dispatch_count = 65536U;
inline constexpr std::size_t maximum_transaction_run_member_count = 65536U;
inline constexpr std::size_t maximum_transaction_run_dependency_count = 65536U;
inline constexpr std::size_t maximum_transaction_run_observation_count = 65536U;

/*! \brief Failure class for run-journal model, codec, and storage contracts. */
enum class transaction_run_journal_error_code : std::uint8_t {
  invalid_nonce = 1,
  invalid_record = 2,
  invalid_transition = 3,
  corrupt_encoding = 4,
  unsupported_encoding = 5,
  store_open_failed = 6,
  store_read_failed = 7,
  store_write_failed = 8,
  store_sync_failed = 9,
  store_conflict = 10,
  store_corrupt = 11,
  store_contract_violation = 12,
};

class transaction_run_journal_error final : public std::runtime_error {
public:
  transaction_run_journal_error(
      transaction_run_journal_error_code code,
      std::string message,
      int system_error = 0);

  [[nodiscard]] transaction_run_journal_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;

private:
  transaction_run_journal_error_code code_;
  int system_error_;
};

/*! \brief Caller-issued nonce distinguishing one durable run history. */
class transaction_run_nonce final {
public:
  using byte_array = std::array<std::uint8_t, transaction_run_nonce_size>;

  [[nodiscard]] static transaction_run_nonce from_bytes(byte_array bytes);
  [[nodiscard]] static transaction_run_nonce from_hex(std::string value);

  [[nodiscard]] const byte_array& bytes() const noexcept;
  [[nodiscard]] std::string hex() const;

  friend bool operator==(const transaction_run_nonce&,
                         const transaction_run_nonce&) noexcept;
  friend bool operator!=(const transaction_run_nonce&,
                         const transaction_run_nonce&) noexcept;

private:
  explicit transaction_run_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief One full immutable snapshot in a single-transition run history. */
class transaction_run_journal_record final {
public:
  /*! \brief Admit one run as sequence zero of a new durable history. */
  [[nodiscard]] static transaction_run_journal_record admit(
      const transaction_run& run,
      transaction_run_nonce nonce);

  /*! \brief Seal one exact successor transaction-run transition. */
  [[nodiscard]] transaction_run_journal_record successor(
      const transaction_run& run) const;

  /*! \brief Validate this record as one exact successor snapshot. */
  void validate_successor_of(
      const transaction_run_journal_record& previous) const;

  /*! \brief Reopen dispatch ownership over exact caller-rehydrated progress. */
  [[nodiscard]] transaction_run reopen(
      transaction_progress progress) const;

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
  [[nodiscard]] const session_identity& journal() const noexcept;
  [[nodiscard]] const session_identity& transaction() const noexcept;
  [[nodiscard]] const transaction_run_nonce& nonce() const noexcept;
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  [[nodiscard]] const std::optional<session_identity>& previous() const noexcept;
  [[nodiscard]] const session_identity& run() const noexcept;
  [[nodiscard]] const session_identity& progress() const noexcept;
  [[nodiscard]] const pkgstate::installed_state_snapshot_identity&
  current_state() const noexcept;
  [[nodiscard]] const transaction_dispatch_policy& policy() const noexcept;
  [[nodiscard]] const std::vector<transaction_dispatch_record>&
  dispatches() const noexcept;
  [[nodiscard]] bool complete() const noexcept;
  [[nodiscard]] bool failed() const noexcept;
  [[nodiscard]] bool stopped() const noexcept;

private:
  friend struct detail_run_journal_codec_access;
  friend transaction_run_journal_record decode_transaction_run_record(
      const std::vector<std::uint8_t>&);

  [[nodiscard]] static transaction_dispatch_policy restore_policy(
      std::size_t construction_capacity,
      std::size_t check_capacity,
      transaction_failure_containment failure_containment,
      const session_identity& expected_identity);

  [[nodiscard]] static ready_transaction_unit restore_unit(
      const session_identity& transaction,
      transaction_unit_kind kind,
      pkgtransaction::transaction_node_identity primary_node,
      std::vector<pkgtransaction::transaction_node_identity> members,
      const session_identity& expected_identity);

  [[nodiscard]] static transaction_dispatch_dependency restore_dependency(
      pkgtransaction::transaction_node_identity node,
      session_identity evidence,
      const session_identity& expected_identity);

  [[nodiscard]] static transaction_dispatch restore_dispatch(
      ready_transaction_unit unit,
      transaction_dispatch_nonce nonce,
      session_identity reserved_from_progress,
      pkgstate::installed_state_snapshot_identity reserved_state,
      std::vector<transaction_dispatch_dependency> dependencies,
      const session_identity& expected_identity);

  [[nodiscard]] static transaction_dispatch_record restore_dispatch_record(
      transaction_dispatch dispatch,
      transaction_dispatch_state state,
      std::optional<session_identity> attempt_session,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations,
      std::optional<session_identity> terminal_evidence,
      const session_identity& expected_identity);

  [[nodiscard]] static transaction_run_journal_record restore(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      transaction_run_nonce nonce,
      std::uint64_t sequence,
      std::optional<session_identity> previous,
      session_identity run,
      session_identity progress,
      pkgstate::installed_state_snapshot_identity current_state,
      transaction_dispatch_policy policy,
      std::vector<transaction_dispatch_record> dispatches,
      bool complete,
      bool failed,
      bool stopped);

  transaction_run_journal_record(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      transaction_run_nonce nonce,
      std::uint64_t sequence,
      std::optional<session_identity> previous,
      session_identity run,
      session_identity progress,
      pkgstate::installed_state_snapshot_identity current_state,
      transaction_dispatch_policy policy,
      std::vector<transaction_dispatch_record> dispatches,
      bool complete,
      bool failed,
      bool stopped);

  std::uint16_t schema_version_ = transaction_run_record_schema_version;
  session_identity identity_;
  session_identity journal_;
  session_identity transaction_;
  transaction_run_nonce nonce_;
  std::uint64_t sequence_;
  std::optional<session_identity> previous_;
  session_identity run_;
  session_identity progress_;
  pkgstate::installed_state_snapshot_identity current_state_;
  transaction_dispatch_policy policy_;
  std::vector<transaction_dispatch_record> dispatches_;
  bool complete_;
  bool failed_;
  bool stopped_;
};

} // namespace pkgctl
