// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_inspect.h>
#include <pkgctl/report.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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

pkgctl::effect_attempt_nonce nonce(std::uint8_t marker)
{
  pkgctl::effect_attempt_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::effect_attempt_nonce::from_bytes(bytes);
}

class inspection_store final : public pkgctl::effect_journal_store {
public:
  explicit inspection_store(
      std::optional<pkgctl::effect_attempt_record> record)
      : record_(std::move(record))
  {
  }

  std::optional<pkgctl::effect_attempt_record> load_latest(
      const pkgctl::session_identity& attempt) const override
  {
    ++load_calls_;
    requested_attempt_ = attempt;
    return record_;
  }

  pkgctl::effect_attempt_record append(
      const pkgctl::effect_attempt_record&) override
  {
    ++append_calls_;
    throw std::runtime_error("inspection store append must not be called");
  }

  std::size_t load_calls() const noexcept { return load_calls_; }
  std::size_t append_calls() const noexcept { return append_calls_; }
  const std::optional<pkgctl::session_identity>& requested_attempt() const noexcept
  {
    return requested_attempt_;
  }

private:
  std::optional<pkgctl::effect_attempt_record> record_;
  mutable std::size_t load_calls_ = 0U;
  mutable std::optional<pkgctl::session_identity> requested_attempt_;
  std::size_t append_calls_ = 0U;
};

void check_read_only_effect_inspection()
{
  const auto session = pkgctl::make_session_identity(
      "pkgctl/test-effect-inspection-session/1", {"fixture"});
  const auto admitted = pkgctl::effect_attempt_record::admit(
      session, 0U, 0U, nonce(1U));

  inspection_store missing(std::nullopt);
  CHECK(rejects(
      pkgctl::effect_journal_error_code::store_conflict,
      [&] { (void)pkgctl::inspect_effect_attempt(admitted.attempt(), missing); }));
  CHECK(missing.load_calls() == 1U);
  CHECK(missing.requested_attempt() &&
        *missing.requested_attempt() == admitted.attempt());
  CHECK(missing.append_calls() == 0U);

  inspection_store initial(admitted);
  const auto initial_view =
      pkgctl::inspect_effect_attempt(admitted.attempt(), initial);
  CHECK(initial_view.record().identity() == admitted.identity());
  CHECK(initial_view.assessment().attempt() == admitted.attempt());
  CHECK(initial_view.assessment().record() == admitted.identity());
  CHECK(initial_view.assessment().stage() == admitted.stage());
  CHECK(initial_view.assessment().disposition() ==
        pkgctl::effect_restart_disposition::start_application);
  CHECK(!initial_view.terminal());
  CHECK(initial_view.automatically_continuable());
  CHECK(!initial_view.external_resolution_required());
  CHECK(initial.load_calls() == 1U);
  CHECK(initial.requested_attempt() &&
        *initial.requested_attempt() == admitted.attempt());
  CHECK(initial.append_calls() == 0U);

  const std::string initial_report =
      "session.kind=effect-attempt\n"
      "effect.attempt=" + admitted.attempt().hex() + "\n"
      "effect.record=" + admitted.identity().hex() + "\n"
      "effect.session=" + admitted.session().hex() + "\n"
      "effect.nonce=" + admitted.nonce().hex() + "\n"
      "effect.sequence=0\n"
      "effect.stage=admitted\n"
      "effect.disposition=start-application\n"
      "effect.terminal=false\n"
      "effect.automatically-continuable=true\n"
      "effect.external-resolution-required=false\n"
      "effect.before-total=0\n"
      "effect.before-completed=0\n"
      "effect.after-total=0\n"
      "effect.after-completed=0\n";
  CHECK(initial_report.find("effect.previous=") == std::string::npos);
  CHECK(initial_report.find("effect.active-index=") == std::string::npos);
  CHECK(pkgctl::render_report(initial_view) == initial_report);
  CHECK(pkgctl::render_report(initial_view) ==
        pkgctl::render_report(initial_view));

  const auto with_before = pkgctl::effect_attempt_record::admit(
      session, 1U, 0U, nonce(2U));
  const auto intent = with_before.begin_before(0U);
  inspection_store intent_store(intent);
  const auto intent_view =
      pkgctl::inspect_effect_attempt(intent.attempt(), intent_store);
  CHECK(!intent_view.terminal());
  CHECK(!intent_view.automatically_continuable());
  CHECK(intent_view.external_resolution_required());
  CHECK(intent_view.assessment().disposition() ==
        pkgctl::effect_restart_disposition::external_resolution_required);
  const auto intent_report = pkgctl::render_report(intent_view);
  CHECK(intent_report.find("effect.previous=" +
                           with_before.identity().hex() + "\n") !=
        std::string::npos);
  CHECK(intent_report.find("effect.stage=before-lifecycle-intent\n") !=
        std::string::npos);
  CHECK(intent_report.find(
            "effect.disposition=external-resolution-required\n") !=
        std::string::npos);
  CHECK(intent_report.find("effect.active-index=0\n") != std::string::npos);

  const auto terminal = admitted.seal_terminal(
      pkgctl::effectful_operation_outcome::outer_lease_lost);
  inspection_store terminal_store(terminal);
  const auto terminal_view =
      pkgctl::inspect_effect_attempt(terminal.attempt(), terminal_store);
  CHECK(terminal_view.terminal());
  CHECK(terminal_view.automatically_continuable());
  CHECK(!terminal_view.external_resolution_required());
  CHECK(terminal_view.assessment().disposition() ==
        pkgctl::effect_restart_disposition::terminal);
  const auto terminal_report = pkgctl::render_report(terminal_view);
  CHECK(terminal_report.find("effect.stage=terminal\n") != std::string::npos);
  CHECK(terminal_report.find("effect.disposition=terminal\n") !=
        std::string::npos);
  CHECK(terminal_report.find("effect.terminal=true\n") != std::string::npos);
  CHECK(terminal_report.find("effect.terminal-outcome=outer-lease-lost\n") !=
        std::string::npos);

  const auto foreign = pkgctl::effect_attempt_record::admit(
      pkgctl::make_session_identity(
          "pkgctl/test-effect-inspection-session/1", {"foreign"}),
      0U, 0U, nonce(3U));
  inspection_store foreign_store(foreign);
  CHECK(rejects(
      pkgctl::effect_journal_error_code::store_contract_violation,
      [&] {
        (void)pkgctl::inspect_effect_attempt(
            admitted.attempt(), foreign_store);
      }));
  CHECK(foreign_store.load_calls() == 1U);
  CHECK(foreign_store.append_calls() == 0U);
}

} // namespace

int main()
{
  check_read_only_effect_inspection();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
