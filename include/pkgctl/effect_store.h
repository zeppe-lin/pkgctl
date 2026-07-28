// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <pkgctl/effect_journal.h>
namespace pkgctl {
class effect_journal_store {
public:
  virtual ~effect_journal_store() = default;
  [[nodiscard]] virtual std::optional<effect_attempt_record>
  load_latest(const session_identity& attempt) const = 0;
  [[nodiscard]] virtual effect_attempt_record
  append(const effect_attempt_record& record) = 0;
};

class posix_effect_journal_store final : public effect_journal_store {
public:
  [[nodiscard]] static posix_effect_journal_store open(const std::string& directory);
  [[nodiscard]] static posix_effect_journal_store from_directory_fd(int directory_fd);
  posix_effect_journal_store(const posix_effect_journal_store&) = delete;
  posix_effect_journal_store& operator=(const posix_effect_journal_store&) = delete;
  posix_effect_journal_store(posix_effect_journal_store&& other) noexcept;
  posix_effect_journal_store& operator=(posix_effect_journal_store&& other) noexcept;
  ~posix_effect_journal_store() override;
  [[nodiscard]] std::optional<effect_attempt_record>
  load_latest(const session_identity& attempt) const override;
  [[nodiscard]] effect_attempt_record
  append(const effect_attempt_record& record) override;
private:
  explicit posix_effect_journal_store(int directory_fd) noexcept;
  int directory_fd_ = -1;
};
} // namespace pkgctl
