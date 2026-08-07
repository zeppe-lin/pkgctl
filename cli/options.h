// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <libpkgexec/model.h>
#include <libpkgstate/installed_package.h>

#include <pkgctl/identity.h>
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

struct installed_tree_option final {
  pkgstate::installed_package_identity package;
  pkgexec::resource_identity resource;
  std::filesystem::path path;
};

struct transaction_run_command final {
  transaction_request transaction;
  transaction_run_command_intent intent;
  transaction_run_nonce nonce;
  std::filesystem::path runtime_root;
  std::filesystem::path build_root;
  std::filesystem::path lifecycle_root;
  std::filesystem::path target_root;
  std::filesystem::path interpreter;
  pkgexec::credential_policy build_credentials;
  pkgexec::credential_policy lifecycle_credentials;
  std::uint64_t source_date_epoch;
  std::size_t maximum_steps;
  std::vector<installed_tree_option> installed_trees;
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
