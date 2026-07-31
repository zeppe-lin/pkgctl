// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_drive.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pkgctl {

struct detail_transaction_run_drive_access final {
  static transaction_run_drive_result make(
      transaction_run_drive_policy policy,
      transaction_run_drive_disposition disposition,
      std::vector<transaction_run_advance_result> steps)
  {
    return transaction_run_drive_result(
        std::move(policy), disposition, std::move(steps));
  }
};

namespace {

[[noreturn]] void invalid_drive(const char* message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition,
      message);
}

transaction_run_drive_disposition classify_stop(
    const transaction_run_advance_result& step)
{
  if (step.external_resolution_required())
    return transaction_run_drive_disposition::external_resolution_required;
  if (step.run().progress().complete())
    return transaction_run_drive_disposition::completed;
  if (step.run().stopped())
    return transaction_run_drive_disposition::stopped_after_failure;
  if (step.disposition() == transaction_run_advance_disposition::quiescent)
    return transaction_run_drive_disposition::quiescent_incomplete;
  return transaction_run_drive_disposition::step_limit_reached;
}

bool stops_before_limit(transaction_run_drive_disposition disposition) noexcept
{
  return disposition != transaction_run_drive_disposition::step_limit_reached;
}

void validate_drive_result(
    const transaction_run_drive_policy& policy,
    transaction_run_drive_disposition disposition,
    const std::vector<transaction_run_advance_result>& steps)
{
  if (steps.empty() || steps.size() > policy.maximum_steps())
    invalid_drive("transaction drive has an invalid step count");

  const auto& last = steps.back();
  const auto expected = classify_stop(last);
  if (disposition == transaction_run_drive_disposition::step_limit_reached)
  {
    if (steps.size() != policy.maximum_steps() ||
        expected != transaction_run_drive_disposition::step_limit_reached)
      invalid_drive("transaction drive step-limit result is inconsistent");
  }
  else if (disposition != expected)
  {
    invalid_drive("transaction drive terminal result is inconsistent");
  }

  for (std::size_t index = 1U; index < steps.size(); ++index)
  {
    if (steps[index - 1U].record().journal() != steps[index].record().journal())
      invalid_drive("transaction drive crossed durable run journals");
    if (stops_before_limit(classify_stop(steps[index - 1U])))
      invalid_drive("transaction drive continued after a stopping outcome");
    if (steps[index].record().sequence() <=
        steps[index - 1U].record().sequence())
      invalid_drive("transaction drive did not advance the durable run head");
  }
}

} // namespace

transaction_run_drive_policy::transaction_run_drive_policy(
    std::size_t maximum_steps) noexcept
    : maximum_steps_(maximum_steps)
{
}

transaction_run_drive_policy transaction_run_drive_policy::make(
    std::size_t maximum_steps)
{
  if (maximum_steps == 0U)
    invalid_drive("transaction drive requires a positive step bound");
  return transaction_run_drive_policy(maximum_steps);
}

std::size_t transaction_run_drive_policy::maximum_steps() const noexcept
{
  return maximum_steps_;
}

transaction_run_drive_result::transaction_run_drive_result(
    transaction_run_drive_policy policy,
    transaction_run_drive_disposition disposition,
    std::vector<transaction_run_advance_result> steps)
    : policy_(std::move(policy)), disposition_(disposition),
      steps_(std::move(steps))
{
  validate_drive_result(policy_, disposition_, steps_);
}

transaction_run_drive_disposition
transaction_run_drive_result::disposition() const noexcept
{
  return disposition_;
}

const std::vector<transaction_run_advance_result>&
transaction_run_drive_result::steps() const noexcept
{
  return steps_;
}

const transaction_run_advance_result&
transaction_run_drive_result::last() const noexcept
{
  return steps_.back();
}

const transaction_run& transaction_run_drive_result::run() const noexcept
{
  return last().run();
}

const transaction_run_journal_record&
transaction_run_drive_result::record() const noexcept
{
  return last().record();
}

std::size_t transaction_run_drive_result::durable_step_count()
    const noexcept
{
  return static_cast<std::size_t>(std::count_if(
      steps_.begin(), steps_.end(), [](const auto& step) {
        return step.durable_transition_committed();
      }));
}

bool transaction_run_drive_result::terminal() const noexcept
{
  return disposition_ == transaction_run_drive_disposition::completed ||
      disposition_ ==
          transaction_run_drive_disposition::stopped_after_failure;
}

bool transaction_run_drive_result::external_resolution_required()
    const noexcept
{
  return disposition_ ==
      transaction_run_drive_disposition::external_resolution_required;
}

transaction_run_drive_result drive_transaction_run(
    session_identity journal,
    transaction_run_drive_policy policy,
    transaction_dispatch_nonce_source& nonces,
    transaction_run_advance_authorities authorities,
    transaction_run_advance_drivers drivers,
    transaction_run_advance_stores stores)
{
  std::vector<transaction_run_advance_result> steps;
  steps.reserve(policy.maximum_steps());

  for (std::size_t index = 0U; index < policy.maximum_steps(); ++index)
  {
    auto step = advance_transaction_run_once(
        journal, nonces, authorities, drivers, stores);
    const auto stop = classify_stop(step);
    steps.push_back(std::move(step));
    if (stops_before_limit(stop))
    {
      return detail_transaction_run_drive_access::make(
          std::move(policy), stop, std::move(steps));
    }
  }

  return detail_transaction_run_drive_access::make(
      std::move(policy), transaction_run_drive_disposition::step_limit_reached,
      std::move(steps));
}

} // namespace pkgctl
