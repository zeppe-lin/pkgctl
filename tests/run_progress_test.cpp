// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "construction_fixture.h"

#include <pkgctl/run_progress.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

using namespace construction_fixture;
namespace fs = std::filesystem;

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

pkgctl::transaction_run_nonce journal_nonce(std::uint8_t marker)
{
  pkgctl::transaction_run_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_run_nonce::from_bytes(bytes);
}

pkgctl::transaction_dispatch_nonce dispatch_nonce(std::uint8_t marker)
{
  pkgctl::transaction_dispatch_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
}

class no_effect_store final : public pkgctl::effect_journal_store {
public:
  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity&) const override
  {
    ++loads_;
    return std::nullopt;
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record&) override
  {
    throw std::runtime_error("progress rehydration attempted effect append");
  }

  std::size_t loads() const noexcept { return loads_; }

private:
  mutable std::size_t loads_ = 0U;
};

class construction_context final
    : public pkgctl::transaction_progress_rehydration_context_source {
public:
  explicit construction_context(pkgctl::construction_result result)
      : result_(std::move(result))
  {
  }

  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress& partial,
      const pkgctl::transaction_dispatch& dispatch,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    ++construction_calls_;
    partial_ = partial.identity();
    dispatch_ = dispatch.identity();
    return {
        result_.session(), result_.materialization(),
        result_.build().execution().request(),
        result_.build().execution().backend(),
    };
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected check progress context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::effect_attempt_record&) override
  {
    throw std::runtime_error("unexpected operation progress context request");
  }

  std::size_t construction_calls() const noexcept
  {
    return construction_calls_;
  }

  const std::optional<pkgctl::session_identity>& partial() const noexcept
  {
    return partial_;
  }

  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  {
    return dispatch_;
  }

private:
  pkgctl::construction_result result_;
  std::size_t construction_calls_ = 0U;
  std::optional<pkgctl::session_identity> partial_;
  std::optional<pkgctl::session_identity> dispatch_;
};

class unreachable_context final
    : public pkgctl::transaction_progress_rehydration_context_source {
public:
  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected construction progress context request");
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected check progress context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::effect_attempt_record&) override
  {
    throw std::runtime_error("unexpected operation progress context request");
  }
};

void check_empty_and_active_history()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state(
      temporary.path() / "state", test_support::binding());
  auto transaction = transaction_session(
      tool_source(sha256_text("payload\n"), "1.0", false),
      dependency_source(), state.read(), temporary.path() / "state");
  auto initial = pkgctl::transaction_progress::begin(transaction);
  auto run = pkgctl::transaction_run::begin(
      initial, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(1U));

  const auto evidence_path = temporary.path() / "evidence";
  fs::create_directory(evidence_path);
  auto evidence = pkgctl::posix_transaction_run_evidence_store::open(
      evidence_path.string());
  no_effect_store effects;
  unreachable_context context;
  pkgctl::stored_transaction_progress_rehydration_source source(
      transaction, evidence, effects, context);

  auto restored = source.rehydrate_progress(admitted);
  CHECK(restored.identity() == initial.identity());
  CHECK(effects.loads() == 0U);
  CHECK(pkgctl::rehydrate_transaction_run(admitted, source).run().identity() ==
        run.identity());

  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(1U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  auto reserved = admitted.successor(reservation.run);
  auto reserved_progress = source.rehydrate_progress(reserved);
  CHECK(reserved_progress.identity() == initial.identity());
  CHECK(effects.loads() == 0U);
}

void check_completed_construction_rehydration()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "rehydrated construction payload\n";
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), "1.0", false),
      dependency_source(), state.read(), temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  auto initial = pkgctl::transaction_progress::begin(transaction);
  auto run = pkgctl::transaction_run::begin(
      initial, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(2U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(2U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  auto reserved = admitted.successor(reservation.run);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  auto completed_run = pkgctl::complete_construction_dispatch(
      started_run, *reservation.dispatch, result);
  auto completed = started.successor(completed_run);

  const auto evidence_path = temporary.path() / "evidence";
  fs::create_directory(evidence_path);
  auto evidence = pkgctl::posix_transaction_run_evidence_store::open(
      evidence_path.string());
  auto durable = pkgctl::construction_dispatch_evidence_record::admit(
      started, *reservation.dispatch, result);
  CHECK(evidence.publish(durable).identity() == durable.identity());

  no_effect_store effects;
  construction_context context(result);
  pkgctl::stored_transaction_progress_rehydration_source source(
      transaction, evidence, effects, context);
  auto restored = source.rehydrate_progress(completed);
  CHECK(restored.identity() == completed_run.progress().identity());
  CHECK(restored.current_state().identity() ==
        completed_run.progress().current_state().identity());
  CHECK(context.construction_calls() == 1U);
  CHECK(context.partial() ==
        std::optional<pkgctl::session_identity>(initial.identity()));
  CHECK(context.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(effects.loads() == 0U);
  CHECK(pkgctl::rehydrate_transaction_run(completed, source).run().identity() ==
        completed_run.identity());

  const auto empty_path = temporary.path() / "empty";
  fs::create_directory(empty_path);
  auto empty = pkgctl::posix_transaction_run_evidence_store::open(
      empty_path.string());
  construction_context unused(result);
  pkgctl::stored_transaction_progress_rehydration_source missing(
      transaction, empty, effects, unused);
  bool refused = false;
  try
  {
    (void)missing.rehydrate_progress(completed);
  }
  catch (const pkgctl::transaction_progress_rehydration_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_progress_rehydration_error_code::evidence_missing;
  }
  CHECK(refused);
  CHECK(unused.construction_calls() == 0U);
}

} // namespace

int
main()
{
  check_empty_and_active_history();
  check_completed_construction_rehydration();
  return failures == 0 ? 0 : 1;
}
