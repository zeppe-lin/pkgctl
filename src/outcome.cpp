// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/outcome.h>

#include <utility>

namespace pkgctl {
namespace {

void
require_diagnostic(const std::string& diagnostic)
{
  if (diagnostic.empty())
    throw error(error_code::invalid_outcome, "step outcome diagnostic is empty");
  if (diagnostic.find('\0') != std::string::npos ||
      diagnostic.find('\r') != std::string::npos)
  {
    throw error(error_code::invalid_outcome,
                "step outcome diagnostic is not presentation-safe");
  }
}

} // namespace

std::string_view
to_string(step_kind value) noexcept
{
  switch (value)
  {
    case step_kind::source_refresh: return "source-refresh";
    case step_kind::source_inspection: return "source-inspection";
    case step_kind::selection: return "selection";
    case step_kind::build: return "build";
    case step_kind::image_inspection: return "image-inspection";
    case step_kind::planning: return "planning";
    case step_kind::application: return "application";
    case step_kind::state_projection: return "state-projection";
    case step_kind::state_publication: return "state-publication";
    case step_kind::lifecycle: return "lifecycle";
    case step_kind::recovery: return "recovery";
  }
  return "unknown";
}

std::string_view
to_string(outcome_state value) noexcept
{
  switch (value)
  {
    case outcome_state::succeeded: return "succeeded";
    case outcome_state::refused: return "refused";
    case outcome_state::failed: return "failed";
    case outcome_state::skipped: return "skipped";
  }
  return "unknown";
}

std::string_view
to_string(failure_domain value) noexcept
{
  switch (value)
  {
    case failure_domain::invalid_input: return "invalid-input";
    case failure_domain::unavailable_authority: return "unavailable-authority";
    case failure_domain::stale_authority: return "stale-authority";
    case failure_domain::policy: return "policy";
    case failure_domain::external_process: return "external-process";
    case failure_domain::filesystem: return "filesystem";
    case failure_domain::integrity: return "integrity";
    case failure_domain::recovery_required: return "recovery-required";
    case failure_domain::internal: return "internal";
  }
  return "unknown";
}

std::string_view
to_string(skip_reason value) noexcept
{
  switch (value)
  {
    case skip_reason::prerequisite_failed: return "prerequisite-failed";
    case skip_reason::excluded_by_policy: return "excluded-by-policy";
    case skip_reason::not_applicable: return "not-applicable";
  }
  return "unknown";
}

step_outcome::step_outcome(step_kind step,
                           outcome_state state,
                           std::optional<failure_domain> failure,
                           std::optional<skip_reason> skip,
                           std::string diagnostic)
    : step_(step),
      state_(state),
      failure_(failure),
      skip_(skip),
      diagnostic_(std::move(diagnostic))
{
}

step_outcome
step_outcome::succeeded(step_kind step)
{
  return step_outcome(step, outcome_state::succeeded,
                      std::nullopt, std::nullopt, {});
}

step_outcome
step_outcome::refused(step_kind step,
                      failure_domain domain,
                      std::string diagnostic)
{
  require_diagnostic(diagnostic);
  return step_outcome(step, outcome_state::refused,
                      domain, std::nullopt, std::move(diagnostic));
}

step_outcome
step_outcome::failed(step_kind step,
                     failure_domain domain,
                     std::string diagnostic)
{
  require_diagnostic(diagnostic);
  return step_outcome(step, outcome_state::failed,
                      domain, std::nullopt, std::move(diagnostic));
}

step_outcome
step_outcome::skipped(step_kind step,
                      skip_reason reason,
                      std::string diagnostic)
{
  require_diagnostic(diagnostic);
  return step_outcome(step, outcome_state::skipped,
                      std::nullopt, reason, std::move(diagnostic));
}

step_kind
step_outcome::step() const noexcept
{
  return step_;
}

outcome_state
step_outcome::state() const noexcept
{
  return state_;
}

const std::optional<failure_domain>&
step_outcome::failure() const noexcept
{
  return failure_;
}

const std::optional<skip_reason>&
step_outcome::skip() const noexcept
{
  return skip_;
}

const std::string&
step_outcome::diagnostic() const noexcept
{
  return diagnostic_;
}

} // namespace pkgctl
