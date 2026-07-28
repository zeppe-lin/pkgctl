// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_support.h"

#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

pkgctl::effect_attempt_nonce nonce(unsigned char marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
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

  const auto terminal = admitted.seal_terminal(
      pkgctl::effectful_operation_outcome::outer_lease_lost);
  CHECK(terminal.stage() == pkgctl::effect_attempt_stage::terminal);
  CHECK(terminal.sequence() == 1U);
  CHECK(terminal.previous() && *terminal.previous() == admitted.identity());
  CHECK(terminal.terminal_outcome() &&
        *terminal.terminal_outcome() ==
            pkgctl::effectful_operation_outcome::outer_lease_lost);
  const auto assessment = pkgctl::assess_effect_restart(terminal);
  CHECK(assessment.disposition() ==
        pkgctl::effect_restart_disposition::terminal);
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

  CHECK(store.append(admitted).identity() == admitted.identity());
  CHECK(store.append(terminal).identity() == terminal.identity());
  const auto latest = store.load_latest(admitted.attempt());
  CHECK(latest && latest->identity() == terminal.identity());

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
