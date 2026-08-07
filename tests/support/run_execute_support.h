// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <pkgctl/run_evidence_store.h>
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


class sequenced_evidence_store final
    : public pkgctl::transaction_run_evidence_store {
public:
  explicit sequenced_evidence_store(
      std::vector<std::string>& trace,
      bool fail_construction = false,
      bool fail_check = false)
      : trace_(trace), fail_construction_(fail_construction),
        fail_check_(fail_check)
  {
  }

  pkgctl::construction_dispatch_evidence_record publish(
      const pkgctl::construction_dispatch_evidence_record& record) override
  {
    trace_.push_back("evidence-construction");
    if (fail_construction_)
      throw pkgctl::transaction_run_evidence_error(
          pkgctl::transaction_run_evidence_error_code::store_write_failed,
          "injected construction-evidence failure");
    if (construction_ && construction_->identity() != record.identity())
      throw pkgctl::transaction_run_evidence_error(
          pkgctl::transaction_run_evidence_error_code::store_conflict,
          "construction evidence conflicts");
    construction_ = record;
    return record;
  }

  pkgctl::check_dispatch_evidence_record publish(
      const pkgctl::check_dispatch_evidence_record& record) override
  {
    trace_.push_back("evidence-check");
    if (fail_check_)
      throw pkgctl::transaction_run_evidence_error(
          pkgctl::transaction_run_evidence_error_code::store_write_failed,
          "injected check-evidence failure");
    if (check_ && check_->identity() != record.identity())
      throw pkgctl::transaction_run_evidence_error(
          pkgctl::transaction_run_evidence_error_code::store_conflict,
          "check evidence conflicts");
    check_ = record;
    return record;
  }

  std::optional<pkgctl::construction_dispatch_evidence_record>
  load_construction(
      const pkgctl::session_identity& journal,
      const pkgctl::session_identity& dispatch,
      const pkgctl::session_identity& attempt_session) const override
  {
    if (!construction_ || construction_->journal() != journal ||
        construction_->dispatch() != dispatch ||
        construction_->attempt_session() != attempt_session)
      return std::nullopt;
    return construction_;
  }

  std::optional<pkgctl::check_dispatch_evidence_record> load_check(
      const pkgctl::session_identity& journal,
      const pkgctl::session_identity& dispatch,
      const pkgctl::session_identity& attempt_session) const override
  {
    if (!check_ || check_->journal() != journal ||
        check_->dispatch() != dispatch ||
        check_->attempt_session() != attempt_session)
      return std::nullopt;
    return check_;
  }

private:
  std::vector<std::string>& trace_;
  bool fail_construction_;
  bool fail_check_;
  std::optional<pkgctl::construction_dispatch_evidence_record> construction_;
  std::optional<pkgctl::check_dispatch_evidence_record> check_;
};

} // namespace run_execute_support
