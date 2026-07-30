// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file effect_journal.h
 *  \brief Durable controller-attempt journal snapshots.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/effect.h>
#include <pkgctl/identity.h>

namespace pkgctl {

inline constexpr std::uint16_t effect_attempt_record_schema_version = 1;
inline constexpr std::size_t effect_attempt_nonce_size = 32;
inline constexpr std::size_t maximum_effect_lifecycle_count = 65536;

/*! \brief Failure class for journal model, codec, and storage contracts. */
enum class effect_journal_error_code : std::uint8_t {
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
};

class effect_journal_error final : public std::runtime_error {
public:
  effect_journal_error(effect_journal_error_code code,
                       std::string message,
                       int system_error = 0);
  [[nodiscard]] effect_journal_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;
private:
  effect_journal_error_code code_;
  int system_error_;
};

/*! \brief Caller-issued nonce distinguishing physical controller attempts. */
class effect_attempt_nonce final {
public:
  using byte_array = std::array<std::uint8_t, effect_attempt_nonce_size>;
  [[nodiscard]] static effect_attempt_nonce from_bytes(byte_array bytes);
  [[nodiscard]] static effect_attempt_nonce from_hex(std::string value);
  [[nodiscard]] const byte_array& bytes() const noexcept;
  [[nodiscard]] std::string hex() const;
  friend bool operator==(const effect_attempt_nonce&, const effect_attempt_nonce&) noexcept;
  friend bool operator!=(const effect_attempt_nonce&, const effect_attempt_nonce&) noexcept;
private:
  explicit effect_attempt_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Last durable boundary represented by one full journal snapshot. */
enum class effect_attempt_stage : std::uint8_t {
  admitted = 1,
  before_lifecycle_intent = 2,
  before_lifecycle_terminal = 3,
  application_intent = 4,
  application_terminal = 5,
  after_lifecycle_intent = 6,
  after_lifecycle_terminal = 7,
  publication_intent = 8,
  publication_terminal = 9,
  terminal = 10,
};

/*! \brief Durable terminal fact for one lifecycle execution. */
class effect_lifecycle_fact final {
public:
  effect_lifecycle_fact(session_identity result, bool succeeded);
  [[nodiscard]] const session_identity& result() const noexcept;
  [[nodiscard]] bool succeeded() const noexcept;
private:
  session_identity result_;
  bool succeeded_;
};

/*! \brief Durable terminal fact for one application handoff. */
class effect_application_fact final {
public:
  effect_application_fact(
      std::string receipt,
      pkgapply::application_attempt_outcome outcome,
      std::optional<std::string> journal,
      std::optional<std::string> completed_evidence);
  [[nodiscard]] const std::string& receipt() const noexcept;
  [[nodiscard]] pkgapply::application_attempt_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<std::string>& journal() const noexcept;
  [[nodiscard]] const std::optional<std::string>& completed_evidence() const noexcept;
private:
  std::string receipt_;
  pkgapply::application_attempt_outcome outcome_;
  std::optional<std::string> journal_;
  std::optional<std::string> completed_evidence_;
};

/*! \brief Durable terminal fact for one state-publication attempt. */
class effect_publication_fact final {
public:
  effect_publication_fact(
      std::string receipt,
      pkgstate::state_publication_outcome outcome,
      std::optional<std::string> resulting_snapshot);
  [[nodiscard]] const std::string& receipt() const noexcept;
  [[nodiscard]] pkgstate::state_publication_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<std::string>& resulting_snapshot() const noexcept;
private:
  std::string receipt_;
  pkgstate::state_publication_outcome outcome_;
  std::optional<std::string> resulting_snapshot_;
};

/*! \brief Immutable full snapshot of one durable controller attempt. */
class effect_attempt_record final {
public:
  [[nodiscard]] static effect_attempt_record admit(
      const session_identity& session,
      std::size_t before_total,
      std::size_t after_total,
      effect_attempt_nonce nonce);

  [[nodiscard]] effect_attempt_record begin_before(std::size_t index) const;
  [[nodiscard]] effect_attempt_record complete_before(
      const pkgapply_exec::lifecycle_execution_result& result) const;
  [[nodiscard]] effect_attempt_record begin_application() const;
  [[nodiscard]] effect_attempt_record complete_application(
      const pkgapply::application_receipt& receipt) const;
  [[nodiscard]] effect_attempt_record begin_after(std::size_t index) const;
  [[nodiscard]] effect_attempt_record complete_after(
      const pkgapply_exec::lifecycle_execution_result& result) const;
  [[nodiscard]] effect_attempt_record begin_publication(
      const pkgstate::transaction_evidence_identity& transaction,
      const pkgstate::state_publication_request& request) const;
  [[nodiscard]] effect_attempt_record complete_publication(
      const pkgstate::state_publication_receipt& receipt) const;
  [[nodiscard]] effect_attempt_record seal_terminal(
      effectful_operation_outcome outcome,
      std::optional<pkgstate::installed_state_snapshot_identity>
          reconciled_state = std::nullopt) const;

  /*! \brief Verify that this snapshot is the exact legal successor. */
  void validate_successor_of(const effect_attempt_record& previous) const;

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
  [[nodiscard]] const session_identity& attempt() const noexcept;
  [[nodiscard]] const session_identity& session() const noexcept;
  [[nodiscard]] const effect_attempt_nonce& nonce() const noexcept;
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  [[nodiscard]] const std::optional<session_identity>& previous() const noexcept;
  [[nodiscard]] std::size_t before_total() const noexcept;
  [[nodiscard]] std::size_t after_total() const noexcept;
  [[nodiscard]] effect_attempt_stage stage() const noexcept;
  [[nodiscard]] const std::optional<std::size_t>& active_index() const noexcept;
  [[nodiscard]] const std::vector<effect_lifecycle_fact>& before() const noexcept;
  [[nodiscard]] const std::optional<effect_application_fact>& application() const noexcept;
  [[nodiscard]] const std::vector<effect_lifecycle_fact>& after() const noexcept;
  [[nodiscard]] const std::optional<std::string>& transaction_evidence() const noexcept;
  [[nodiscard]] const std::optional<std::string>& publication_request() const noexcept;
  [[nodiscard]] const std::optional<effect_publication_fact>& publication() const noexcept;
  [[nodiscard]] const std::optional<effectful_operation_outcome>& terminal_outcome() const noexcept;
  [[nodiscard]] const std::optional<std::string>& reconciled_state() const noexcept;

private:
  friend effect_attempt_record decode_effect_attempt_record(
      const std::vector<std::uint8_t>&);

  [[nodiscard]] static effect_attempt_record restore(
      session_identity identity,
      session_identity attempt,
      session_identity session,
      effect_attempt_nonce nonce,
      std::uint64_t sequence,
      std::optional<session_identity> previous,
      std::size_t before_total,
      std::size_t after_total,
      effect_attempt_stage stage,
      std::optional<std::size_t> active_index,
      std::vector<effect_lifecycle_fact> before,
      std::optional<effect_application_fact> application,
      std::vector<effect_lifecycle_fact> after,
      std::optional<std::string> transaction_evidence,
      std::optional<std::string> publication_request,
      std::optional<effect_publication_fact> publication,
      std::optional<effectful_operation_outcome> terminal_outcome,
      std::optional<std::string> reconciled_state);

  effect_attempt_record(
      session_identity identity,
      session_identity attempt,
      session_identity session,
      effect_attempt_nonce nonce,
      std::uint64_t sequence,
      std::optional<session_identity> previous,
      std::size_t before_total,
      std::size_t after_total,
      effect_attempt_stage stage,
      std::optional<std::size_t> active_index,
      std::vector<effect_lifecycle_fact> before,
      std::optional<effect_application_fact> application,
      std::vector<effect_lifecycle_fact> after,
      std::optional<std::string> transaction_evidence,
      std::optional<std::string> publication_request,
      std::optional<effect_publication_fact> publication,
      std::optional<effectful_operation_outcome> terminal_outcome,
      std::optional<std::string> reconciled_state);

  [[nodiscard]] effect_attempt_record successor(
      effect_attempt_stage stage,
      std::optional<std::size_t> active_index,
      std::vector<effect_lifecycle_fact> before,
      std::optional<effect_application_fact> application,
      std::vector<effect_lifecycle_fact> after,
      std::optional<std::string> transaction_evidence,
      std::optional<std::string> publication_request,
      std::optional<effect_publication_fact> publication,
      std::optional<effectful_operation_outcome> terminal_outcome,
      std::optional<std::string> reconciled_state) const;

  std::uint16_t schema_version_ = effect_attempt_record_schema_version;
  session_identity identity_;
  session_identity attempt_;
  session_identity session_;
  effect_attempt_nonce nonce_;
  std::uint64_t sequence_;
  std::optional<session_identity> previous_;
  std::size_t before_total_;
  std::size_t after_total_;
  effect_attempt_stage stage_;
  std::optional<std::size_t> active_index_;
  std::vector<effect_lifecycle_fact> before_;
  std::optional<effect_application_fact> application_;
  std::vector<effect_lifecycle_fact> after_;
  std::optional<std::string> transaction_evidence_;
  std::optional<std::string> publication_request_;
  std::optional<effect_publication_fact> publication_;
  std::optional<effectful_operation_outcome> terminal_outcome_;
  std::optional<std::string> reconciled_state_;
};

} // namespace pkgctl
