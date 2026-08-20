// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_locator.h
 *  \brief Deterministic native construction and check session realization.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <pkgctl/run_authority.h>

namespace pkgctl {

/*! \brief One retained installed package tree supplied by its storage owner. */
struct retained_installed_package_tree final {
  pkgstate::installed_package_identity package;
  pkgexec::resource_identity resource;
  std::filesystem::path path;
};

/*! \brief Caller-owned retained resource for one exact installed package.
 *
 * Implementations reproduce an already admitted identity/path mapping. They
 * must not discover package authority by scanning or inspecting the host.
 */
class retained_installed_package_tree_source {
public:
  virtual ~retained_installed_package_tree_source() = default;

  [[nodiscard]] virtual retained_installed_package_tree locate(
      const pkgstate::installed_package& package) = 0;
};

/*! \brief Explicit roots and root-view authority for native session allocation. */
struct native_transaction_session_roots final {
  std::filesystem::path content_store_root;
  std::filesystem::path construction_session_root;
  std::filesystem::path package_output_root;
  std::filesystem::path artifact_root;
  std::filesystem::path installed_resource_root;
  std::filesystem::path check_resource_root;
  std::filesystem::path check_temporary_root;
  pkgexec::root_view_identity root_view;
  std::filesystem::path root_view_path;
};

/*! \brief Explicit semantic and execution policy for native sessions. */
struct native_transaction_session_policy final {
  pkgbuild::build_policy build;
  pkgfetch::acquisition_policy acquisition;
  pkgbuild_exec::execution_identity construction_execution;
  pkgcheck_exec::execution_identity check_execution;
  pkgexec::resource_limits check_limits = pkgexec::resource_limits::make();
  pkgbuild::artifact_compression compression =
      pkgbuild::artifact_compression::none;
};

/*! \brief Validated configuration for deterministic session realization. */
class native_transaction_session_configuration final {
public:
  [[nodiscard]] static native_transaction_session_configuration make(
      native_transaction_session_roots roots,
      native_transaction_session_policy policy);

  [[nodiscard]] const native_transaction_session_roots&
  roots() const noexcept;
  [[nodiscard]] const native_transaction_session_policy&
  policy() const noexcept;

private:
  native_transaction_session_configuration(
      native_transaction_session_roots roots,
      native_transaction_session_policy policy);

  native_transaction_session_roots roots_;
  native_transaction_session_policy policy_;
};

/*! \brief Stable failures owned by native session/resource location. */
enum class native_session_locator_error_code : std::uint8_t {
  invalid_configuration = 1,
  unsupported_dispatch = 2,
  collection_authority_missing = 3,
  source_coordinate_mismatch = 4,
  predecessor_construction_missing = 5,
  predecessor_construction_ambiguous = 6,
  installed_resource_mismatch = 7,
  invalid_resource_path = 8,
  construction_session_invalid = 9,
  check_session_invalid = 10,
};

/*! \brief Invalid native configuration or irreproducible physical authority. */
class native_session_locator_error final : public std::runtime_error {
public:
  native_session_locator_error(
      native_session_locator_error_code code,
      std::string message);

  [[nodiscard]] native_session_locator_error_code code() const noexcept;

private:
  native_session_locator_error_code code_;
};

/*! \brief Native deterministic source for construction and check sessions.
 *
 * The source derives call-scoped writable coordinates from one validated
 * configuration and the durable journal/dispatch identities.  Catalog source
 * roots are recovered only from the exact acquisition specification and its
 * retained native recipe coordinate.  Catalog build inputs reuse successful
 * predecessor construction resources; installed inputs are borrowed from the
 * caller-owned retained-package source.
 *
 * Session acquisition performs no filesystem observation or mutation, source
 * materialization, backend construction, execution, journal I/O, or progress
 * advancement. Fresh construction retains the exact admitted session in
 * durable controller evidence; construction restart does not reconsult this
 * source or the retained-installed-package source.
 */
class native_transaction_dispatch_session_source final
    : public transaction_dispatch_session_source {
public:
  native_transaction_dispatch_session_source(
      native_transaction_session_configuration configuration,
      retained_installed_package_tree_source& installed_packages);

  [[nodiscard]] construction_session construction(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] transaction_check_session check(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) override;

private:
  native_transaction_session_configuration configuration_;
  retained_installed_package_tree_source& installed_packages_;
};

} // namespace pkgctl
