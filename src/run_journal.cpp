// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/run_journal.h>

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_record(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record, message);
}

[[noreturn]] void invalid_transition(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition, message);
}

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_nonce,
      "transaction-run nonce contains invalid hexadecimal data");
}

std::string hexadecimal(const transaction_run_nonce::byte_array& bytes)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2U);
  for (const std::uint8_t byte : bytes)
  {
    result.push_back(digits[(byte >> 4U) & 0x0fU]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}

std::string bool_field(bool value)
{
  return value ? "1" : "0";
}

session_identity journal_identity(
    const session_identity& transaction,
    const transaction_dispatch_policy& policy,
    const transaction_run_nonce& nonce)
{
  return make_session_identity(
      "pkgctl/transaction-run-journal-attempt/1",
      {transaction.hex(), policy.identity().hex(), nonce.hex()});
}

std::vector<std::string> record_identity_fields(
    const session_identity& journal,
    const session_identity& transaction,
    const transaction_run_nonce& nonce,
    std::uint64_t sequence,
    const std::optional<session_identity>& previous,
    const session_identity& run,
    const session_identity& progress,
    const pkgstate::installed_state_snapshot_identity& current_state,
    const transaction_dispatch_policy& policy,
    const std::vector<transaction_dispatch_record>& dispatches,
    bool complete,
    bool failed,
    bool stopped)
{
  std::vector<std::string> fields{
      journal.hex(),
      transaction.hex(),
      nonce.hex(),
      std::to_string(sequence),
      previous ? previous->hex() : std::string{},
      run.hex(),
      progress.hex(),
      current_state.string(),
      policy.identity().hex(),
      bool_field(complete),
      bool_field(failed),
      bool_field(stopped),
      std::to_string(dispatches.size()),
  };
  fields.reserve(fields.size() + dispatches.size());
  for (const auto& dispatch : dispatches)
    fields.push_back(dispatch.identity().hex());
  return fields;
}

session_identity record_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const transaction_run_nonce& nonce,
    std::uint64_t sequence,
    const std::optional<session_identity>& previous,
    const session_identity& run,
    const session_identity& progress,
    const pkgstate::installed_state_snapshot_identity& current_state,
    const transaction_dispatch_policy& policy,
    const std::vector<transaction_dispatch_record>& dispatches,
    bool complete,
    bool failed,
    bool stopped)
{
  return make_session_identity(
      "pkgctl/transaction-run-journal-record/1",
      record_identity_fields(
          journal, transaction, nonce, sequence, previous, run, progress,
          current_state, policy, dispatches, complete, failed, stopped));
}

void validate_record_bounds(
    const transaction_dispatch_policy& policy,
    const std::vector<transaction_dispatch_record>& dispatches)
{
  if (dispatches.size() > maximum_transaction_run_dispatch_count)
    invalid_record("transaction-run journal contains too many dispatches");

  std::set<std::string> nonces;
  std::set<std::string> identities;
  std::vector<const transaction_dispatch_record*> active;

  for (const auto& record : dispatches)
  {
    const auto& dispatch = record.dispatch();
    if (dispatch.unit().members().empty() ||
        dispatch.unit().members().size() >
            maximum_transaction_run_member_count)
      invalid_record("durable dispatch has an invalid member count");
    if (dispatch.dependencies().size() >
        maximum_transaction_run_dependency_count)
      invalid_record("durable dispatch has too many dependencies");
    if (record.observations().size() >
        maximum_transaction_run_observation_count)
      invalid_record("durable dispatch has too many observations");
    if (!nonces.insert(dispatch.nonce().hex()).second)
      invalid_record("durable transaction run reuses a dispatch nonce");
    if (!identities.insert(dispatch.identity().hex()).second)
      invalid_record("durable transaction run duplicates a dispatch");

    if (!std::is_sorted(
            dispatch.dependencies().begin(), dispatch.dependencies().end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.node() < rhs.node();
            }) ||
        std::adjacent_find(
            dispatch.dependencies().begin(), dispatch.dependencies().end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.node() == rhs.node();
            }) != dispatch.dependencies().end())
      invalid_record("durable dispatch dependencies are not canonical");

    std::set<std::string> observations;
    for (const auto& observation : record.observations())
      if (!observations.insert(observation.hex()).second)
        invalid_record("durable dispatch repeats an uncertainty observation");

    if (record.active())
      active.push_back(&record);
  }

  for (std::size_t left = 0; left < active.size(); ++left)
  {
    for (std::size_t right = left + 1U; right < active.size(); ++right)
    {
      for (const auto& member : active[left]->dispatch().unit().members())
      {
        const auto& other = active[right]->dispatch().unit().members();
        if (std::find(other.begin(), other.end(), member) != other.end())
          invalid_record("durable active dispatches overlap graph members");
      }
    }
  }

  const auto capacity = [&](transaction_unit_kind kind) {
    switch (kind)
    {
      case transaction_unit_kind::construction:
        return policy.construction_capacity();
      case transaction_unit_kind::check:
        return policy.check_capacity();
      case transaction_unit_kind::operation:
        return policy.operation_capacity();
    }
    return std::size_t{0U};
  };

  for (const auto kind : {
           transaction_unit_kind::construction,
           transaction_unit_kind::check,
           transaction_unit_kind::operation})
  {
    const auto count = static_cast<std::size_t>(std::count_if(
        active.begin(), active.end(), [&](const auto* record) {
          return record->dispatch().unit().kind() == kind;
        }));
    if (count > capacity(kind))
      invalid_record("durable active dispatch count exceeds policy capacity");
  }
}

std::uint64_t retained_transition_count(
    const std::vector<transaction_dispatch_record>& dispatches)
{
  std::uint64_t count = 0U;
  for (const auto& record : dispatches)
  {
    count += 1U;
    switch (record.state())
    {
      case transaction_dispatch_state::reserved:
        break;
      case transaction_dispatch_state::started:
        count += 1U + record.observations().size();
        break;
      case transaction_dispatch_state::completed:
        count += 2U + record.observations().size();
        break;
      case transaction_dispatch_state::released_unstarted:
        count += 1U;
        break;
    }
  }
  return count;
}

void validate_top_level_shape(
    const session_identity& identity,
    const session_identity& journal,
    const session_identity& transaction,
    const transaction_run_nonce& nonce,
    std::uint64_t sequence,
    const std::optional<session_identity>& previous,
    const session_identity& run,
    const session_identity& progress,
    const pkgstate::installed_state_snapshot_identity& current_state,
    const transaction_dispatch_policy& policy,
    const std::vector<transaction_dispatch_record>& dispatches,
    bool complete,
    bool failed,
    bool stopped)
{
  if ((sequence == 0U) != !previous.has_value())
    invalid_record(
        "transaction-run journal predecessor shape disagrees with sequence");
  if (sequence == 0U && !dispatches.empty())
    invalid_record(
        "transaction-run journal admission contains dispatch ownership");
  if (sequence != 0U && dispatches.empty())
    invalid_record(
        "positive transaction-run journal sequence has no dispatch history");
  if (dispatches.size() > sequence)
    invalid_record(
        "transaction-run journal has more reservations than transitions");
  if (sequence != retained_transition_count(dispatches))
    invalid_record(
        "transaction-run journal sequence disagrees with retained history");
  if (complete && failed)
    invalid_record("transaction-run journal is both complete and failed");
  if (stopped !=
      (failed && policy.failure_containment() ==
          transaction_failure_containment::stop_after_terminal_failure))
    invalid_record("transaction-run journal stopped state is inconsistent");

  validate_record_bounds(policy, dispatches);

  const auto expected_journal = journal_identity(transaction, policy, nonce);
  if (journal != expected_journal)
    invalid_record("transaction-run journal identity does not match its fields");

  const auto expected_record = record_identity(
      journal, transaction, nonce, sequence, previous, run, progress,
      current_state, policy, dispatches, complete, failed, stopped);
  if (identity != expected_record)
    invalid_record("transaction-run record identity does not match its fields");
}

bool same_prefix(
    const std::vector<session_identity>& previous,
    const std::vector<session_identity>& next)
{
  return next.size() == previous.size() + 1U &&
      std::equal(previous.begin(), previous.end(), next.begin());
}

void validate_record_transition(
    const transaction_dispatch_record& previous,
    const transaction_dispatch_record& next,
    bool progress_changed)
{
  if (previous.dispatch().identity() != next.dispatch().identity())
    invalid_transition("transaction-run transition changes dispatch authority");

  switch (previous.state())
  {
    case transaction_dispatch_state::reserved:
      if (next.state() == transaction_dispatch_state::started)
      {
        if (progress_changed || !next.attempt_session() ||
            !next.observations().empty() || next.terminal_evidence())
          invalid_transition("reserved-to-started transition has invalid evidence");
        return;
      }
      if (next.state() == transaction_dispatch_state::released_unstarted)
      {
        if (progress_changed || next.attempt_session() ||
            !next.observations().empty() || next.terminal_evidence())
          invalid_transition("released reservation contains execution evidence");
        return;
      }
      break;

    case transaction_dispatch_state::started:
      if (next.state() == transaction_dispatch_state::started)
      {
        if (progress_changed ||
            next.attempt_session() != previous.attempt_session() ||
            next.effect_attempt() != previous.effect_attempt() ||
            next.terminal_evidence() ||
            !same_prefix(previous.observations(), next.observations()))
          invalid_transition("uncertain observation is not one exact append");
        return;
      }
      if (next.state() == transaction_dispatch_state::completed)
      {
        if (!progress_changed ||
            next.attempt_session() != previous.attempt_session() ||
            next.effect_attempt() != previous.effect_attempt() ||
            next.observations() != previous.observations() ||
            !next.terminal_evidence())
          invalid_transition("started-to-completed transition is inconsistent");
        return;
      }
      break;

    case transaction_dispatch_state::completed:
    case transaction_dispatch_state::released_unstarted:
      break;
  }

  invalid_transition("transaction-run record has an unsupported state transition");
}

void validate_transition_snapshots(
    const std::vector<transaction_dispatch_record>& before,
    const session_identity& previous_progress,
    const pkgstate::installed_state_snapshot_identity& previous_state,
    const std::vector<transaction_dispatch_record>& after,
    const session_identity& next_progress)
{
  const bool progress_changed = next_progress != previous_progress;

  if (after.size() == before.size() + 1U)
  {
    if (progress_changed)
      invalid_transition("reservation transition changes progression");
    if (!std::equal(
            before.begin(), before.end(), after.begin(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.identity() == rhs.identity();
            }))
      invalid_transition("reservation transition rewrites prior history");
    if (after.back().state() != transaction_dispatch_state::reserved)
      invalid_transition("new dispatch history entry is not reserved");
    if (after.back().dispatch().reserved_from_progress() != previous_progress ||
        after.back().dispatch().reserved_state() != previous_state)
      invalid_transition(
          "new dispatch reservation is detached from durable progression");
    return;
  }

  if (after.size() != before.size())
    invalid_transition(
        "transaction-run successor changes history by more than one record");

  std::optional<std::size_t> changed;
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    if (before[index].identity() == after[index].identity())
      continue;
    if (changed)
      invalid_transition("transaction-run successor changes multiple records");
    changed = index;
  }
  if (!changed)
    invalid_transition(
        "transaction-run successor does not change a dispatch record");

  validate_record_transition(before[*changed], after[*changed], progress_changed);
}

void validate_live_successor(
    const transaction_run_journal_record& previous,
    const transaction_run& next)
{
  if (next.progress().transaction().identity() != previous.transaction())
    invalid_transition("transaction-run successor changes transaction authority");
  if (next.policy().identity() != previous.policy().identity())
    invalid_transition("transaction-run successor changes dispatch policy");
  if (next.identity() == previous.run())
    invalid_transition("transaction-run successor contains no transition");

  validate_transition_snapshots(
      previous.dispatches(), previous.progress(), previous.current_state(),
      next.records(), next.progress().identity());
}

std::optional<std::size_t> changed_dispatch_index(
    const std::vector<transaction_dispatch_record>& before,
    const std::vector<transaction_dispatch_record>& after)
{
  if (before.size() != after.size())
    return std::nullopt;

  std::optional<std::size_t> changed;
  for (std::size_t index = 0; index < before.size(); ++index)
  {
    if (before[index].identity() == after[index].identity())
      continue;
    if (changed)
      return std::nullopt;
    changed = index;
  }
  return changed;
}

void validate_durable_successor(
    const transaction_run_journal_record& previous,
    const transaction_run_journal_record& next)
{
  if (next.sequence() != previous.sequence() + 1U ||
      !next.previous() || *next.previous() != previous.identity())
    invalid_transition("transaction-run journal sequence chain is invalid");
  if (next.journal() != previous.journal() ||
      next.transaction() != previous.transaction() ||
      next.nonce() != previous.nonce() ||
      next.policy().identity() != previous.policy().identity())
    invalid_transition("transaction-run journal successor changes authority");
  if (next.run() == previous.run())
    invalid_transition("transaction-run journal successor contains no transition");

  validate_transition_snapshots(
      previous.dispatches(), previous.progress(), previous.current_state(),
      next.dispatches(), next.progress());

  const bool progress_changed = next.progress() != previous.progress();
  if (!progress_changed)
  {
    if (next.current_state() != previous.current_state() ||
        next.complete() != previous.complete() ||
        next.failed() != previous.failed() ||
        next.stopped() != previous.stopped())
      invalid_transition(
          "non-terminal journal transition changes progression state");
    return;
  }

  if (next.current_state() != previous.current_state())
  {
    const auto changed = changed_dispatch_index(
        previous.dispatches(), next.dispatches());
    if (!changed ||
        next.dispatches()[*changed].state() !=
            transaction_dispatch_state::completed ||
        next.dispatches()[*changed].dispatch().unit().kind() !=
            transaction_unit_kind::operation)
      invalid_transition(
          "non-operation completion changes canonical state epoch");
  }
}

} // namespace

transaction_run_journal_error::transaction_run_journal_error(
    transaction_run_journal_error_code code,
    std::string message,
    int system_error)
    : std::runtime_error(std::move(message)),
      code_(code), system_error_(system_error)
{
}

transaction_run_journal_error_code
transaction_run_journal_error::code() const noexcept
{
  return code_;
}

int transaction_run_journal_error::system_error() const noexcept
{
  return system_error_;
}

transaction_run_nonce::transaction_run_nonce(byte_array bytes)
    : bytes_(std::move(bytes))
{
}

transaction_run_nonce transaction_run_nonce::from_bytes(byte_array bytes)
{
  const bool all_zero = std::all_of(
      bytes.begin(), bytes.end(), [](std::uint8_t value) {
        return value == 0U;
      });
  if (all_zero)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_nonce,
        "transaction-run nonce must not be all zero");
  return transaction_run_nonce(std::move(bytes));
}

transaction_run_nonce transaction_run_nonce::from_hex(std::string value)
{
  if (value.size() != transaction_run_nonce_size * 2U)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_nonce,
        "transaction-run nonce is not a 32-byte hexadecimal value");
  byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    const auto high = hexadecimal_digit(value[index * 2U]);
    const auto low = hexadecimal_digit(value[index * 2U + 1U]);
    bytes[index] = static_cast<std::uint8_t>((high << 4U) | low);
  }
  return from_bytes(std::move(bytes));
}

const transaction_run_nonce::byte_array&
transaction_run_nonce::bytes() const noexcept
{
  return bytes_;
}

std::string transaction_run_nonce::hex() const
{
  return hexadecimal(bytes_);
}

bool operator==(const transaction_run_nonce& lhs,
                const transaction_run_nonce& rhs) noexcept
{
  return lhs.bytes_ == rhs.bytes_;
}

bool operator!=(const transaction_run_nonce& lhs,
                const transaction_run_nonce& rhs) noexcept
{
  return !(lhs == rhs);
}

transaction_run_journal_record::transaction_run_journal_record(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    transaction_run_nonce nonce,
    std::uint64_t sequence,
    std::optional<session_identity> previous,
    session_identity run,
    session_identity progress,
    pkgstate::installed_state_snapshot_identity current_state,
    transaction_dispatch_policy policy,
    std::vector<transaction_dispatch_record> dispatches,
    bool complete,
    bool failed,
    bool stopped)
    : identity_(std::move(identity)), journal_(std::move(journal)),
      transaction_(std::move(transaction)), nonce_(std::move(nonce)),
      sequence_(sequence), previous_(std::move(previous)), run_(std::move(run)),
      progress_(std::move(progress)), current_state_(std::move(current_state)),
      policy_(std::move(policy)), dispatches_(std::move(dispatches)),
      complete_(complete), failed_(failed), stopped_(stopped)
{
}

transaction_run_journal_record transaction_run_journal_record::admit(
    const transaction_run& run,
    transaction_run_nonce nonce)
{
  if (!run.records().empty())
    invalid_record(
        "durable transaction-run history must begin before first reservation");

  const auto journal = journal_identity(
      run.progress().transaction().identity(), run.policy(), nonce);
  const auto identity = record_identity(
      journal, run.progress().transaction().identity(), nonce, 0U,
      std::nullopt, run.identity(), run.progress().identity(),
      run.progress().current_state().identity(), run.policy(), run.records(),
      run.progress().complete(), run.progress().failed(), run.stopped());

  return transaction_run_journal_record(
      identity, journal, run.progress().transaction().identity(),
      std::move(nonce), 0U, std::nullopt, run.identity(),
      run.progress().identity(), run.progress().current_state().identity(),
      run.policy(), run.records(), run.progress().complete(),
      run.progress().failed(), run.stopped());
}

transaction_run_journal_record transaction_run_journal_record::successor(
    const transaction_run& run) const
{
  if (sequence_ == std::numeric_limits<std::uint64_t>::max())
    invalid_transition("transaction-run journal sequence is exhausted");
  validate_live_successor(*this, run);

  const std::uint64_t next_sequence = sequence_ + 1U;
  const std::optional<session_identity> previous = identity_;
  const auto identity = record_identity(
      journal_, transaction_, nonce_, next_sequence, previous,
      run.identity(), run.progress().identity(),
      run.progress().current_state().identity(), run.policy(), run.records(),
      run.progress().complete(), run.progress().failed(), run.stopped());

  return transaction_run_journal_record(
      identity, journal_, transaction_, nonce_, next_sequence, previous,
      run.identity(), run.progress().identity(),
      run.progress().current_state().identity(), run.policy(), run.records(),
      run.progress().complete(), run.progress().failed(), run.stopped());
}

void transaction_run_journal_record::validate_successor_of(
    const transaction_run_journal_record& previous) const
{
  validate_durable_successor(previous, *this);
}

transaction_dispatch_policy transaction_run_journal_record::restore_policy(
    std::size_t construction_capacity,
    std::size_t check_capacity,
    transaction_failure_containment failure_containment,
    const session_identity& expected_identity)
{
  return transaction_dispatch_policy::restore(
      construction_capacity, check_capacity, failure_containment,
      expected_identity);
}

ready_transaction_unit transaction_run_journal_record::restore_unit(
    const session_identity& transaction,
    transaction_unit_kind kind,
    pkgtransaction::transaction_node_identity primary_node,
    std::vector<pkgtransaction::transaction_node_identity> members,
    const session_identity& expected_identity)
{
  return ready_transaction_unit::restore(
      transaction, kind, std::move(primary_node), std::move(members),
      expected_identity);
}

transaction_dispatch_dependency
transaction_run_journal_record::restore_dependency(
    pkgtransaction::transaction_node_identity node,
    session_identity evidence,
    const session_identity& expected_identity)
{
  return transaction_dispatch_dependency::restore(
      std::move(node), std::move(evidence), expected_identity);
}

transaction_dispatch transaction_run_journal_record::restore_dispatch(
    ready_transaction_unit unit,
    transaction_dispatch_nonce nonce,
    session_identity reserved_from_progress,
    pkgstate::installed_state_snapshot_identity reserved_state,
    std::vector<transaction_dispatch_dependency> dependencies,
    const session_identity& expected_identity)
{
  return transaction_dispatch::restore(
      std::move(unit), std::move(nonce),
      std::move(reserved_from_progress), std::move(reserved_state),
      std::move(dependencies), expected_identity);
}

transaction_dispatch_record
transaction_run_journal_record::restore_dispatch_record(
    transaction_dispatch dispatch,
    transaction_dispatch_state state,
    std::optional<session_identity> attempt_session,
    std::optional<session_identity> effect_attempt,
    std::vector<session_identity> observations,
    std::optional<session_identity> terminal_evidence,
    const session_identity& expected_identity)
{
  return transaction_dispatch_record::restore(
      std::move(dispatch), state, std::move(attempt_session),
      std::move(effect_attempt), std::move(observations),
      std::move(terminal_evidence),
      expected_identity);
}

transaction_run_journal_record transaction_run_journal_record::restore(
    session_identity identity,
    session_identity journal,
    session_identity transaction,
    transaction_run_nonce nonce,
    std::uint64_t sequence,
    std::optional<session_identity> previous,
    session_identity run,
    session_identity progress,
    pkgstate::installed_state_snapshot_identity current_state,
    transaction_dispatch_policy policy,
    std::vector<transaction_dispatch_record> dispatches,
    bool complete,
    bool failed,
    bool stopped)
{
  validate_top_level_shape(
      identity, journal, transaction, nonce, sequence, previous, run,
      progress, current_state, policy, dispatches, complete, failed, stopped);
  return transaction_run_journal_record(
      std::move(identity), std::move(journal), std::move(transaction),
      std::move(nonce), sequence, std::move(previous), std::move(run),
      std::move(progress), std::move(current_state), std::move(policy),
      std::move(dispatches), complete, failed, stopped);
}

transaction_run transaction_run_journal_record::reopen(
    transaction_progress progress) const
{
  if (progress.transaction().identity() != transaction_ ||
      progress.identity() != progress_ ||
      progress.current_state().identity() != current_state_ ||
      progress.complete() != complete_ ||
      progress.failed() != failed_)
  {
    invalid_record(
        "rehydrated progression differs from durable transaction-run authority");
  }

  try
  {
    auto result = transaction_run::restore(
        std::move(progress), policy_, dispatches_, run_);
    if (result.stopped() != stopped_)
      invalid_record("reopened transaction run has a different stopped state");
    return result;
  }
  catch (const error& problem)
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_record,
        std::string("cannot reopen durable transaction run: ") +
            problem.what());
  }
}

std::uint16_t transaction_run_journal_record::schema_version() const noexcept
{
  return schema_version_;
}

const session_identity& transaction_run_journal_record::identity() const noexcept
{
  return identity_;
}

const session_identity& transaction_run_journal_record::journal() const noexcept
{
  return journal_;
}

const session_identity& transaction_run_journal_record::transaction() const noexcept
{
  return transaction_;
}

const transaction_run_nonce& transaction_run_journal_record::nonce() const noexcept
{
  return nonce_;
}

std::uint64_t transaction_run_journal_record::sequence() const noexcept
{
  return sequence_;
}

const std::optional<session_identity>&
transaction_run_journal_record::previous() const noexcept
{
  return previous_;
}

const session_identity& transaction_run_journal_record::run() const noexcept
{
  return run_;
}

const session_identity& transaction_run_journal_record::progress() const noexcept
{
  return progress_;
}

const pkgstate::installed_state_snapshot_identity&
transaction_run_journal_record::current_state() const noexcept
{
  return current_state_;
}

const transaction_dispatch_policy&
transaction_run_journal_record::policy() const noexcept
{
  return policy_;
}

const std::vector<transaction_dispatch_record>&
transaction_run_journal_record::dispatches() const noexcept
{
  return dispatches_;
}

bool transaction_run_journal_record::complete() const noexcept
{
  return complete_;
}

bool transaction_run_journal_record::failed() const noexcept
{
  return failed_;
}

bool transaction_run_journal_record::stopped() const noexcept
{
  return stopped_;
}

} // namespace pkgctl
