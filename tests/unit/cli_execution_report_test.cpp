// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../cli/execution_report.h"

#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>

namespace {

#define CHECK(condition) \
  do \
  { \
    if (!(condition)) \
      std::abort(); \
  } while (false)

std::string render(
    pkgexec::execution_failure_kind failure,
    std::optional<pkgexec::process_termination> termination)
{
  std::ostringstream output;
  pkgctl::cli::render_execution_classification(
      output, "check", failure, termination);
  return output.str();
}

} // namespace

int main()
{
  CHECK(render(
            pkgexec::execution_failure_kind::program_exited_nonzero,
            pkgexec::process_termination::exited(7)) ==
        "pkgctl: check execution: failure=program-exited-nonzero "
        "termination=exited status=7\n");

  CHECK(render(
            pkgexec::execution_failure_kind::program_terminated_by_signal,
            pkgexec::process_termination::signaled(9)) ==
        "pkgctl: check execution: failure=program-terminated-by-signal "
        "termination=signaled signal=9\n");

  CHECK(render(
            pkgexec::execution_failure_kind::resource_limit_exceeded,
            pkgexec::process_termination::resource_limited(
                pkgexec::resource_limit_kind::cpu_time)) ==
        "pkgctl: check execution: failure=resource-limit-exceeded "
        "termination=resource-limited limit=cpu-time\n");

  CHECK(render(
            pkgexec::execution_failure_kind::backend_unsupported,
            std::nullopt) ==
        "pkgctl: check execution: failure=backend-unsupported\n");

  std::ostringstream no_failure;
  pkgctl::cli::render_execution_classification(
      no_failure, "check", std::nullopt, std::nullopt);
  CHECK(no_failure.str().empty());

  return 0;
}
