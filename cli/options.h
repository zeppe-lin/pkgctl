// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <libpkgbuild/request.h>
#include <libpkgexec/model.h>
#include <libpkgstate/installed_package.h>

#include <pkgctl/identity.h>
#include <pkgctl/native_policy.h>
#include <pkgctl/request.h>
#include <pkgctl/run_journal.h>

namespace pkgctl::cli {

struct run_inspection_command final {
  std::string store;
  session_identity journal;
};

struct effect_inspection_command final {
  std::string store;
  session_identity attempt;
};

enum class transaction_run_command_intent {
  start,
  resume,
};

enum class transaction_run_command_frontend {
  run,
  build,
};

struct transaction_run_command final {
  transaction_run_command_frontend frontend;
  std::optional<transaction_request> transaction;
  std::filesystem::path canonical_store;
  transaction_run_command_intent intent;
  transaction_run_nonce nonce;
  std::filesystem::path runtime_root;
  std::filesystem::path build_root;
  std::optional<pkgexec::root_view_identity> build_root_view;
  std::filesystem::path artifact_root;
  std::optional<std::filesystem::path> lifecycle_root;
  std::optional<pkgexec::root_view_identity> lifecycle_root_view;
  std::optional<std::filesystem::path> target_root;
  std::filesystem::path interpreter;
  pkgexec::credential_policy build_credentials;
  std::optional<pkgexec::credential_policy> lifecycle_credentials;
  std::optional<pkgbuild::build_policy> build_policy;
  std::optional<native_operation_policy> operation_policy;
  std::size_t maximum_steps;
  std::optional<std::filesystem::path> package_object_store;
};

using command = std::variant<catalog_request,
                             resolution_request,
                             transaction_request,
                             transaction_run_command,
                             run_inspection_command,
                             effect_inspection_command>;

class usage_error final : public std::invalid_argument {
public:
  explicit usage_error(std::string message);
};

[[nodiscard]] command parse_command(int argc, char** argv);
[[nodiscard]] std::string help_text();

} // namespace pkgctl::cli
