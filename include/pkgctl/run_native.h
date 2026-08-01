// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_native.h
 *  \brief Caller-configured POSIX effect authority for one exact dispatch.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <libpkgapply/backend.h>
#include <libpkgimage/package_archive.h>
#include <libpkgexec/backend.h>
#include <libpkgstate/canonical_store.h>

#include <pkgctl/run_advance.h>

namespace pkgctl {

/*! \brief Source of one replayable archive matching exact admitted image truth. */
class transaction_effect_archive_source {
public:
  virtual ~transaction_effect_archive_source() = default;

  /*! \brief Open one archive retaining the supplied incoming authority exactly. */
  [[nodiscard]] virtual std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority& incoming) = 0;
};

/*! \brief Open and validate the replayable archive required by one request.
 *
 * Removal requests return no archive and do not consult the source.  Incoming
 * requests require one non-null archive whose retained package-image and
 * inspection-receipt identities exactly match the admitted incoming authority.
 */
[[nodiscard]] std::unique_ptr<pkgimage::package_archive>
acquire_transaction_effect_archive(
    transaction_effect_archive_source& source,
    const pkgapply::package_application_request& request);

/*! \brief Structured failure owned by native effect-source composition. */
enum class native_effect_source_error_code : std::uint8_t {
  lock_directory_invalid = 1,
  lock_directory_duplicate_failed = 2,
  operation_handoff_missing = 3,
  archive_missing = 4,
  archive_image_mismatch = 5,
  archive_receipt_mismatch = 6,
  recovery_authority_shape = 7,
};

/*! \brief Invalid descriptor or returned authority in native effect assembly. */
class native_effect_source_error final : public std::runtime_error {
public:
  native_effect_source_error(
      native_effect_source_error_code code,
      int system_error,
      std::string message);

  [[nodiscard]] native_effect_source_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;

private:
  native_effect_source_error_code code_;
  int system_error_;
};

/*! \brief Native per-dispatch source using the POSIX outer target lease.
 *
 * The caller supplies already-selected application, lifecycle, canonical-state,
 * archive, and lock-directory authorities.  The source duplicates the lock
 * directory once.  Each operation acquisition then validates one exact
 * semantic handoff, opens and verifies any required replayable archive,
 * acquires a fresh nonblocking POSIX target mutation lease, derives the
 * lease-bound installed-state projection when continuation requires it, and
 * returns only the call-scoped authority shape selected by the controller.
 *
 * It performs no path discovery, backend construction, credential selection,
 * waiting, retry, scheduling, journal I/O, cleanup, or policy selection.
 */
class posix_transaction_effect_driver_source final
    : public transaction_effect_driver_source {
public:
  /*! \brief Retain one caller-selected lock directory and fixed native backends. */
  [[nodiscard]] static std::unique_ptr<posix_transaction_effect_driver_source>
  from_lock_directory_fd(
      int lock_directory_fd,
      pkgapply::application_backend& application_backend,
      pkgexec::execution_backend& lifecycle_backend,
      pkgstate::canonical_store& state_store,
      transaction_effect_archive_source& archives);

  posix_transaction_effect_driver_source(
      const posix_transaction_effect_driver_source&) = delete;
  posix_transaction_effect_driver_source& operator=(
      const posix_transaction_effect_driver_source&) = delete;
  posix_transaction_effect_driver_source(
      posix_transaction_effect_driver_source&&) = delete;
  posix_transaction_effect_driver_source& operator=(
      posix_transaction_effect_driver_source&&) = delete;
  ~posix_transaction_effect_driver_source() override;

  [[nodiscard]] transaction_effect_execution_drivers
  acquire_execution_drivers(
      const transaction_dispatch_execution_handoff& handoff) override;

  [[nodiscard]] transaction_effect_recovery_drivers
  acquire_recovery_drivers(
      const transaction_dispatch_recovery_handoff& handoff) override;

private:
  class implementation;
  explicit posix_transaction_effect_driver_source(
      std::unique_ptr<implementation> state);

  std::unique_ptr<implementation> state_;
};

} // namespace pkgctl
