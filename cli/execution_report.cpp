// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "execution_report.h"

#include <ostream>

namespace pkgctl::cli {

void render_execution_classification(
    std::ostream& output,
    std::string_view stage,
    const std::optional<pkgexec::execution_failure_kind>& failure,
    const std::optional<pkgexec::process_termination>& termination)
{
  if (!failure)
    return;

  output << "pkgctl: " << stage << " execution: failure="
         << pkgexec::to_string(*failure);

  if (termination)
  {
    output << " termination=" << pkgexec::to_string(termination->kind());
    if (termination->value())
    {
      switch (termination->kind())
      {
        case pkgexec::process_termination_kind::exited:
          output << " status=" << *termination->value();
          break;
        case pkgexec::process_termination_kind::signaled:
          output << " signal=" << *termination->value();
          break;
        case pkgexec::process_termination_kind::cancelled:
        case pkgexec::process_termination_kind::resource_limited:
          output << " value=" << *termination->value();
          break;
      }
    }
    if (termination->limit())
      output << " limit=" << pkgexec::to_string(*termination->limit());
  }

  output << '\n';
}

} // namespace pkgctl::cli
