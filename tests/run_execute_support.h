// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <pkgctl/run_store.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace run_execute_support {

class sequenced_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  sequenced_run_store(
      pkgctl::transaction_run_journal_record current,
      std::vector<std::string>& trace,
      std::size_t fail_on_append = 0U)
      : latest_(std::move(current)), trace_(trace),
        fail_on_append_(fail_on_append)
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    if (!latest_ || latest_->journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++append_count_;
    trace_.push_back("run-" + std::to_string(append_count_));
    if (append_count_ == fail_on_append_)
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected run-store failure");
    if (latest_ && latest_->identity() == record.identity())
      return *latest_;
    if (latest_)
      record.validate_successor_of(*latest_);
    latest_ = record;
    return record;
  }

  const pkgctl::transaction_run_journal_record& latest() const
  {
    if (!latest_)
      throw std::runtime_error("run store has no latest record");
    return *latest_;
  }

private:
  std::optional<pkgctl::transaction_run_journal_record> latest_;
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  std::size_t append_count_ = 0U;
};

} // namespace run_execute_support
