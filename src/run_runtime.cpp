// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_runtime.h>

#include <utility>

namespace pkgctl {

class posix_transaction_run_runtime::implementation final {
public:
  implementation(
      int run_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
      transaction_run_runtime_authorities authorities,
      transaction_run_runtime_backends backends)
      : authorities_(authorities),
        runs_(posix_transaction_run_journal_store::from_directory_fd(
            run_store_directory_fd)),
        effects_(posix_effect_journal_store::from_directory_fd(
            effect_store_directory_fd)),
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
      transaction_run_drive_policy drive_policy)
  {
    return launch_transaction_run(
        std::move(progress), std::move(dispatch_policy),
        std::move(drive_policy), authorities_.run_nonces,
        authorities_.dispatch_nonces, semantic_authorities(), drivers(),
        stores());
  }

  transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy)
  {
    return drive_transaction_run(
        std::move(journal), std::move(drive_policy),
        authorities_.dispatch_nonces, semantic_authorities(), drivers(),
        stores());
  }

private:
  transaction_run_advance_authorities semantic_authorities() noexcept
  {
    return transaction_run_advance_authorities{
        authorities_.progress, authorities_.execution,
        authorities_.recovery};
  }

  transaction_run_advance_drivers drivers() noexcept
  {
    return transaction_run_advance_drivers{
        &construction_, &check_, operation_.get()};
  }

  transaction_run_advance_stores stores() noexcept
  {
    return transaction_run_advance_stores{runs_, &effects_};
  }

  transaction_run_runtime_authorities authorities_;
  posix_transaction_run_journal_store runs_;
  posix_effect_journal_store effects_;
  native_construction_driver construction_;
  native_transaction_check_driver check_;
  std::unique_ptr<posix_transaction_effect_driver_source> operation_;
};

std::unique_ptr<posix_transaction_run_runtime>
posix_transaction_run_runtime::from_directory_fds(
    int run_store_directory_fd,
    int effect_store_directory_fd,
    int target_lock_directory_fd,
    transaction_run_runtime_authorities authorities,
    transaction_run_runtime_backends backends)
{
  auto state = std::make_unique<implementation>(
      run_store_directory_fd, effect_store_directory_fd,
      target_lock_directory_fd, authorities, backends);
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
    transaction_run_drive_policy drive_policy)
{
  return state_->launch(
      std::move(progress), std::move(dispatch_policy),
      std::move(drive_policy));
}

transaction_run_drive_result posix_transaction_run_runtime::drive(
    session_identity journal,
    transaction_run_drive_policy drive_policy)
{
  return state_->drive(std::move(journal), std::move(drive_policy));
}

} // namespace pkgctl
