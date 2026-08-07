// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/evp.h>

namespace {

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

template<typename Function>
bool rejects(pkgctl::effect_journal_error_code expected, Function&& function)
{
  try
  {
    function();
  }
  catch (const pkgctl::effect_journal_error& problem)
  {
    return problem.code() == expected;
  }
  return false;
}

pkgctl::effect_attempt_nonce nonce(unsigned char marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
}

std::string record_name(
    const pkgctl::session_identity& attempt,
    std::uint64_t value,
    const pkgctl::session_identity& identity)
{
  std::ostringstream sequence;
  sequence << std::setw(20) << std::setfill('0') << value;
  return attempt.hex() + "-" + sequence.str() + "-" + identity.hex() +
      ".pje";
}

std::string record_name(const pkgctl::effect_attempt_record& record)
{
  return record_name(record.attempt(), record.sequence(), record.identity());
}

std::string head_name(const pkgctl::effect_attempt_record& record)
{
  return record.attempt().hex() + ".pjeh";
}

std::array<std::uint8_t, 32> sha256(
    const std::uint8_t* data, std::size_t size)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  std::array<std::uint8_t, 32> result{};
  unsigned int length = 0U;
  if (!context ||
      EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (size != 0U &&
       EVP_DigestUpdate(context.get(), data, size) != 1) ||
      EVP_DigestFinal_ex(context.get(), result.data(), &length) != 1 ||
      length != result.size())
    throw std::runtime_error("cannot hash effect-journal fixture");
  return result;
}

pkgctl::effect_attempt_encoding refresh_checksum(
    pkgctl::effect_attempt_encoding encoding)
{
  const auto payload_size = encoding.size() - 32U;
  const auto checksum = sha256(encoding.data(), payload_size);
  std::copy(checksum.begin(), checksum.end(),
            encoding.begin() + static_cast<std::ptrdiff_t>(payload_size));
  return encoding;
}

std::string bool_text(bool value)
{
  return value ? "1" : "0";
}

pkgctl::session_identity record_identity_for(
    const pkgctl::effect_attempt_record& record,
    std::uint64_t sequence,
    const std::optional<pkgctl::session_identity>& previous)
{
  std::vector<std::string> fields{
      record.attempt().hex(),
      record.session().hex(),
      record.nonce().hex(),
      std::to_string(sequence),
      previous ? previous->hex() : std::string{},
      std::to_string(record.before_total()),
      std::to_string(record.after_total()),
      std::to_string(static_cast<unsigned int>(record.stage())),
      record.active_index() ? std::to_string(*record.active_index())
                            : std::string{},
      std::to_string(record.before().size()),
  };
  for (const auto& fact : record.before())
  {
    fields.push_back(fact.result().hex());
    fields.push_back(bool_text(fact.succeeded()));
  }
  fields.push_back(
      record.application() ? record.application()->receipt() : std::string{});
  fields.push_back(record.application()
      ? std::to_string(static_cast<unsigned int>(
            record.application()->outcome()))
      : std::string{});
  fields.push_back(record.application() && record.application()->journal()
      ? *record.application()->journal() : std::string{});
  fields.push_back(
      record.application() && record.application()->completed_evidence()
          ? *record.application()->completed_evidence() : std::string{});
  fields.push_back(std::to_string(record.after().size()));
  for (const auto& fact : record.after())
  {
    fields.push_back(fact.result().hex());
    fields.push_back(bool_text(fact.succeeded()));
  }
  fields.push_back(record.transaction_evidence().value_or(std::string{}));
  fields.push_back(record.publication_request().value_or(std::string{}));
  fields.push_back(
      record.publication() ? record.publication()->receipt() : std::string{});
  fields.push_back(record.publication()
      ? std::to_string(static_cast<unsigned int>(
            record.publication()->outcome()))
      : std::string{});
  fields.push_back(record.publication() &&
                           record.publication()->resulting_snapshot()
      ? *record.publication()->resulting_snapshot() : std::string{});
  fields.push_back(record.terminal_outcome()
      ? std::to_string(static_cast<unsigned int>(*record.terminal_outcome()))
      : std::string{});
  fields.push_back(record.reconciled_state().value_or(std::string{}));
  return pkgctl::make_session_identity(
      "pkgctl/effect-attempt-record/1", fields);
}

void write_u64(
    pkgctl::effect_attempt_encoding& encoding,
    std::size_t offset,
    std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    encoding[offset++] = static_cast<std::uint8_t>(
        (value >> static_cast<unsigned int>(shift)) & 0xffU);
}

void write_hex_text(
    pkgctl::effect_attempt_encoding& encoding,
    std::size_t offset,
    const pkgctl::session_identity& identity)
{
  const auto hex = identity.hex();
  std::copy(hex.begin(), hex.end(),
            encoding.begin() + static_cast<std::ptrdiff_t>(offset));
}

struct forged_effect_record final {
  pkgctl::effect_attempt_encoding encoding;
  std::uint64_t sequence;
  pkgctl::session_identity identity;
};

forged_effect_record forge_terminal_successor(
    const pkgctl::effect_attempt_record& terminal)
{
  auto encoding = pkgctl::encode_effect_attempt_record(terminal);
  const std::uint64_t sequence = terminal.sequence() + 1U;
  const auto identity =
      record_identity_for(terminal, sequence, terminal.identity());
  write_u64(encoding, 260U, sequence);
  write_hex_text(encoding, 277U, terminal.identity());
  write_hex_text(encoding, 20U, identity);
  return {refresh_checksum(std::move(encoding)), sequence, identity};
}

void write_read_only(
    const std::filesystem::path& path,
    const pkgctl::effect_attempt_encoding& encoding)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("cannot write effect-journal fixture");
  output.write(
      reinterpret_cast<const char*>(encoding.data()),
      static_cast<std::streamsize>(encoding.size()));
  output.close();
  if (!output || ::chmod(path.c_str(), 0444) != 0)
    throw std::runtime_error("cannot seal effect-journal fixture");
}

std::vector<char> read_bytes(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read effect-journal fixture");
  return std::vector<char>(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

void replace_bytes(
    const std::filesystem::path& path,
    const std::vector<char>& bytes)
{
  if (::chmod(path.c_str(), 0644) != 0)
    throw std::runtime_error("cannot make effect-journal fixture writable");
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("cannot replace effect-journal fixture");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output || ::chmod(path.c_str(), 0444) != 0)
    throw std::runtime_error("cannot reseal effect-journal fixture");
}

void check_model_and_codec()
{
  const auto session = pkgctl::make_session_identity(
      "pkgctl/test-effect-session/1", {"fixture"});
  const auto admitted = pkgctl::effect_attempt_record::admit(
      session, 2U, 1U, nonce(1));
  CHECK(admitted.sequence() == 0U);
  CHECK(admitted.stage() == pkgctl::effect_attempt_stage::admitted);
  CHECK(admitted.session() == session);
  CHECK(!admitted.previous());

  const auto encoding = pkgctl::encode_effect_attempt_record(admitted);
  CHECK(encoding[8] == 0U);
  CHECK(encoding[9] == pkgctl::effect_attempt_encoding_version);
  const auto decoded = pkgctl::decode_effect_attempt_record(encoding);
  CHECK(decoded.identity() == admitted.identity());
  CHECK(decoded.attempt() == admitted.attempt());
  CHECK(decoded.nonce() == admitted.nonce());
  CHECK(decoded.before_total() == 2U);
  CHECK(decoded.after_total() == 1U);
  auto damaged = encoding;
  damaged[damaged.size() / 2U] ^= 0x80U;
  bool rejected = false;
  try
  {
    (void)pkgctl::decode_effect_attempt_record(damaged);
  }
  catch (const pkgctl::effect_journal_error& value)
  {
    rejected = value.code() ==
        pkgctl::effect_journal_error_code::corrupt_encoding;
  }
  CHECK(rejected);

  const auto payload_size = encoding.size() - 32U;
  for (std::size_t index = 0U; index < payload_size; ++index)
  {
    auto structurally_damaged = encoding;
    structurally_damaged[index] ^= 0x01U;
    structurally_damaged = refresh_checksum(
        std::move(structurally_damaged));
    bool typed_refusal = false;
    try
    {
      (void)pkgctl::decode_effect_attempt_record(structurally_damaged);
    }
    catch (const pkgctl::effect_journal_error&)
    {
      typed_refusal = true;
    }
    CHECK(typed_refusal);
  }

  const auto terminal = admitted.seal_terminal(
      pkgctl::effectful_operation_outcome::outer_lease_lost);
  CHECK(terminal.stage() == pkgctl::effect_attempt_stage::terminal);
  CHECK(terminal.sequence() == 1U);
  CHECK(terminal.previous() && *terminal.previous() == admitted.identity());
  terminal.validate_successor_of(admitted);
  CHECK(terminal.terminal_outcome() &&
        *terminal.terminal_outcome() ==
            pkgctl::effectful_operation_outcome::outer_lease_lost);
  const auto assessment = pkgctl::assess_effect_restart(terminal);
  CHECK(assessment.disposition() ==
        pkgctl::effect_restart_disposition::terminal);

  const auto forged_terminal = forge_terminal_successor(terminal);
  CHECK(rejects(
      pkgctl::effect_journal_error_code::corrupt_encoding,
      [&] {
        (void)pkgctl::decode_effect_attempt_record(forged_terminal.encoding);
      }));
  CHECK(rejects(
      pkgctl::effect_journal_error_code::invalid_transition,
      [&] { admitted.validate_successor_of(terminal); }));
}

void check_store()
{
  test_support::temporary_directory directory;
  auto store = pkgctl::posix_effect_journal_store::open(
      directory.path().string());
  const auto session = pkgctl::make_session_identity(
      "pkgctl/test-effect-session/1", {"store"});
  const auto admitted = pkgctl::effect_attempt_record::admit(
      session, 0U, 0U, nonce(2));
  const auto terminal = admitted.seal_terminal(
      pkgctl::effectful_operation_outcome::outer_lease_lost);

  const auto lock_path = directory.path() / ".pkgctl-effect.lock";
  CHECK(!store.load_latest(admitted.attempt()));
  CHECK(!std::filesystem::exists(lock_path));

  CHECK(store.append(admitted).identity() == admitted.identity());
  CHECK(std::filesystem::exists(lock_path));
  CHECK(store.append(terminal).identity() == terminal.identity());
  CHECK(store.append(terminal).identity() == terminal.identity());
  CHECK(::unlink(lock_path.c_str()) == 0);
  const auto latest = store.load_latest(admitted.attempt());
  CHECK(latest && latest->identity() == terminal.identity());
  CHECK(!std::filesystem::exists(lock_path));
  const auto head = directory.path() / head_name(admitted);
  CHECK(std::filesystem::is_regular_file(head));
  CHECK((std::filesystem::status(head).permissions() &
         std::filesystem::perms::owner_write) ==
        std::filesystem::perms::none);

  std::size_t snapshots = 0U;
  for (const auto& entry : std::filesystem::directory_iterator(directory.path()))
  {
    if (entry.path().extension() == ".pje")
    {
      ++snapshots;
      const auto permissions = entry.status().permissions();
      CHECK((permissions & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none);
    }
  }
  CHECK(snapshots == 2U);

  CHECK(::unlink(
            (directory.path() / record_name(admitted)).c_str()) == 0);
  CHECK(store.load_latest(admitted.attempt())->identity() ==
        terminal.identity());

  bool conflict = false;
  try
  {
    (void)store.append(admitted.seal_terminal(
        pkgctl::effectful_operation_outcome::application_not_completed));
  }
  catch (const pkgctl::effect_journal_error& value)
  {
    conflict = value.code() == pkgctl::effect_journal_error_code::store_conflict ||
               value.code() ==
                   pkgctl::effect_journal_error_code::invalid_transition;
  }
  CHECK(conflict);

  {
    test_support::temporary_directory orphan_directory;
    auto orphan_store = pkgctl::posix_effect_journal_store::open(
        orphan_directory.path().string());
    const auto orphan = pkgctl::effect_attempt_record::admit(
        pkgctl::make_session_identity(
            "pkgctl/test-effect-session/1", {"orphan-admission"}),
        0U, 0U, nonce(3));
    write_read_only(
        orphan_directory.path() / record_name(orphan),
        pkgctl::encode_effect_attempt_record(orphan));
    CHECK(orphan_store.append(orphan).identity() == orphan.identity());
    CHECK(orphan_store.load_latest(orphan.attempt())->identity() ==
          orphan.identity());
  }

  {
    test_support::temporary_directory interrupted_directory;
    auto interrupted_store = pkgctl::posix_effect_journal_store::open(
        interrupted_directory.path().string());
    const auto first = pkgctl::effect_attempt_record::admit(
        pkgctl::make_session_identity(
            "pkgctl/test-effect-session/1", {"interrupted-successor"}),
        0U, 0U, nonce(4));
    const auto last = first.seal_terminal(
        pkgctl::effectful_operation_outcome::outer_lease_lost);
    CHECK(interrupted_store.append(first).identity() == first.identity());
    write_read_only(
        interrupted_directory.path() / record_name(last),
        pkgctl::encode_effect_attempt_record(last));
    CHECK(interrupted_store.append(last).identity() == last.identity());
    CHECK(interrupted_store.load_latest(first.attempt())->identity() ==
          last.identity());
  }

  {
    test_support::temporary_directory damaged_head_directory;
    auto damaged_head_store = pkgctl::posix_effect_journal_store::open(
        damaged_head_directory.path().string());
    const auto value = pkgctl::effect_attempt_record::admit(
        pkgctl::make_session_identity(
            "pkgctl/test-effect-session/1", {"damaged-head"}),
        0U, 0U, nonce(5));
    CHECK(damaged_head_store.append(value).identity() == value.identity());
    const auto head_path = damaged_head_directory.path() / head_name(value);
    const auto intact_head = read_bytes(head_path);
    for (std::size_t index = 0U; index < intact_head.size(); ++index)
    {
      auto damaged_head = intact_head;
      damaged_head[index] ^= 0x01;
      replace_bytes(head_path, damaged_head);
      CHECK(rejects(
          pkgctl::effect_journal_error_code::store_corrupt,
          [&] { (void)damaged_head_store.load_latest(value.attempt()); }));
      replace_bytes(head_path, intact_head);
    }
    CHECK(::chmod(head_path.c_str(), 0644) == 0);
    CHECK(rejects(
        pkgctl::effect_journal_error_code::store_corrupt,
        [&] { (void)damaged_head_store.load_latest(value.attempt()); }));
    CHECK(::chmod(head_path.c_str(), 0444) == 0);
  }

  {
    test_support::temporary_directory missing_head_directory;
    auto missing_head_store = pkgctl::posix_effect_journal_store::open(
        missing_head_directory.path().string());
    const auto value = pkgctl::effect_attempt_record::admit(
        pkgctl::make_session_identity(
            "pkgctl/test-effect-session/1", {"missing-head"}),
        0U, 0U, nonce(8));
    CHECK(missing_head_store.append(value).identity() == value.identity());
    CHECK(::unlink(
              (missing_head_directory.path() / head_name(value)).c_str()) ==
          0);
    bool corrupt = false;
    try
    {
      (void)missing_head_store.load_latest(value.attempt());
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      corrupt = problem.code() ==
          pkgctl::effect_journal_error_code::store_corrupt;
    }
    CHECK(corrupt);
  }

  {
    test_support::temporary_directory missing_tail_directory;
    auto missing_tail_store = pkgctl::posix_effect_journal_store::open(
        missing_tail_directory.path().string());
    const auto first = pkgctl::effect_attempt_record::admit(
        pkgctl::make_session_identity(
            "pkgctl/test-effect-session/1", {"missing-tail"}),
        0U, 0U, nonce(9));
    const auto last = first.seal_terminal(
        pkgctl::effectful_operation_outcome::outer_lease_lost);
    CHECK(missing_tail_store.append(first).identity() == first.identity());
    CHECK(missing_tail_store.append(last).identity() == last.identity());
    CHECK(::unlink(
              (missing_tail_directory.path() / record_name(last)).c_str()) ==
          0);
    bool corrupt = false;
    try
    {
      (void)missing_tail_store.load_latest(first.attempt());
    }
    catch (const pkgctl::effect_journal_error& problem)
    {
      corrupt = problem.code() ==
          pkgctl::effect_journal_error_code::store_corrupt;
    }
    CHECK(corrupt);
  }

  {
    test_support::temporary_directory invalid_lock_directory;
    std::filesystem::create_directory(
        invalid_lock_directory.path() / ".pkgctl-effect.lock");
    auto invalid_lock_store = pkgctl::posix_effect_journal_store::open(
        invalid_lock_directory.path().string());
    CHECK(rejects(
        pkgctl::effect_journal_error_code::store_open_failed,
        [&] { (void)invalid_lock_store.load_latest(admitted.attempt()); }));
  }
}

void check_nonce_refusal()
{
  bool rejected = false;
  try
  {
    (void)pkgctl::effect_attempt_nonce::from_bytes({});
  }
  catch (const pkgctl::effect_journal_error& value)
  {
    rejected = value.code() == pkgctl::effect_journal_error_code::invalid_nonce;
  }
  CHECK(rejected);
}

} // namespace

int main()
{
  check_nonce_refusal();
  check_model_and_codec();
  check_store();
  return failures == 0 ? 0 : 1;
}
