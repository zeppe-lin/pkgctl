// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_runtime.h
 *  \brief Caller-configured POSIX assembly of one durable transaction runtime.
 */
#pragma once

#include <memory>

#include <pkgctl/run_launch.h>
#include <pkgctl/run_native.h>

namespace pkgctl {

/*! \brief Replay-safe and semantic authorities borrowed by one runtime. */
struct transaction_run_runtime_authorities final {
  transaction_run_nonce_source& run_nonces;
  transaction_dispatch_nonce_source& dispatch_nonces;
  transaction_progress_rehydration_source& progress;
  transaction_dispatch_execution_authority_source& execution;
  transaction_dispatch_recovery_authority_source& recovery;
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
 * The caller supplies existing directory descriptors, exact replay-safe nonce
 * sources, semantic execution/recovery sources, archive authority, and already
 * selected physical backends. The runtime duplicates all three descriptors,
 * owns one run store, one effect store, native construction/check drivers, and
 * one POSIX per-dispatch effect source. Borrowed sources and backends must
 * outlive the runtime.
 *
 * The runtime only wires existing controller boundaries. It does not discover
 * paths or journals, initialize directories, issue semantic evidence, create
 * nonce policy, construct backends, wait, retry, schedule, clean up, compact,
 * or expose a command action.
 */
class posix_transaction_run_runtime final {
public:
  /*! \brief Retain three caller-selected directory authorities.
   *
   * Descriptors respectively identify the transaction-run journal directory,
   * effect-attempt journal directory, and target-mutation lock directory.
   */
  [[nodiscard]] static std::unique_ptr<posix_transaction_run_runtime>
  from_directory_fds(
      int run_store_directory_fd,
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

  /*! \brief Admit or resume one exact run and drive it under a caller bound. */
  [[nodiscard]] transaction_run_launch_result launch(
      transaction_progress progress,
      transaction_dispatch_policy dispatch_policy,
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
