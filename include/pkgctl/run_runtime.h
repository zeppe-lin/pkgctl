// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_runtime.h
 *  \brief Caller-configured POSIX assembly of one durable transaction runtime.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/run_launch.h>
#include <pkgctl/run_locator.h>
#include <pkgctl/run_operation.h>
#include <pkgctl/run_progress.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_native.h>
#include <pkgctl/run_nonce.h>

namespace pkgctl {

/*! \brief Semantic authorities borrowed by one runtime. */
struct transaction_run_runtime_authorities final {
  transaction_progress_rehydration_source& progress;
  transaction_dispatch_session_source& sessions;
  transaction_operation_execution_authority_source& operation_execution;
  transaction_operation_recovery_authority_source& operation_recovery;
  transaction_effect_archive_source& archives;
};

/*! \brief Already-selected physical mechanisms borrowed by one runtime. */
struct transaction_run_runtime_backends final {
  pkgexec::execution_backend& construction;
  pkgexec::execution_backend& check;
  pkgapply::application_backend& application;
  pkgexec::execution_backend& lifecycle;
  pkgstate::canonical_store& state;
};

/*! \brief POSIX journal and native-driver composition for bounded run control.
 *
 * The caller supplies existing directory descriptors, one shared deterministic
 * construction/check session source, operation execution/recovery sources,
 * archive authority, and already selected physical backends. The runtime
 * duplicates all four descriptors, owns one run store,
 * one construction/check evidence store, one effect store, native
 * construction/check drivers, one POSIX per-dispatch
 * effect source, and canonical committed-head dispatch nonce authority.
 * Borrowed sources and backends must outlive the runtime.
 *
 * One explicit run nonce is supplied to each launch call because that nonce
 * distinguishes caller intent between otherwise identical durable histories.
 * Dispatch nonces are controller-mechanical and are derived from the exact
 * committed head. The runtime does not discover paths or journals, initialize
 * directories, issue run intent, reconstruct semantics, construct backends,
 * wait, retry, schedule, clean up, compact, or expose a command action.
 */
class posix_transaction_run_runtime final {
public:
  /*! \brief Retain four caller-selected directory authorities.
   *
   * Descriptors respectively identify the transaction-run journal directory,
   * construction/check evidence directory, effect-attempt journal directory,
   * and target-mutation lock directory.
   */
  [[nodiscard]] static std::unique_ptr<posix_transaction_run_runtime>
  from_directory_fds(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
      transaction_run_runtime_authorities authorities,
      transaction_run_runtime_backends backends);

  posix_transaction_run_runtime(const posix_transaction_run_runtime&) = delete;
  posix_transaction_run_runtime& operator=(
      const posix_transaction_run_runtime&) = delete;
  posix_transaction_run_runtime(posix_transaction_run_runtime&&) = delete;
  posix_transaction_run_runtime& operator=(
      posix_transaction_run_runtime&&) = delete;
  ~posix_transaction_run_runtime();

  /*! \brief Admit or resume one explicit run intent and drive it boundedly. */
  [[nodiscard]] transaction_run_launch_result launch(
      transaction_progress progress,
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy);

  /*! \brief Drive one exact already-admitted journal under a caller bound. */
  [[nodiscard]] transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy);

private:
  class implementation;
  explicit posix_transaction_run_runtime(
      std::unique_ptr<implementation> state);

  std::unique_ptr<implementation> state_;
};

/*! \brief Four existing POSIX namespaces selected for native run control. */
struct native_transaction_run_runtime_paths final {
  std::filesystem::path run_store;
  std::filesystem::path evidence_store;
  std::filesystem::path effect_store;
  std::filesystem::path target_lock_store;
};

/*! \brief Stable failure classes for native runtime composition. */
enum class native_transaction_run_runtime_error_code : std::uint8_t {
  invalid_configuration = 1,
  directory_open_failed = 2,
  directory_invalid = 3,
  directory_overlap = 4,
};

/*! \brief Refused roots, descriptors, or cross-bound native authority. */
class native_transaction_run_runtime_error final : public std::runtime_error {
public:
  native_transaction_run_runtime_error(
      native_transaction_run_runtime_error_code code,
      int system_error,
      std::string message);

  [[nodiscard]] native_transaction_run_runtime_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;

private:
  native_transaction_run_runtime_error_code code_;
  int system_error_;
};

/*! \brief Complete fixed semantic/mechanical configuration for one transaction. */
class native_transaction_run_runtime_configuration final {
public:
  [[nodiscard]] static native_transaction_run_runtime_configuration make(
      transaction_session transaction,
      native_transaction_session_configuration sessions,
      native_transaction_operation_configuration operations,
      std::vector<retained_transaction_effect_archive> archives);

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const native_transaction_session_configuration&
  sessions() const noexcept;
  [[nodiscard]] const native_transaction_operation_configuration&
  operations() const noexcept;
  [[nodiscard]] const std::vector<retained_transaction_effect_archive>&
  archives() const noexcept;

private:
  native_transaction_run_runtime_configuration(
      transaction_session transaction,
      native_transaction_session_configuration sessions,
      native_transaction_operation_configuration operations,
      std::vector<retained_transaction_effect_archive> archives);

  transaction_session transaction_;
  native_transaction_session_configuration sessions_;
  native_transaction_operation_configuration operations_;
  std::vector<retained_transaction_effect_archive> archives_;
};

/*! \brief Live semantic owners bound into one native composition root.
 *
 * Optional construction/check recovery profiles are historical evidence
 * authority.  When absent, the selected live backend profile is used, which is
 * suitable for a fresh runtime that has not crossed execution contexts.  A
 * durable command may supply retained profiles so old evidence can be decoded
 * without pretending the current backend already owns execution authority.
 */
struct native_transaction_run_runtime_authorities final {
  retained_installed_package_tree_source& installed_packages;
  transaction_operation_specification_source& operation_specifications;
  transaction_effect_restart_body_source& effect_restart_bodies;
  transaction_effect_archive_source* archives = nullptr;
  transaction_effect_body_sink* effect_bodies = nullptr;
  transaction_operation_session_sink* operation_sessions = nullptr;
  const pkgexec::backend_capability_profile* construction_recovery_backend = nullptr;
  const pkgexec::backend_capability_profile* check_recovery_backend = nullptr;
};

/*! \brief Explicit selected physical mechanisms for one native runtime.
 *
 * Process backends are nullable because durable recovery may prove that no
 * current process execution can occur.  A null process backend is not
 * historical authority and cannot execute; retained recovery profiles belong
 * to native_transaction_run_runtime_authorities instead.
 */
struct native_transaction_run_runtime_backends final {
  pkgexec::execution_backend* construction;
  pkgexec::execution_backend* check;
  pkgapply::application_backend& application;
  pkgexec::execution_backend* lifecycle;
  pkgstate::canonical_store& state;
  pkgimage::archive_backend& archive;
};

/*! \brief Stable native composition root for one sealed transaction.
 *
 * The root opens or duplicates four existing POSIX namespaces and owns the
 * concrete native session locator, operation authority, archive map, semantic
 * progress rehydrator, restart chain, and effect drivers over explicitly
 * selected backends.  Live operation sensing and subordinate restart bodies
 * remain borrowed from their semantic owners and must outlive the runtime.
 *
 * No directory is initialized, no backend is selected implicitly, and no
 * transaction is launched until launch() receives an explicit durable user
 * run-intent nonce and positive drive bound.
 */
class native_posix_transaction_run_runtime final {
public:
  [[nodiscard]] static std::unique_ptr<native_posix_transaction_run_runtime>
  open(
      native_transaction_run_runtime_paths paths,
      native_transaction_run_runtime_configuration configuration,
      native_transaction_run_runtime_authorities authorities,
      native_transaction_run_runtime_backends backends);

  [[nodiscard]] static std::unique_ptr<native_posix_transaction_run_runtime>
  from_directory_fds(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
      native_transaction_run_runtime_configuration configuration,
      native_transaction_run_runtime_authorities authorities,
      native_transaction_run_runtime_backends backends);

  native_posix_transaction_run_runtime(
      const native_posix_transaction_run_runtime&) = delete;
  native_posix_transaction_run_runtime& operator=(
      const native_posix_transaction_run_runtime&) = delete;
  native_posix_transaction_run_runtime(
      native_posix_transaction_run_runtime&&) = delete;
  native_posix_transaction_run_runtime& operator=(
      native_posix_transaction_run_runtime&&) = delete;
  ~native_posix_transaction_run_runtime();

  [[nodiscard]] transaction_run_launch_result launch(
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy);

  [[nodiscard]] transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy);

private:
  class implementation;
  explicit native_posix_transaction_run_runtime(
      std::unique_ptr<implementation> state);

  std::unique_ptr<implementation> state_;
};

} // namespace pkgctl
