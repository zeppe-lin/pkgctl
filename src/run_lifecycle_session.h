// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>

#include <pkgctl/dispatch.h>
#include <pkgctl/identity.h>
#include <pkgctl/run_journal.h>

namespace pkgctl::detail {

[[nodiscard]] inline std::filesystem::path native_lifecycle_session_path(
    const std::filesystem::path& session_root,
    const transaction_run_journal_record& record,
    const transaction_dispatch& dispatch,
    const pkgtransaction::transaction_node_identity& lifecycle_node)
{
  return session_root /
      make_session_identity(
          "pkgctl/native-lifecycle-session-root/1",
          {record.journal().hex(), dispatch.identity().hex(),
           lifecycle_node.hex()})
          .hex();
}

} // namespace pkgctl::detail
