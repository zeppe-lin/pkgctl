// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file outcome.h
 *  \brief Orchestrator-native step outcomes.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pkgctl {

/*! \brief One orchestration phase, independent of a concrete backend. */
enum class step_kind : std::uint8_t {
  source_refresh = 1,
  source_inspection = 2,
  selection = 3,
  build = 4,
  image_inspection = 5,
  planning = 6,
  application = 7,
  state_projection = 8,
  state_publication = 9,
  lifecycle = 10,
  recovery = 11,
};

/*! \brief Terminal orchestration status of one step. */
enum class outcome_state : std::uint8_t {
  succeeded = 1,
  refused = 2,
  failed = 3,
  skipped = 4,
};

/*! \brief Stable failure domain without backend-specific exit lore. */
enum class failure_domain : std::uint8_t {
  invalid_input = 1,
  unavailable_authority = 2,
  stale_authority = 3,
  policy = 4,
  external_process = 5,
  filesystem = 6,
  integrity = 7,
  recovery_required = 8,
  internal = 9,
};

/*! \brief Reason a step was intentionally not attempted. */
enum class skip_reason : std::uint8_t {
  prerequisite_failed = 1,
  excluded_by_policy = 2,
  not_applicable = 3,
};

[[nodiscard]] std::string_view to_string(step_kind value) noexcept;
[[nodiscard]] std::string_view to_string(outcome_state value) noexcept;
[[nodiscard]] std::string_view to_string(failure_domain value) noexcept;
[[nodiscard]] std::string_view to_string(skip_reason value) noexcept;

/*! \brief Immutable result of one orchestrator phase. */
class step_outcome final {
public:
  [[nodiscard]] static step_outcome succeeded(step_kind step);
  [[nodiscard]] static step_outcome refused(step_kind step,
                                             failure_domain domain,
                                             std::string diagnostic);
  [[nodiscard]] static step_outcome failed(step_kind step,
                                            failure_domain domain,
                                            std::string diagnostic);
  [[nodiscard]] static step_outcome skipped(step_kind step,
                                             skip_reason reason,
                                             std::string diagnostic);

  [[nodiscard]] step_kind step() const noexcept;
  [[nodiscard]] outcome_state state() const noexcept;
  [[nodiscard]] const std::optional<failure_domain>& failure() const noexcept;
  [[nodiscard]] const std::optional<skip_reason>& skip() const noexcept;
  [[nodiscard]] const std::string& diagnostic() const noexcept;

private:
  step_outcome(step_kind step,
               outcome_state state,
               std::optional<failure_domain> failure,
               std::optional<skip_reason> skip,
               std::string diagnostic);

  step_kind step_;
  outcome_state state_;
  std::optional<failure_domain> failure_;
  std::optional<skip_reason> skip_;
  std::string diagnostic_;
};

} // namespace pkgctl
