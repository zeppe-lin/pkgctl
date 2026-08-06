// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "construction_fixture.h"

#include <pkgctl/run_journal.h>
#include <pkgctl/report.h>
#include <pkgctl/run_inspect.h>
#include <pkgctl/run_journal_codec.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_store.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace construction_fixture;
namespace fs = std::filesystem;

int failures = 0;
#define CHECK(value)                                                         \
  do {                                                                       \
    if (!(value)) {                                                          \
      std::cerr << "CHECK failed: " #value "\n";                          \
      ++failures;                                                            \
    }                                                                        \
  } while (false)

template<typename Function>
bool rejects(
    pkgctl::transaction_run_journal_error_code expected,
    Function&& function)
{
  try
  {
    function();
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    return problem.code() == expected;
  }
  return false;
}

pkgctl::transaction_run_nonce run_nonce(std::uint8_t marker)
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

std::string journal_record_name(
    const pkgctl::transaction_run_journal_record& record)
{
  std::ostringstream sequence;
  sequence << std::setw(20) << std::setfill('0') << record.sequence();
  return record.journal().hex() + "-" + sequence.str() + "-" +
      record.identity().hex() + ".pjr";
}

std::string journal_head_name(
    const pkgctl::transaction_run_journal_record& record)
{
  return record.journal().hex() + ".pjh";
}

std::vector<char> read_bytes(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read run-journal test file");
  return std::vector<char>(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

void replace_bytes(const fs::path& path, const std::vector<char>& bytes)
{
  if (::chmod(path.c_str(), 0644) != 0)
    throw std::runtime_error("cannot make run-journal test file writable");
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("cannot replace run-journal test file");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output || ::chmod(path.c_str(), 0444) != 0)
    throw std::runtime_error("cannot seal replaced run-journal test file");
}

void write_read_only(
    const fs::path& path,
    const pkgctl::transaction_run_encoding& bytes)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("cannot write run-journal test file");
  output.write(
      reinterpret_cast<const char*>(bytes.data()),
      static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output || ::chmod(path.c_str(), 0444) != 0)
    throw std::runtime_error("cannot seal run-journal test file");
}

mode_t file_mode(const fs::path& path)
{
  struct stat status{};
  if (::stat(path.c_str(), &status) != 0)
    throw std::runtime_error("cannot inspect run-journal test file");
  return status.st_mode;
}

std::string bool_field(bool value)
{
  return value ? "1" : "0";
}

pkgctl::session_identity record_identity_for(
    const pkgctl::transaction_run_journal_record& record,
    std::uint64_t sequence,
    const std::optional<pkgctl::session_identity>& previous,
    const pkgstate::installed_state_snapshot_identity& current_state)
{
  std::vector<std::string> fields{
      record.journal().hex(),
      record.transaction().hex(),
      record.nonce().hex(),
      std::to_string(sequence),
      previous ? previous->hex() : std::string{},
      record.run().hex(),
      record.progress().hex(),
      current_state.string(),
      record.policy().identity().hex(),
      bool_field(record.complete()),
      bool_field(record.failed()),
      bool_field(record.stopped()),
      std::to_string(record.dispatches().size()),
  };
  for (const auto& dispatch : record.dispatches())
    fields.push_back(dispatch.identity().hex());
  return pkgctl::make_session_identity(
      "pkgctl/transaction-run-journal-record/1", fields);
}

std::uint8_t hex_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  return static_cast<std::uint8_t>(value - 'a' + 10);
}

void write_identity(
    pkgctl::transaction_run_encoding& encoding,
    std::size_t offset,
    const pkgctl::session_identity& identity)
{
  const auto hex = identity.hex();
  for (std::size_t index = 0U; index < 32U; ++index)
    encoding[offset + index] = static_cast<std::uint8_t>(
        (hex_digit(hex[index * 2U]) << 4U) |
        hex_digit(hex[index * 2U + 1U]));
}

void write_u64(
    pkgctl::transaction_run_encoding& encoding,
    std::size_t offset,
    std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    encoding[offset++] = static_cast<std::uint8_t>(
        (value >> static_cast<unsigned int>(shift)) & 0xffU);
}

std::uint32_t read_u32(
    const pkgctl::transaction_run_encoding& encoding,
    std::size_t offset)
{
  std::uint32_t value = 0U;
  for (std::size_t index = 0U; index < 4U; ++index)
    value = (value << 8U) | encoding[offset + index];
  return value;
}

void replace_current_state(
    pkgctl::transaction_run_encoding& encoding,
    std::size_t length_offset,
    const pkgstate::installed_state_snapshot_identity& state)
{
  const auto text = state.string();
  const auto length = read_u32(encoding, length_offset);
  if (length != text.size())
    throw std::runtime_error("run-journal state identity length changed");
  std::copy(text.begin(), text.end(),
            encoding.begin() +
                static_cast<std::ptrdiff_t>(length_offset + 4U));
}

const pkgtransaction::transaction_node& build_node_for(
    const pkgctl::transaction_session& transaction,
    const std::string& package)
{
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == package)
      return node;
  }
  throw std::runtime_error("run journal fixture lacks build node");
}

pkgctl::construction_session construction_session_for(
    const pkgctl::transaction_session& transaction,
    const fs::path& root,
    const std::string& package)
{
  const auto& node = build_node_for(transaction, package);
  auto request = pkgctl::construction_request::make(
      transaction, node.identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(
              2U, 0022, 1700000000)));

  pkgctl::construction_paths paths{
      root / "sources",
      root / "store",
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          root / "root-view",
          root / "session",
          root / "package",
          root / "artifact" / (package + ".tar"),
      },
  };
  fs::create_directories(paths.local_source_root);
  fs::create_directories(paths.build.root_view_path);

  return pkgctl::construction_session::admit(
      std::move(request), std::move(paths), {},
      {
          pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
          static_cast<std::uint64_t>(::geteuid()),
          static_cast<std::uint64_t>(::getegid()),
          {},
      });
}

void stage_fixture_source(
    const pkgctl::construction_session& session,
    const std::string& payload)
{
  const auto& sources = session.request().source().recipe().sources();
  if (sources.empty())
    return;
  if (sources.size() != 1U ||
      sources.front().kind() != pkgsource::source_input_kind::local)
    throw std::runtime_error(
        "run journal fixture requires one package-local source");
  test_support::write(
      session.paths().local_source_root / sources.front().location(), payload);
}

pkgctl::construction_result execute_build(
    const pkgctl::construction_session& session,
    const std::string& payload)
{
  stage_fixture_source(session, payload);
  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  return pkgctl::execute_construction(session, driver);
}

class inspection_store final : public pkgctl::transaction_run_journal_store {
public:
  explicit inspection_store(
      std::optional<pkgctl::transaction_run_journal_record> record)
      : record_(std::move(record))
  {
  }

  [[nodiscard]] std::optional<pkgctl::transaction_run_journal_record>
  load_latest(const pkgctl::session_identity&) const override
  {
    ++load_calls_;
    return record_;
  }

  [[nodiscard]] pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record&) override
  {
    ++append_calls_;
    throw std::runtime_error("inspection store append must not be called");
  }

  [[nodiscard]] std::size_t load_calls() const noexcept { return load_calls_; }
  [[nodiscard]] std::size_t append_calls() const noexcept { return append_calls_; }

private:
  std::optional<pkgctl::transaction_run_journal_record> record_;
  mutable std::size_t load_calls_ = 0U;
  std::size_t append_calls_ = 0U;
};

struct fixture final {
  test_support::temporary_directory temporary;
  pkgstate::posix::canonical_generation_store store;
  std::string payload;
  pkgsource::source_snapshot source;
  pkgctl::transaction_session transaction;
  pkgctl::transaction_progress progress;

  fixture()
      : store(temporary.path() / "state", test_support::binding()),
        payload("source payload\n"),
        source(tool_source(
            sha256_text(payload),
            tool_source_options{"1.0", false, {}, std::nullopt})),
        transaction(transaction_session(
            source,
            std::vector<pkgsource::source_snapshot>{dependency_source()},
            store.read(), temporary.path() / "state",
            false, true, false)),
        progress(pkgctl::transaction_progress::begin(transaction))
  {
  }
};

void check_read_only_run_inspection()
{
  fixture value;
  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(41U));

  inspection_store missing(std::nullopt);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_conflict,
      [&] {
        (void)pkgctl::inspect_transaction_run(admitted.journal(), missing);
      }));
  CHECK(missing.load_calls() == 1U);
  CHECK(missing.append_calls() == 0U);

  inspection_store initial(admitted);
  const auto initial_view =
      pkgctl::inspect_transaction_run(admitted.journal(), initial);
  CHECK(initial_view.disposition() ==
        pkgctl::transaction_run_inspection_disposition::quiescent_incomplete);
  CHECK(initial_view.record().identity() == admitted.identity());
  CHECK(initial_view.assessment().quiescent());
  CHECK(!initial_view.terminal());
  CHECK(!initial_view.active());
  CHECK(!initial_view.external_evidence_required());
  CHECK(initial.load_calls() == 1U);
  CHECK(initial.append_calls() == 0U);
  const auto initial_report = pkgctl::render_report(initial_view);
  CHECK(initial_report.find("session.kind=transaction-run\n") !=
        std::string::npos);
  CHECK(initial_report.find(
            "run.journal=" + admitted.journal().hex() + "\n") !=
        std::string::npos);
  CHECK(initial_report.find("run.disposition=quiescent-incomplete\n") !=
        std::string::npos);
  CHECK(initial_report.find("run.active=0\n") != std::string::npos);

  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(42U));
  CHECK(reservation.dispatch.has_value());
  auto reserved = admitted.successor(reservation.run);
  inspection_store reserved_store(reserved);
  const auto reserved_view =
      pkgctl::inspect_transaction_run(reserved.journal(), reserved_store);
  CHECK(reserved_view.disposition() ==
        pkgctl::transaction_run_inspection_disposition::active);
  CHECK(reserved_view.active());
  CHECK(!reserved_view.terminal());
  CHECK(!reserved_view.external_evidence_required());
  CHECK(reserved_view.assessment().active().size() == 1U);
  CHECK(reserved_view.assessment().active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::release_reserved);
  const auto reserved_report = pkgctl::render_report(reserved_view);
  CHECK(reserved_report.find("run.disposition=active\n") !=
        std::string::npos);
  CHECK(reserved_report.find("active.0.disposition=release-reserved\n") !=
        std::string::npos);

  const auto package =
      reservation.dispatch->unit().primary_node() ==
              build_node_for(value.transaction, "tool").identity()
          ? std::string("tool")
          : std::string("dep");
  auto session = construction_session_for(
      value.transaction, value.temporary.path() / "inspection-start", package);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);
  inspection_store started_store(started);
  const auto started_view =
      pkgctl::inspect_transaction_run(started.journal(), started_store);
  CHECK(started_view.active());
  CHECK(started_view.external_evidence_required());
  CHECK(started_view.assessment().active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::
            recover_construction);
  const auto started_report = pkgctl::render_report(started_view);
  CHECK(started_report.find(
            "active.0.disposition=recover-construction\n") !=
        std::string::npos);
  CHECK(started_report.find("run.external-evidence-required=true\n") !=
        std::string::npos);

  stage_fixture_source(session, value.payload);
  fixture_backend failed_backend(backend_mode::fail);
  pkgctl::native_construction_driver failed_driver(failed_backend);
  const auto failed_result = pkgctl::execute_construction(session, failed_driver);
  auto stopped_run = pkgctl::complete_construction_dispatch(
      started_run, *reservation.dispatch, failed_result);
  auto stopped = started.successor(stopped_run);
  inspection_store stopped_store(stopped);
  const auto stopped_view =
      pkgctl::inspect_transaction_run(stopped.journal(), stopped_store);
  CHECK(stopped_view.disposition() ==
        pkgctl::transaction_run_inspection_disposition::
            stopped_after_failure);
  CHECK(stopped_view.terminal());
  CHECK(!stopped_view.active());
  CHECK(!stopped_view.external_evidence_required());
  CHECK(pkgctl::render_report(stopped_view).find(
            "run.disposition=stopped-after-failure\n") !=
        std::string::npos);

  auto foreign_run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto foreign = pkgctl::transaction_run_journal_record::admit(
      foreign_run, run_nonce(43U));
  inspection_store foreign_store(foreign);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_contract_violation,
      [&] {
        (void)pkgctl::inspect_transaction_run(
            admitted.journal(), foreign_store);
      }));
  CHECK(foreign_store.append_calls() == 0U);
}

void check_nonce_contract()
{
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_nonce, [] {
        (void)pkgctl::transaction_run_nonce::from_bytes({});
      }));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_nonce, [] {
        (void)pkgctl::transaction_run_nonce::from_hex("01");
      }));
  const auto first = run_nonce(1U);
  CHECK(pkgctl::transaction_run_nonce::from_hex(first.hex()) == first);
}

void check_journal_transition_and_reopen()
{
  fixture value;
  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(2U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(2U));
  auto repeated_admission = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(2U));
  auto other_history = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(3U));

  CHECK(admitted.identity() == repeated_admission.identity());
  CHECK(admitted.journal() == repeated_admission.journal());
  CHECK(admitted.journal() != other_history.journal());
  CHECK(admitted.sequence() == 0U);
  CHECK(!admitted.previous());
  CHECK(admitted.dispatches().empty());
  CHECK(admitted.reopen(value.progress).identity() == run.identity());
  CHECK(pkgctl::transaction_run_restart_checkpoint::make(
            run.progress(), admitted).assessment().quiescent());

  const auto reservation = pkgctl::reserve_next(run, dispatch_nonce(3U));
  CHECK(reservation.dispatch.has_value());
  const auto reserved = admitted.successor(reservation.run);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_record,
      [&] {
        (void)pkgctl::transaction_run_journal_record::admit(
            reservation.run, run_nonce(10U));
      }));
  CHECK(reserved.sequence() == 1U);
  CHECK(reserved.previous() == admitted.identity());
  CHECK(reserved.dispatches().size() == 1U);
  CHECK(reserved.dispatches().front().state() ==
        pkgctl::transaction_dispatch_state::reserved);
  reserved.validate_successor_of(admitted);

  const auto reserved_restart =
      pkgctl::transaction_run_restart_checkpoint::make(
          reservation.run.progress(), reserved).assessment();
  CHECK(reserved_restart.active().size() == 1U);
  CHECK(!reserved_restart.external_evidence_required());
  CHECK(reserved_restart.active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::release_reserved);

  const auto package =
      reservation.dispatch->unit().primary_node() ==
              build_node_for(value.transaction, "tool").identity()
          ? std::string("tool")
          : std::string("dep");
  auto session = construction_session_for(
      value.transaction, value.temporary.path() / "build", package);
  const auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  const auto started = reserved.successor(started_run);
  CHECK(started.sequence() == 2U);
  CHECK(started.dispatches().front().attempt_session() == session.identity());

  const auto started_restart =
      pkgctl::transaction_run_restart_checkpoint::make(
          started_run.progress(), started).assessment();
  CHECK(started_restart.external_evidence_required());
  CHECK(started_restart.active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::recover_construction);

  auto result = execute_build(session, value.payload);
  const auto completed_run = pkgctl::complete_construction_dispatch(
      started_run, *reservation.dispatch, result);
  const auto completed = started.successor(completed_run);
  CHECK(completed.sequence() == 3U);
  CHECK(completed.dispatches().front().state() ==
        pkgctl::transaction_dispatch_state::completed);
  CHECK(completed.dispatches().front().terminal_evidence() == result.identity());
  CHECK(pkgctl::transaction_run_restart_checkpoint::make(
            completed_run.progress(), completed).assessment().quiescent());
  CHECK(completed.reopen(completed_run.progress()).identity() ==
        completed_run.identity());

  const auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
      completed_run.progress(), completed);
  CHECK(checkpoint.run().identity() == completed_run.identity());
  CHECK(checkpoint.record().identity() == completed.identity());
  CHECK(checkpoint.assessment().quiescent());

  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_transition,
      [&] { (void)admitted.successor(started_run); }));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_transition,
      [&] { (void)completed.successor(completed_run); }));

  fixture foreign;
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_record,
      [&] { (void)completed.reopen(foreign.progress); }));
}

void check_parallel_restart_rehydration()
{
  fixture value;
  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(2U, 1U));
  auto journal = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(11U));

  auto first = pkgctl::reserve_next(run, dispatch_nonce(11U));
  CHECK(first.dispatch.has_value());
  journal = journal.successor(first.run);
  auto second = pkgctl::reserve_next(first.run, dispatch_nonce(12U));
  CHECK(second.dispatch.has_value());
  CHECK(first.dispatch && second.dispatch &&
        first.dispatch->identity() != second.dispatch->identity());
  journal = journal.successor(second.run);

  const auto package_for = [&](const pkgctl::transaction_dispatch& dispatch) {
    return dispatch.unit().primary_node() ==
            build_node_for(value.transaction, "tool").identity()
        ? std::string("tool")
        : std::string("dep");
  };

  auto first_session = construction_session_for(
      value.transaction, value.temporary.path() / "parallel-first",
      package_for(*first.dispatch));
  auto second_session = construction_session_for(
      value.transaction, value.temporary.path() / "parallel-second",
      package_for(*second.dispatch));

  auto first_started = pkgctl::start_construction_dispatch(
      second.run, *first.dispatch, first_session);
  journal = journal.successor(first_started);
  auto both_started = pkgctl::start_construction_dispatch(
      first_started, *second.dispatch, second_session);
  journal = journal.successor(both_started);

  const auto first_result = execute_build(first_session, value.payload);
  auto first_completed = pkgctl::complete_construction_dispatch(
      both_started, *first.dispatch, first_result);
  journal = journal.successor(first_completed);

  const auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
      first_completed.progress(), journal);
  CHECK(checkpoint.run().identity() == first_completed.identity());
  CHECK(checkpoint.assessment().active().size() == 1U);
  CHECK(checkpoint.assessment().active().front().dispatch() ==
        second.dispatch->identity());
  CHECK(checkpoint.assessment().active().front().disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::
            recover_construction);

  const auto second_result = execute_build(second_session, value.payload);
  auto complete = pkgctl::complete_construction_dispatch(
      first_completed, *second.dispatch, second_result);
  journal = journal.successor(complete);
  CHECK(pkgctl::transaction_run_restart_checkpoint::make(
            complete.progress(), journal).assessment().quiescent());
}

void check_release_transition()
{
  fixture value;
  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(4U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(5U));
  auto reserved = admitted.successor(reservation.run);
  auto released_run = pkgctl::release_unstarted_dispatch(
      reservation.run, *reservation.dispatch);
  auto released = reserved.successor(released_run);
  CHECK(released.dispatches().front().state() ==
        pkgctl::transaction_dispatch_state::released_unstarted);
  CHECK(pkgctl::transaction_run_restart_checkpoint::make(
            released_run.progress(), released).assessment().quiescent());
}

void check_codec_contract()
{
  fixture value;
  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(6U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(7U));
  auto reserved = admitted.successor(reservation.run);

  const auto first = pkgctl::encode_transaction_run_record(reserved);
  const auto second = pkgctl::encode_transaction_run_record(reserved);
  CHECK(first == second);
  const auto decoded = pkgctl::decode_transaction_run_record(first);
  CHECK(decoded.identity() == reserved.identity());
  CHECK(decoded.journal() == reserved.journal());
  CHECK(decoded.run() == reserved.run());
  CHECK(decoded.dispatches().front().identity() ==
        reserved.dispatches().front().identity());
  CHECK(pkgctl::encode_transaction_run_record(decoded) == first);

  // A self-consistent record cannot move dispatch ownership into admission.
  auto sequence_zero_with_dispatch = first;
  write_u64(sequence_zero_with_dispatch, 140U, 0U);
  sequence_zero_with_dispatch[148U] = 0U;
  sequence_zero_with_dispatch.erase(
      sequence_zero_with_dispatch.begin() + 149,
      sequence_zero_with_dispatch.begin() + 181);
  write_identity(
      sequence_zero_with_dispatch, 12U,
      record_identity_for(reserved, 0U, std::nullopt,
                          reserved.current_state()));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] {
        (void)pkgctl::decode_transaction_run_record(
            sequence_zero_with_dispatch);
      }));

  // A non-admission snapshot cannot exist before any reservation transition.
  auto positive_sequence_without_dispatch =
      pkgctl::encode_transaction_run_record(admitted);
  write_u64(positive_sequence_without_dispatch, 140U, 1U);
  positive_sequence_without_dispatch[148U] = 1U;
  positive_sequence_without_dispatch.insert(
      positive_sequence_without_dispatch.begin() + 149, 32U, 0U);
  write_identity(
      positive_sequence_without_dispatch, 149U, admitted.identity());
  write_identity(
      positive_sequence_without_dispatch, 12U,
      record_identity_for(admitted, 1U, admitted.identity(),
                          admitted.current_state()));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] {
        (void)pkgctl::decode_transaction_run_record(
            positive_sequence_without_dispatch);
      }));

  // Sequence is the exact transition count encoded by retained dispatch state.
  auto inflated_sequence = first;
  write_u64(inflated_sequence, 140U, 2U);
  write_identity(
      inflated_sequence, 12U,
      record_identity_for(reserved, 2U, admitted.identity(),
                          reserved.current_state()));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] {
        (void)pkgctl::decode_transaction_run_record(inflated_sequence);
      }));

  // Reservation authority must bind to the predecessor's exact state epoch.
  pkgstate::sha256_digest_bytes foreign_state_bytes{};
  foreign_state_bytes.back() = 0xa5U;
  const auto foreign_state =
      pkgstate::installed_state_snapshot_identity::from_sha256(
          foreign_state_bytes);
  auto detached_admission_encoding =
      pkgctl::encode_transaction_run_record(admitted);
  replace_current_state(detached_admission_encoding, 213U, foreign_state);
  write_identity(
      detached_admission_encoding, 12U,
      record_identity_for(admitted, 0U, std::nullopt, foreign_state));
  const auto detached_admission =
      pkgctl::decode_transaction_run_record(detached_admission_encoding);

  auto detached_reservation_encoding = first;
  write_identity(
      detached_reservation_encoding, 149U, detached_admission.identity());
  write_identity(
      detached_reservation_encoding, 12U,
      record_identity_for(reserved, 1U, detached_admission.identity(),
                          reserved.current_state()));
  const auto detached_reservation =
      pkgctl::decode_transaction_run_record(detached_reservation_encoding);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::invalid_transition,
      [&] {
        detached_reservation.validate_successor_of(detached_admission);
      }));

  for (std::size_t offset = 0; offset < first.size(); ++offset)
  {
    auto changed = first;
    changed[offset] ^= 0x01U;
    bool refused = false;
    try
    {
      (void)pkgctl::decode_transaction_run_record(changed);
    }
    catch (const pkgctl::transaction_run_journal_error&)
    {
      refused = true;
    }
    CHECK(refused);
  }

  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [] {
        (void)pkgctl::decode_transaction_run_record({});
      }));
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [] {
        pkgctl::transaction_run_encoding oversized(
            pkgctl::maximum_transaction_run_encoding_size + 1U, 0U);
        (void)pkgctl::decode_transaction_run_record(oversized);
      }));

  auto truncated = first;
  truncated.pop_back();
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] { (void)pkgctl::decode_transaction_run_record(truncated); }));

  auto trailing = first;
  trailing.push_back(0U);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] { (void)pkgctl::decode_transaction_run_record(trailing); }));

  auto bad_magic = first;
  bad_magic.front() ^= 0xffU;
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::corrupt_encoding,
      [&] { (void)pkgctl::decode_transaction_run_record(bad_magic); }));

  auto unsupported = first;
  unsupported[8] = 0U;
  unsupported[9] = 2U;
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::unsupported_encoding,
      [&] { (void)pkgctl::decode_transaction_run_record(unsupported); }));

  auto unsupported_schema = first;
  unsupported_schema[10] = 0U;
  unsupported_schema[11] = 2U;
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::unsupported_encoding,
      [&] {
        (void)pkgctl::decode_transaction_run_record(unsupported_schema);
      }));
}

void check_posix_store_contract()
{
  fixture value;
  const auto directory = value.temporary.path() / "run-journal";
  fs::create_directories(directory);
  auto store = pkgctl::posix_transaction_run_journal_store::open(
      directory.string());

  auto run = pkgctl::transaction_run::begin(
      value.progress,
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, run_nonce(8U));
  CHECK(!store.load_latest(admitted.journal()));
  CHECK(store.append(admitted).identity() == admitted.identity());
  const auto head_path = directory / journal_head_name(admitted);
  const auto admitted_head = read_bytes(head_path);
  CHECK(S_ISREG(file_mode(head_path)));
  CHECK((file_mode(head_path) & 0777) == 0444);
  CHECK((file_mode(directory / journal_record_name(admitted)) & 0777) == 0444);

  const auto lock_path = directory / ".pkgctl-run.lock";
  CHECK(fs::exists(lock_path));
  CHECK(::unlink(lock_path.c_str()) == 0);
  const auto read_only = store.load_latest(admitted.journal());
  CHECK(read_only && read_only->identity() == admitted.identity());
  CHECK(!fs::exists(lock_path));

  const int directory_fd = ::open(
      directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  CHECK(directory_fd >= 0);
  auto descriptor_store =
      pkgctl::posix_transaction_run_journal_store::from_directory_fd(
          directory_fd);
  CHECK(::close(directory_fd) == 0);
  CHECK(descriptor_store.load_latest(admitted.journal())->identity() ==
        admitted.identity());

  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(9U));
  auto reserved = admitted.successor(reservation.run);
  CHECK(store.append(reserved).identity() == reserved.identity());
  CHECK((file_mode(directory / journal_record_name(reserved)) & 0777) == 0444);

  replace_bytes(head_path, admitted_head);
  const auto rolled_back = store.load_latest(admitted.journal());
  CHECK(rolled_back.has_value());
  CHECK(rolled_back->identity() == admitted.identity());

  CHECK(store.append(reserved).identity() == reserved.identity());
  const auto recovered = store.load_latest(admitted.journal());
  CHECK(recovered.has_value());
  CHECK(recovered->identity() == reserved.identity());
  CHECK(recovered->reopen(reservation.run.progress()).identity() ==
        reservation.run.identity());
  CHECK(store.append(reserved).identity() == reserved.identity());

  auto released_run = pkgctl::release_unstarted_dispatch(
      reservation.run, *reservation.dispatch);
  auto released = reserved.successor(released_run);
  CHECK(store.append(released).identity() == released.identity());

  const auto package =
      reservation.dispatch->unit().primary_node() ==
              build_node_for(value.transaction, "tool").identity()
          ? std::string("tool")
          : std::string("dep");
  auto session = construction_session_for(
      value.transaction, value.temporary.path() / "fork-build", package);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto fork = reserved.successor(started_run);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_conflict,
      [&] { (void)store.append(fork); }));

  const auto filename = directory / journal_record_name(released);
  CHECK(::chmod(filename.c_str(), 0644) == 0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)store.load_latest(admitted.journal()); }));

  const auto gap_directory = value.temporary.path() / "run-journal-gap";
  fs::create_directories(gap_directory);
  auto gap_store = pkgctl::posix_transaction_run_journal_store::open(
      gap_directory.string());
  CHECK(gap_store.append(admitted).identity() == admitted.identity());
  CHECK(gap_store.append(reserved).identity() == reserved.identity());
  CHECK(::unlink((gap_directory / journal_record_name(admitted)).c_str()) == 0);
  const auto gap_latest = gap_store.load_latest(admitted.journal());
  CHECK(gap_latest && gap_latest->identity() == reserved.identity());

  const auto orphan_directory =
      value.temporary.path() / "run-journal-orphan-admission";
  fs::create_directories(orphan_directory);
  write_read_only(
      orphan_directory / journal_record_name(admitted),
      pkgctl::encode_transaction_run_record(admitted));
  auto orphan_store = pkgctl::posix_transaction_run_journal_store::open(
      orphan_directory.string());
  CHECK(orphan_store.append(admitted).identity() == admitted.identity());
  CHECK(orphan_store.load_latest(admitted.journal())->identity() ==
        admitted.identity());

  const auto damaged_head_directory =
      value.temporary.path() / "run-journal-damaged-head";
  fs::create_directories(damaged_head_directory);
  auto damaged_head_store =
      pkgctl::posix_transaction_run_journal_store::open(
          damaged_head_directory.string());
  CHECK(damaged_head_store.append(admitted).identity() ==
        admitted.identity());
  const auto damaged_head_path =
      damaged_head_directory / journal_head_name(admitted);
  const auto intact_head = read_bytes(damaged_head_path);
  for (std::size_t index = 0U; index < intact_head.size(); ++index)
  {
    auto damaged_head = intact_head;
    damaged_head[index] ^= 0x01;
    replace_bytes(damaged_head_path, damaged_head);
    CHECK(rejects(
        pkgctl::transaction_run_journal_error_code::store_corrupt,
        [&] {
          (void)damaged_head_store.load_latest(admitted.journal());
        }));
    replace_bytes(damaged_head_path, intact_head);
  }
  CHECK(::chmod(damaged_head_path.c_str(), 0644) == 0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)damaged_head_store.load_latest(admitted.journal()); }));
  CHECK(::chmod(damaged_head_path.c_str(), 0444) == 0);

  const auto concurrent_directory =
      value.temporary.path() / "run-journal-concurrent-retry";
  fs::create_directories(concurrent_directory);
  int ready_pipe[2]{};
  int release_pipe[2]{};
  CHECK(::pipe(ready_pipe) == 0);
  CHECK(::pipe(release_pipe) == 0);
  const pid_t child = ::fork();
  CHECK(child >= 0);
  if (child == 0)
  {
    (void)::close(ready_pipe[0]);
    (void)::close(release_pipe[1]);
    const char ready = 'R';
    if (::write(ready_pipe[1], &ready, 1U) != 1)
      ::_exit(2);
    char released = 0;
    if (::read(release_pipe[0], &released, 1U) != 1)
      ::_exit(3);
    try
    {
      auto child_store =
          pkgctl::posix_transaction_run_journal_store::open(
              concurrent_directory.string());
      (void)child_store.append(admitted);
      ::_exit(0);
    }
    catch (...)
    {
      ::_exit(4);
    }
  }
  if (child > 0)
  {
    (void)::close(ready_pipe[1]);
    (void)::close(release_pipe[0]);
    char ready = 0;
    CHECK(::read(ready_pipe[0], &ready, 1U) == 1);
    const char release = 'G';
    CHECK(::write(release_pipe[1], &release, 1U) == 1);
    auto parent_store = pkgctl::posix_transaction_run_journal_store::open(
        concurrent_directory.string());
    CHECK(parent_store.append(admitted).identity() == admitted.identity());
    int status = 0;
    CHECK(::waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    CHECK(parent_store.load_latest(admitted.journal())->identity() ==
          admitted.identity());
    (void)::close(ready_pipe[0]);
    (void)::close(release_pipe[1]);
  }

  const auto no_head_directory = value.temporary.path() / "run-journal-no-head";
  fs::create_directories(no_head_directory);
  auto no_head_store = pkgctl::posix_transaction_run_journal_store::open(
      no_head_directory.string());
  CHECK(no_head_store.append(admitted).identity() == admitted.identity());
  CHECK(::unlink(
            (no_head_directory / journal_head_name(admitted)).c_str()) == 0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)no_head_store.load_latest(admitted.journal()); }));

  const auto missing_tail_directory =
      value.temporary.path() / "run-journal-missing-tail";
  fs::create_directories(missing_tail_directory);
  auto missing_tail_store =
      pkgctl::posix_transaction_run_journal_store::open(
          missing_tail_directory.string());
  CHECK(missing_tail_store.append(admitted).identity() == admitted.identity());
  CHECK(missing_tail_store.append(reserved).identity() == reserved.identity());
  CHECK(::unlink(
            (missing_tail_directory / journal_record_name(reserved)).c_str()) ==
        0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)missing_tail_store.load_latest(admitted.journal()); }));

  const auto malformed_directory =
      value.temporary.path() / "run-journal-malformed";
  fs::create_directories(malformed_directory);
  auto malformed_store = pkgctl::posix_transaction_run_journal_store::open(
      malformed_directory.string());
  CHECK(malformed_store.append(admitted).identity() == admitted.identity());
  test_support::write(
      malformed_directory / (admitted.journal().hex() + "-garbage"),
      "garbage");
  CHECK(malformed_store.load_latest(admitted.journal())->identity() ==
        admitted.identity());

  const auto symlink_record_directory =
      value.temporary.path() / "run-journal-symlink-record";
  fs::create_directories(symlink_record_directory);
  auto symlink_record_store =
      pkgctl::posix_transaction_run_journal_store::open(
          symlink_record_directory.string());
  CHECK(symlink_record_store.append(admitted).identity() == admitted.identity());
  const auto record_path =
      symlink_record_directory / journal_record_name(admitted);
  const auto saved_record_path = symlink_record_directory / "saved-record";
  CHECK(::rename(record_path.c_str(), saved_record_path.c_str()) == 0);
  CHECK(::symlink(saved_record_path.filename().c_str(), record_path.c_str()) ==
        0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)symlink_record_store.load_latest(admitted.journal()); }));

  const auto symlink_head_directory =
      value.temporary.path() / "run-journal-symlink-head";
  fs::create_directories(symlink_head_directory);
  auto symlink_head_store =
      pkgctl::posix_transaction_run_journal_store::open(
          symlink_head_directory.string());
  CHECK(symlink_head_store.append(admitted).identity() == admitted.identity());
  const auto exact_head_path =
      symlink_head_directory / journal_head_name(admitted);
  const auto saved_head_path = symlink_head_directory / "saved-head";
  CHECK(::rename(exact_head_path.c_str(), saved_head_path.c_str()) == 0);
  CHECK(::symlink(
            saved_head_path.filename().c_str(), exact_head_path.c_str()) == 0);
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_corrupt,
      [&] { (void)symlink_head_store.load_latest(admitted.journal()); }));

  const auto invalid_lock_directory =
      value.temporary.path() / "run-journal-invalid-lock";
  fs::create_directories(invalid_lock_directory / ".pkgctl-run.lock");
  auto invalid_lock_store =
      pkgctl::posix_transaction_run_journal_store::open(
          invalid_lock_directory.string());
  CHECK(rejects(
      pkgctl::transaction_run_journal_error_code::store_open_failed,
      [&] { (void)invalid_lock_store.load_latest(admitted.journal()); }));
}

} // namespace

int main()
{
  check_read_only_run_inspection();
  check_nonce_contract();
  check_journal_transition_and_reopen();
  check_parallel_restart_rehydration();
  check_release_transition();
  check_codec_contract();
  check_posix_store_contract();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
