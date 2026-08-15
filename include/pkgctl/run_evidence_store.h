// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_evidence_store.h
 *  \brief Crash-consistent construction and check evidence storage.
 */
#pragma once

#include <optional>
#include <string>

#include <pkgctl/run_evidence_codec.h>

namespace pkgctl {

/*! \brief Durable typed evidence storage selected by run/dispatch/attempt. */
class transaction_run_evidence_store {
public:
  virtual ~transaction_run_evidence_store() = default;

  [[nodiscard]] virtual construction_dispatch_attempt_record
  publish(const construction_dispatch_attempt_record& record) = 0;

  [[nodiscard]] virtual check_dispatch_attempt_record
  publish(const check_dispatch_attempt_record& record) = 0;

  [[nodiscard]] virtual construction_dispatch_evidence_record
  publish(const construction_dispatch_evidence_record& record) = 0;

  [[nodiscard]] virtual check_dispatch_evidence_record
  publish(const check_dispatch_evidence_record& record) = 0;

  [[nodiscard]] virtual std::optional<construction_dispatch_attempt_record>
  load_construction_attempt(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const = 0;

  [[nodiscard]] virtual std::optional<check_dispatch_attempt_record>
  load_check_attempt(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const = 0;

  [[nodiscard]] virtual std::optional<construction_dispatch_evidence_record>
  load_construction(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const = 0;

  [[nodiscard]] virtual std::optional<check_dispatch_evidence_record>
  load_check(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const = 0;
};

/*! \brief Descriptor-anchored immutable-object and atomic-index POSIX store.
 *
 * Writers hold one exclusive store lock. Existing stores are read under a
 * shared read-only lock; observing an empty caller-created directory creates no
 * lock or other filesystem object.
 */
class posix_transaction_run_evidence_store final
    : public transaction_run_evidence_store {
public:
  [[nodiscard]] static posix_transaction_run_evidence_store open(
      const std::string& directory);
  [[nodiscard]] static posix_transaction_run_evidence_store from_directory_fd(
      int directory_fd);

  posix_transaction_run_evidence_store(
      const posix_transaction_run_evidence_store&) = delete;
  posix_transaction_run_evidence_store& operator=(
      const posix_transaction_run_evidence_store&) = delete;
  posix_transaction_run_evidence_store(
      posix_transaction_run_evidence_store&& other) noexcept;
  posix_transaction_run_evidence_store& operator=(
      posix_transaction_run_evidence_store&& other) noexcept;
  ~posix_transaction_run_evidence_store() override;

  [[nodiscard]] construction_dispatch_attempt_record publish(
      const construction_dispatch_attempt_record& record) override;
  [[nodiscard]] check_dispatch_attempt_record publish(
      const check_dispatch_attempt_record& record) override;
  [[nodiscard]] construction_dispatch_evidence_record publish(
      const construction_dispatch_evidence_record& record) override;
  [[nodiscard]] check_dispatch_evidence_record publish(
      const check_dispatch_evidence_record& record) override;

  [[nodiscard]] std::optional<construction_dispatch_attempt_record>
  load_construction_attempt(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const override;
  [[nodiscard]] std::optional<check_dispatch_attempt_record> load_check_attempt(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const override;

  [[nodiscard]] std::optional<construction_dispatch_evidence_record>
  load_construction(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const override;
  [[nodiscard]] std::optional<check_dispatch_evidence_record> load_check(
      const session_identity& journal,
      const session_identity& dispatch,
      const session_identity& attempt_session) const override;

private:
  explicit posix_transaction_run_evidence_store(int directory_fd) noexcept;
  int directory_fd_ = -1;
};

} // namespace pkgctl
