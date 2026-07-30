// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_restart.h>

#include <algorithm>
#include <utility>

namespace pkgctl {
namespace {

transaction_dispatch_restart_disposition disposition_for(
    const transaction_dispatch_record& record)
{
  if (record.state() == transaction_dispatch_state::reserved)
    return transaction_dispatch_restart_disposition::release_reserved;

  switch (record.dispatch().unit().kind())
  {
    case transaction_unit_kind::construction:
      return transaction_dispatch_restart_disposition::recover_construction;
    case transaction_unit_kind::check:
      return transaction_dispatch_restart_disposition::recover_check;
    case transaction_unit_kind::operation:
      return transaction_dispatch_restart_disposition::inspect_effect_journal;
  }
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record,
      "active dispatch has an unknown durable unit kind");
}

} // namespace

transaction_dispatch_restart_assessment::
transaction_dispatch_restart_assessment(
    session_identity dispatch,
    transaction_unit_kind kind,
    transaction_dispatch_state state,
    transaction_dispatch_restart_disposition disposition,
    std::optional<session_identity> attempt_session,
    std::optional<session_identity> effect_attempt,
    std::vector<session_identity> observations)
    : dispatch_(std::move(dispatch)), kind_(kind), state_(state),
      disposition_(disposition), attempt_session_(std::move(attempt_session)),
      effect_attempt_(std::move(effect_attempt)),
      observations_(std::move(observations))
{
}

const session_identity&
transaction_dispatch_restart_assessment::dispatch() const noexcept
{
  return dispatch_;
}

transaction_unit_kind
transaction_dispatch_restart_assessment::kind() const noexcept
{
  return kind_;
}

transaction_dispatch_state
transaction_dispatch_restart_assessment::state() const noexcept
{
  return state_;
}

transaction_dispatch_restart_disposition
transaction_dispatch_restart_assessment::disposition() const noexcept
{
  return disposition_;
}

const std::optional<session_identity>&
transaction_dispatch_restart_assessment::attempt_session() const noexcept
{
  return attempt_session_;
}

const std::optional<session_identity>&
transaction_dispatch_restart_assessment::effect_attempt() const noexcept
{
  return effect_attempt_;
}

const std::vector<session_identity>&
transaction_dispatch_restart_assessment::observations() const noexcept
{
  return observations_;
}

bool transaction_dispatch_restart_assessment::external_evidence_required()
    const noexcept
{
  return disposition_ !=
      transaction_dispatch_restart_disposition::release_reserved;
}

transaction_run_restart_assessment::transaction_run_restart_assessment(
    session_identity journal,
    session_identity record,
    std::uint64_t sequence,
    std::vector<transaction_dispatch_restart_assessment> active)
    : journal_(std::move(journal)), record_(std::move(record)),
      sequence_(sequence), active_(std::move(active))
{
}

const session_identity& transaction_run_restart_assessment::journal()
    const noexcept
{
  return journal_;
}

const session_identity& transaction_run_restart_assessment::record()
    const noexcept
{
  return record_;
}

std::uint64_t transaction_run_restart_assessment::sequence() const noexcept
{
  return sequence_;
}

const std::vector<transaction_dispatch_restart_assessment>&
transaction_run_restart_assessment::active() const noexcept
{
  return active_;
}

bool transaction_run_restart_assessment::quiescent() const noexcept
{
  return active_.empty();
}

bool transaction_run_restart_assessment::external_evidence_required()
    const noexcept
{
  return std::any_of(
      active_.begin(), active_.end(), [](const auto& assessment) {
        return assessment.external_evidence_required();
      });
}

transaction_run_restart_assessment assess_reopened_run(
    const transaction_run& run,
    const transaction_run_journal_record& record)
{
  if (run.identity() != record.run() ||
      run.progress().identity() != record.progress() ||
      run.progress().transaction().identity() != record.transaction() ||
      run.policy().identity() != record.policy().identity())
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_record,
        "restart assessment contradicts reopened transaction run");
  std::vector<transaction_dispatch_restart_assessment> active;
  for (const auto& dispatch : record.dispatches())
  {
    if (!dispatch.active())
      continue;
    active.emplace_back(
        dispatch.dispatch().identity(),
        dispatch.dispatch().unit().kind(),
        dispatch.state(),
        disposition_for(dispatch),
        dispatch.attempt_session(),
        dispatch.effect_attempt(),
        dispatch.observations());
  }
  return transaction_run_restart_assessment(
      record.journal(), record.identity(), record.sequence(),
      std::move(active));
}

transaction_run_restart_checkpoint::transaction_run_restart_checkpoint(
    transaction_run run,
    transaction_run_journal_record record,
    transaction_run_restart_assessment assessment)
    : run_(std::move(run)), record_(std::move(record)),
      assessment_(std::move(assessment))
{
}

transaction_run_restart_checkpoint transaction_run_restart_checkpoint::make(
    transaction_progress progress,
    transaction_run_journal_record record)
{
  auto run = record.reopen(std::move(progress));
  auto assessment = assess_reopened_run(run, record);
  return transaction_run_restart_checkpoint(
      std::move(run), std::move(record), std::move(assessment));
}

const transaction_run& transaction_run_restart_checkpoint::run() const noexcept
{
  return run_;
}

const transaction_run_journal_record&
transaction_run_restart_checkpoint::record() const noexcept
{
  return record_;
}

const transaction_run_restart_assessment&
transaction_run_restart_checkpoint::assessment() const noexcept
{
  return assessment_;
}

} // namespace pkgctl
