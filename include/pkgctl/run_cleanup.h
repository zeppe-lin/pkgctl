// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_cleanup.h
 *  \brief Terminal disposal of private transaction-run realizations.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <pkgctl/run_journal.h>
#include <pkgctl/run_locator.h>

namespace pkgctl {

/*! \brief Private runtime realization classes that carry no terminal truth. */
enum class transaction_run_private_realization_kind : std::uint8_t {
  construction_session = 1,
  package_output = 2,
  installed_resource = 3,
  check_resource = 4,
  check_temporary = 5,
  lifecycle_session = 6,
};

/*! \brief One exact disposable leaf derived from durable run authority. */
class transaction_run_private_realization final {
public:
  [[nodiscard]] transaction_run_private_realization_kind kind() const noexcept;
  [[nodiscard]] const session_identity& dispatch() const noexcept;
  [[nodiscard]] const std::filesystem::path& root() const noexcept;
  [[nodiscard]] const std::filesystem::path& relative_path() const noexcept;
  [[nodiscard]] std::filesystem::path path() const;

private:
  friend class transaction_run_cleanup_plan;

  transaction_run_private_realization(
      transaction_run_private_realization_kind kind,
      session_identity dispatch,
      std::filesystem::path root,
      std::filesystem::path relative_path);

  transaction_run_private_realization_kind kind_;
  session_identity dispatch_;
  std::filesystem::path root_;
  std::filesystem::path relative_path_;
};

/*! \brief Why private realization cleanup is or is not authorized. */
enum class transaction_run_cleanup_disposition : std::uint8_t {
  completed = 1,
  incomplete = 2,
  stopped_after_failure = 3,
};

/*! \brief Pure cleanup authority projected from one exact durable run head.
 *
 * Only a successfully completed run owns cleanup authority. Within that run,
 * only completed dispatches can have disposable realization; construction/check
 * own their exact per-dispatch trees and operation dispatches may own exact
 * lifecycle-session leaves. Released-unstarted reservations own no execution
 * tree. Incomplete and
 * failure-contained runs produce an empty target set: retained work may still
 * be required for replay, diagnosis, or explicit recovery.
 */
class transaction_run_cleanup_plan final {
public:
  [[nodiscard]] static transaction_run_cleanup_plan make(
      const transaction_run_journal_record& record,
      const native_transaction_session_configuration& configuration,
      std::optional<std::filesystem::path> lifecycle_session_root =
          std::nullopt);

  [[nodiscard]] transaction_run_cleanup_disposition
  disposition() const noexcept;
  [[nodiscard]] const session_identity& journal() const noexcept;
  [[nodiscard]] const session_identity& record() const noexcept;
  [[nodiscard]] bool eligible() const noexcept;
  [[nodiscard]] const std::vector<transaction_run_private_realization>&
  targets() const noexcept;

private:
  transaction_run_cleanup_plan(
      transaction_run_cleanup_disposition disposition,
      session_identity journal,
      session_identity record,
      std::vector<transaction_run_private_realization> targets);

  transaction_run_cleanup_disposition disposition_;
  session_identity journal_;
  session_identity record_;
  std::vector<transaction_run_private_realization> targets_;
};

/*! \brief Stable failures from descriptor-anchored private-tree disposal. */
enum class transaction_run_cleanup_error_code : std::uint8_t {
  root_open_failed = 1,
  journal_open_failed = 2,
  target_inspect_failed = 3,
  target_type_invalid = 4,
  directory_open_failed = 5,
  directory_enumeration_failed = 6,
  entry_inspect_failed = 7,
  entry_remove_failed = 8,
  target_remove_failed = 9,
  directory_prepare_failed = 10,
};

/*! \brief One refused cleanup mechanism operation. */
class transaction_run_cleanup_error final : public std::runtime_error {
public:
  transaction_run_cleanup_error(
      transaction_run_cleanup_error_code code,
      int system_error,
      std::string message);

  [[nodiscard]] transaction_run_cleanup_error_code code() const noexcept;
  [[nodiscard]] int system_error() const noexcept;

private:
  transaction_run_cleanup_error_code code_;
  int system_error_;
};

/*! \brief Mechanism for removing one exact private realization leaf. */
class transaction_run_private_realization_cleaner {
public:
  virtual ~transaction_run_private_realization_cleaner() = default;

  virtual void remove(const transaction_run_private_realization& target) = 0;
};

/*! \brief POSIX no-follow recursive disposal rooted at the planned authority.
 *
 * Authorized private directories may be made owner-removable through already
 * opened descriptors. Caller-owned realization-class roots are never chmodded.
 */
class posix_transaction_run_private_realization_cleaner final
    : public transaction_run_private_realization_cleaner {
public:
  void remove(const transaction_run_private_realization& target) override;
};

/*! \brief One non-authoritative cleanup failure retained only for reporting. */
struct transaction_run_cleanup_failure final {
  transaction_run_private_realization target;
  std::string problem;
};

/*! \brief Operational result of attempting an eligible cleanup plan. */
class transaction_run_cleanup_result final {
public:
  [[nodiscard]] const transaction_run_cleanup_plan& plan() const noexcept;
  [[nodiscard]] std::size_t cleaned() const noexcept;
  [[nodiscard]] const std::vector<transaction_run_cleanup_failure>&
  failures() const noexcept;
  [[nodiscard]] bool complete() const noexcept;

private:
  friend transaction_run_cleanup_result
  cleanup_transaction_run_private_realizations(
      transaction_run_cleanup_plan,
      transaction_run_private_realization_cleaner&);

  transaction_run_cleanup_result(
      transaction_run_cleanup_plan plan,
      std::size_t cleaned,
      std::vector<transaction_run_cleanup_failure> failures);

  transaction_run_cleanup_plan plan_;
  std::size_t cleaned_;
  std::vector<transaction_run_cleanup_failure> failures_;
};

/*! \brief Best-effort disposal without changing transaction authority.
 *
 * Ineligible plans invoke no mechanism. Eligible plans attempt every target
 * independently: one cleanup failure cannot hide another residue class and
 * cannot turn the already-completed transaction into a failed transaction.
 */
[[nodiscard]] transaction_run_cleanup_result
cleanup_transaction_run_private_realizations(
    transaction_run_cleanup_plan plan,
    transaction_run_private_realization_cleaner& cleaner);

} // namespace pkgctl
