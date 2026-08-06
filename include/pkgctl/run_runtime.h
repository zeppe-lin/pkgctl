// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_runtime.h
 *  \brief Caller-configured POSIX assembly of one durable transaction runtime.
 */
#pragma once

#include <memory>

#include <pkgctl/run_launch.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_native.h>
#include <pkgctl/run_nonce.h>

namespace pkgctl {

/*! \brief Semantic authorities borrowed by one runtime. */
struct transaction_run_runtime_authorities final {
  transaction_progress_rehydration_source& progress;
  transaction_dispatch_execution_authority_source& execution;
  transaction_dispatch_recovery_context_source& recovery;
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
 * The caller supplies existing directory descriptors, semantic
 * execution and recovery-context sources, archive authority, and already
 * selected physical backends. The runtime duplicates all four descriptors, owns one run store,
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

} // namespace pkgctl
