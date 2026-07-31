// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_inspect.h>

#include <string>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void store_failure(
    transaction_run_journal_error_code code,
    const std::string& message)
{
  throw transaction_run_journal_error(code, message);
}

transaction_run_inspection_disposition classify(
    const transaction_run_journal_record& record,
    const transaction_run_restart_assessment& assessment)
{
  if (record.complete())
    return transaction_run_inspection_disposition::completed;
  if (record.stopped())
    return transaction_run_inspection_disposition::stopped_after_failure;
  if (!assessment.quiescent())
    return transaction_run_inspection_disposition::active;
  return transaction_run_inspection_disposition::quiescent_incomplete;
}

void validate_inspection(
    transaction_run_inspection_disposition disposition,
    const transaction_run_journal_record& record,
    const transaction_run_restart_assessment& assessment)
{
  if (assessment.journal() != record.journal() ||
      assessment.record() != record.identity() ||
      assessment.sequence() != record.sequence())
  {
    store_failure(
        transaction_run_journal_error_code::invalid_record,
        "transaction-run inspection contradicts its durable record");
  }

  const auto expected = classify(record, assessment);
  if (disposition != expected)
  {
    store_failure(
        transaction_run_journal_error_code::invalid_record,
        "transaction-run inspection disposition is inconsistent");
  }
}

} // namespace

transaction_run_inspection::transaction_run_inspection(
    transaction_run_inspection_disposition disposition,
    transaction_run_journal_record record,
    transaction_run_restart_assessment assessment)
    : disposition_(disposition), record_(std::move(record)),
      assessment_(std::move(assessment))
{
  validate_inspection(disposition_, record_, assessment_);
}

transaction_run_inspection_disposition
transaction_run_inspection::disposition() const noexcept
{
  return disposition_;
}

const transaction_run_journal_record&
transaction_run_inspection::record() const noexcept
{
  return record_;
}

const transaction_run_restart_assessment&
transaction_run_inspection::assessment() const noexcept
{
  return assessment_;
}

bool transaction_run_inspection::terminal() const noexcept
{
  return disposition_ == transaction_run_inspection_disposition::completed ||
      disposition_ ==
          transaction_run_inspection_disposition::stopped_after_failure;
}

bool transaction_run_inspection::active() const noexcept
{
  return disposition_ == transaction_run_inspection_disposition::active;
}

bool transaction_run_inspection::external_evidence_required() const noexcept
{
  return assessment_.external_evidence_required();
}

transaction_run_inspection inspect_transaction_run(
    session_identity journal,
    const transaction_run_journal_store& store)
{
  auto record = store.load_latest(journal);
  if (!record)
  {
    store_failure(
        transaction_run_journal_error_code::store_conflict,
        "transaction-run inspection journal has no committed store head");
  }
  if (record->journal() != journal)
  {
    store_failure(
        transaction_run_journal_error_code::store_contract_violation,
        "run store returned foreign inspection authority");
  }

  auto assessment = assess_transaction_run_record(*record);
  const auto disposition = classify(*record, assessment);
  return transaction_run_inspection(
      disposition, std::move(*record), std::move(assessment));
}

} // namespace pkgctl
