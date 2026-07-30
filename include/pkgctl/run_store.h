// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>

#include <pkgctl/run_journal.h>

namespace pkgctl {

/*! \brief Storage boundary for one caller-selected durable run history. */
class transaction_run_journal_store {
public:
  virtual ~transaction_run_journal_store() = default;

  /*! \brief Load the exact snapshot selected by the committed store head. */
  [[nodiscard]] virtual std::optional<transaction_run_journal_record>
  load_latest(const session_identity& journal) const = 0;

  /*! \brief Durably commit one exact successor; exact retries are idempotent. */
  [[nodiscard]] virtual transaction_run_journal_record
  append(const transaction_run_journal_record& record) = 0;
};

/*! \brief Crash-consistent immutable-record and atomic-head POSIX store. */
class posix_transaction_run_journal_store final
    : public transaction_run_journal_store {
public:
  /*! \brief Open an existing caller-owned directory without following it. */
  [[nodiscard]] static posix_transaction_run_journal_store open(
      const std::string& directory);
  /*! \brief Duplicate one already-open caller-owned directory descriptor. */
  [[nodiscard]] static posix_transaction_run_journal_store from_directory_fd(
      int directory_fd);

  posix_transaction_run_journal_store(
      const posix_transaction_run_journal_store&) = delete;
  posix_transaction_run_journal_store& operator=(
      const posix_transaction_run_journal_store&) = delete;
  posix_transaction_run_journal_store(
      posix_transaction_run_journal_store&& other) noexcept;
  posix_transaction_run_journal_store& operator=(
      posix_transaction_run_journal_store&& other) noexcept;
  ~posix_transaction_run_journal_store() override;

  [[nodiscard]] std::optional<transaction_run_journal_record>
  load_latest(const session_identity& journal) const override;

  [[nodiscard]] transaction_run_journal_record
  append(const transaction_run_journal_record& record) override;

private:
  explicit posix_transaction_run_journal_store(int directory_fd) noexcept;
  int directory_fd_ = -1;
};

} // namespace pkgctl
