// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_runtime.h>

#include <utility>

namespace pkgctl {
namespace {

class explicit_run_nonce_source final : public transaction_run_nonce_source {
public:
  explicit explicit_run_nonce_source(transaction_run_nonce nonce)
      : nonce_(std::move(nonce))
  {
  }

  transaction_run_nonce issue(const transaction_run&) override
  {
    return nonce_;
  }

private:
  transaction_run_nonce nonce_;
};

} // namespace

class posix_transaction_run_runtime::implementation final {
public:
  implementation(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
      transaction_run_runtime_authorities authorities,
      transaction_run_runtime_backends backends)
      : authorities_(authorities),
        runs_(posix_transaction_run_journal_store::from_directory_fd(
            run_store_directory_fd)),
        evidence_(posix_transaction_run_evidence_store::from_directory_fd(
            evidence_store_directory_fd)),
        effects_(posix_effect_journal_store::from_directory_fd(
            effect_store_directory_fd)),
        execution_(authorities_.sessions, authorities_.operation_execution),
        recovery_context_(
            authorities_.sessions, backends.construction, backends.check,
            authorities_.operation_recovery),
        recovery_(evidence_, recovery_context_),
        construction_(backends.construction), check_(backends.check),
        operation_(
            posix_transaction_effect_driver_source::from_lock_directory_fd(
                target_lock_directory_fd, backends.application,
                backends.lifecycle, backends.state, authorities.archives))
  {
  }

  transaction_run_launch_result launch(
      transaction_progress progress,
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy)
  {
    explicit_run_nonce_source run_nonces(std::move(run_nonce));
    return launch_transaction_run(
        std::move(progress), std::move(dispatch_policy),
        std::move(drive_policy), run_nonces, dispatch_nonces_,
        semantic_authorities(), drivers(), stores());
  }

  transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy)
  {
    return drive_transaction_run(
        std::move(journal), std::move(drive_policy), dispatch_nonces_,
        semantic_authorities(), drivers(), stores());
  }

private:
  transaction_run_advance_authorities semantic_authorities() noexcept
  {
    return transaction_run_advance_authorities{
        authorities_.progress, execution_, recovery_};
  }

  transaction_run_advance_drivers drivers() noexcept
  {
    return transaction_run_advance_drivers{
        &construction_, &check_, operation_.get()};
  }

  transaction_run_advance_stores stores() noexcept
  {
    return transaction_run_advance_stores{runs_, evidence_, &effects_};
  }

  transaction_run_runtime_authorities authorities_;
  posix_transaction_run_journal_store runs_;
  posix_transaction_run_evidence_store evidence_;
  posix_effect_journal_store effects_;
  composed_transaction_dispatch_execution_authority_source execution_;
  native_transaction_dispatch_recovery_context_source recovery_context_;
  stored_transaction_dispatch_recovery_authority_source recovery_;
  native_construction_driver construction_;
  native_transaction_check_driver check_;
  std::unique_ptr<posix_transaction_effect_driver_source> operation_;
  canonical_transaction_dispatch_nonce_source dispatch_nonces_;
};

std::unique_ptr<posix_transaction_run_runtime>
posix_transaction_run_runtime::from_directory_fds(
    int run_store_directory_fd,
    int evidence_store_directory_fd,
    int effect_store_directory_fd,
    int target_lock_directory_fd,
    transaction_run_runtime_authorities authorities,
    transaction_run_runtime_backends backends)
{
  auto state = std::make_unique<implementation>(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, target_lock_directory_fd,
      authorities, backends);
  return std::unique_ptr<posix_transaction_run_runtime>(
      new posix_transaction_run_runtime(std::move(state)));
}

posix_transaction_run_runtime::posix_transaction_run_runtime(
    std::unique_ptr<implementation> state)
    : state_(std::move(state))
{
}

posix_transaction_run_runtime::~posix_transaction_run_runtime() = default;

transaction_run_launch_result posix_transaction_run_runtime::launch(
    transaction_progress progress,
    transaction_dispatch_policy dispatch_policy,
    transaction_run_nonce run_nonce,
    transaction_run_drive_policy drive_policy)
{
  return state_->launch(
      std::move(progress), std::move(dispatch_policy), std::move(run_nonce),
      std::move(drive_policy));
}

transaction_run_drive_result posix_transaction_run_runtime::drive(
    session_identity journal,
    transaction_run_drive_policy drive_policy)
{
  return state_->drive(std::move(journal), std::move(drive_policy));
}

} // namespace pkgctl
