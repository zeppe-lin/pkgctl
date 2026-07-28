// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file effect_restart.h
 *  \brief Conservative restart assessment and continuation of effect attempts.
 */
#pragma once

#include <optional>
#include <vector>

#include <libpkgapply/journal.h>

#include <pkgctl/effect.h>
#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_store.h>

namespace pkgctl {

/*! \brief Required next controller action for one validated journal snapshot. */
enum class effect_restart_disposition : std::uint8_t {
  continue_before_lifecycle = 1,
  start_application = 2,
  resume_application = 3,
  continue_after_application = 4,
  continue_after_lifecycle = 5,
  start_publication = 6,
  reconcile_publication = 7,
  seal_terminal = 8,
  terminal = 9,
  external_resolution_required = 10,
};

class effect_restart_assessment final {
public:
  effect_restart_assessment(session_identity attempt,
                            session_identity record,
                            effect_attempt_stage stage,
                            effect_restart_disposition disposition);
  [[nodiscard]] const session_identity& attempt() const noexcept;
  [[nodiscard]] const session_identity& record() const noexcept;
  [[nodiscard]] effect_attempt_stage stage() const noexcept;
  [[nodiscard]] effect_restart_disposition disposition() const noexcept;
  [[nodiscard]] bool automatically_continuable() const noexcept;
private:
  session_identity attempt_;
  session_identity record_;
  effect_attempt_stage stage_;
  effect_restart_disposition disposition_;
};

/*! \brief Classify restart handling without touching a target or backend. */
[[nodiscard]] effect_restart_assessment
assess_effect_restart(const effect_attempt_record& record);

/*! \brief Exact subordinate material supplied to reopen one controller attempt. */
class effect_restart_checkpoint final {
public:
  [[nodiscard]] static effect_restart_checkpoint make(
      effectful_operation_session session,
      effect_attempt_record record,
      std::vector<pkgapply_exec::lifecycle_execution_result> before,
      std::optional<pkgapply::application_receipt> application,
      std::vector<pkgapply_exec::lifecycle_execution_result> after,
      std::optional<pkgstate::state_publication_request> publication_request,
      std::optional<pkgstate::state_publication_receipt> publication_receipt,
      std::optional<pkgapply::application_journal_record> application_journal =
          std::nullopt);

  [[nodiscard]] const effectful_operation_session& session() const noexcept;
  [[nodiscard]] const effect_attempt_record& record() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::lifecycle_execution_result>&
  before() const noexcept;
  [[nodiscard]] const std::optional<pkgapply::application_receipt>&
  application() const noexcept;
  [[nodiscard]] const std::vector<pkgapply_exec::lifecycle_execution_result>&
  after() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::state_publication_request>&
  publication_request() const noexcept;
  [[nodiscard]] const std::optional<pkgstate::state_publication_receipt>&
  publication_receipt() const noexcept;
  [[nodiscard]] const std::optional<pkgapply::application_journal_record>&
  application_journal() const noexcept;
private:
  effect_restart_checkpoint(
      effectful_operation_session session,
      effect_attempt_record record,
      std::vector<pkgapply_exec::lifecycle_execution_result> before,
      std::optional<pkgapply::application_receipt> application,
      std::vector<pkgapply_exec::lifecycle_execution_result> after,
      std::optional<pkgstate::state_publication_request> publication_request,
      std::optional<pkgstate::state_publication_receipt> publication_receipt,
      std::optional<pkgapply::application_journal_record> application_journal);

  effectful_operation_session session_;
  effect_attempt_record record_;
  std::vector<pkgapply_exec::lifecycle_execution_result> before_;
  std::optional<pkgapply::application_receipt> application_;
  std::vector<pkgapply_exec::lifecycle_execution_result> after_;
  std::optional<pkgstate::state_publication_request> publication_request_;
  std::optional<pkgstate::state_publication_receipt> publication_receipt_;
  std::optional<pkgapply::application_journal_record> application_journal_;
};

/*! \brief Result of conservative controller-attempt continuation. */
class effect_restart_result final {
public:
  [[nodiscard]] effect_restart_disposition disposition() const noexcept;
  [[nodiscard]] const effect_attempt_record& journal() const noexcept;
  [[nodiscard]] const std::optional<effectful_operation_result>&
  operation() const noexcept;
  [[nodiscard]] bool terminal() const noexcept;
  [[nodiscard]] bool external_resolution_required() const noexcept;
private:
  friend effect_restart_result resume_effectful_operation(
      effect_restart_checkpoint, transaction_effect_driver&,
      effect_journal_store&);
  effect_restart_result(
      effect_restart_disposition disposition,
      effect_attempt_record journal,
      std::optional<effectful_operation_result> operation);
  effect_restart_disposition disposition_;
  effect_attempt_record journal_;
  std::optional<effectful_operation_result> operation_;
};

/*! \brief Continue one exact durable attempt under a newly acquired lease. */
[[nodiscard]] effect_restart_result resume_effectful_operation(
    effect_restart_checkpoint checkpoint,
    transaction_effect_driver& driver,
    effect_journal_store& journal_store);

} // namespace pkgctl
