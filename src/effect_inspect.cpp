// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_inspect.h>

#include <string>
#include <utility>

namespace pkgctl {
namespace {

[[noreturn]] void store_failure(
    effect_journal_error_code code,
    const std::string& message)
{
  throw effect_journal_error(code, message);
}

void validate_inspection(
    const effect_attempt_record& record,
    const effect_restart_assessment& assessment)
{
  if (assessment.attempt() != record.attempt() ||
      assessment.record() != record.identity() ||
      assessment.stage() != record.stage())
  {
    store_failure(
        effect_journal_error_code::invalid_record,
        "effect-attempt inspection contradicts its durable record");
  }

  const auto expected = assess_effect_restart(record);
  if (assessment.disposition() != expected.disposition())
  {
    store_failure(
        effect_journal_error_code::invalid_record,
        "effect-attempt inspection disposition is inconsistent");
  }
}

} // namespace

effect_attempt_inspection::effect_attempt_inspection(
    effect_attempt_record record,
    effect_restart_assessment assessment)
    : record_(std::move(record)), assessment_(std::move(assessment))
{
  validate_inspection(record_, assessment_);
}

const effect_attempt_record& effect_attempt_inspection::record() const noexcept
{
  return record_;
}

const effect_restart_assessment&
effect_attempt_inspection::assessment() const noexcept
{
  return assessment_;
}

bool effect_attempt_inspection::terminal() const noexcept
{
  return assessment_.disposition() == effect_restart_disposition::terminal;
}

bool effect_attempt_inspection::automatically_continuable() const noexcept
{
  return assessment_.automatically_continuable();
}

bool effect_attempt_inspection::external_resolution_required() const noexcept
{
  return assessment_.disposition() ==
      effect_restart_disposition::external_resolution_required;
}

effect_attempt_inspection inspect_effect_attempt(
    session_identity attempt,
    const effect_journal_store& store)
{
  auto record = store.load_latest(attempt);
  if (!record)
  {
    store_failure(
        effect_journal_error_code::store_conflict,
        "effect-attempt inspection attempt has no committed store head");
  }
  if (record->attempt() != attempt)
  {
    store_failure(
        effect_journal_error_code::store_contract_violation,
        "effect store returned foreign inspection authority");
  }

  auto assessment = assess_effect_restart(*record);
  return effect_attempt_inspection(
      std::move(*record), std::move(assessment));
}

} // namespace pkgctl
