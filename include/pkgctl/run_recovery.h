// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_recovery.h
 *  \brief Evidence-backed recovery of construction and check authorities.
 */
#pragma once

#include <pkgctl/run_authority.h>
#include <pkgctl/run_evidence_store.h>

namespace pkgctl {

/*! \brief Exact bodies required to decode one retained construction attempt. */
struct construction_dispatch_recovery_context final {
  construction_session session;
  pkgfetch::source_materialization materialization;
  pkgexec::execution_request execution_request;
  pkgexec::backend_capability_profile backend;
};

/*! \brief Exact bodies required to decode one retained check attempt. */
struct check_dispatch_recovery_context final {
  transaction_check_session session;
  pkgexec::execution_request execution_request;
  pkgexec::backend_capability_profile backend;
};

/*! \brief Caller-owned source of semantic bodies absent from durable evidence.
 *
 * The durable evidence store retains canonical subordinate result bytes and the
 * identities of every body required to decode them.  It deliberately does not
 * reconstruct those bodies.  Implementations obtain the exact controller
 * session, source/check context, execution request, and backend profile from
 * their owning authorities.  Operation recovery remains delegated because its
 * evidence belongs to the effect journal boundary rather than this store.
 */
class transaction_dispatch_recovery_context_source {
public:
  virtual ~transaction_dispatch_recovery_context_source() = default;

  [[nodiscard]] virtual construction_dispatch_recovery_context construction(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch,
      const construction_dispatch_evidence_record& evidence) = 0;

  [[nodiscard]] virtual check_dispatch_recovery_context check(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch,
      const check_dispatch_evidence_record& evidence) = 0;

  [[nodiscard]] virtual effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Effect-journal recovery authority for one operation dispatch. */
class transaction_operation_recovery_authority_source {
public:
  virtual ~transaction_operation_recovery_authority_source() = default;

  [[nodiscard]] virtual effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) = 0;
};

/*! \brief Reproduce construction/check recovery context from one session source.
 *
 * The same deterministic session source used for fresh execution is consulted
 * again under the retained durable record and dispatch.  Construction source
 * material is reacquired through libpkgfetch, execution requests are reproduced
 * through the pure build/check projections, and the exact historical backend
 * capability profiles are supplied as retained evidence authority.  Recovery
 * therefore does not require a live execution backend merely to decode old
 * evidence.  Operation recovery is delegated to its effect-journal owner.
 */
class native_transaction_dispatch_recovery_context_source final
    : public transaction_dispatch_recovery_context_source {
public:
  native_transaction_dispatch_recovery_context_source(
      transaction_dispatch_session_source& sessions,
      pkgexec::backend_capability_profile construction_backend,
      pkgexec::backend_capability_profile check_backend,
      transaction_operation_recovery_authority_source& operations);

  [[nodiscard]] construction_dispatch_recovery_context construction(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch,
      const construction_dispatch_evidence_record& evidence) override;

  [[nodiscard]] check_dispatch_recovery_context check(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch,
      const check_dispatch_evidence_record& evidence) override;

  [[nodiscard]] effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) override;

private:
  transaction_dispatch_session_source& sessions_;
  pkgexec::backend_capability_profile construction_backend_;
  pkgexec::backend_capability_profile check_backend_;
  transaction_operation_recovery_authority_source& operations_;
};

/*! \brief Recover exact semantic authorities from durable typed evidence.
 *
 * Construction and check records are selected by the exact durable journal,
 * dispatch, and attempt identities.  Caller-supplied context is validated
 * against every retained identity before subordinate decoding.  The resulting
 * controller authority is rebuilt canonically and must reproduce the retained
 * controller-result identity.  Missing evidence is never interpreted as work
 * that did not happen.
 */
class stored_transaction_dispatch_recovery_authority_source final
    : public transaction_dispatch_recovery_authority_source {
public:
  stored_transaction_dispatch_recovery_authority_source(
      transaction_run_evidence_store& evidence,
      transaction_dispatch_recovery_context_source& context);

  [[nodiscard]] construction_result construction(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] transaction_check_result check(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) override;

  [[nodiscard]] effect_restart_checkpoint operation(
      const transaction_run_restart_checkpoint& checkpoint,
      const transaction_dispatch_restart_assessment& assessment,
      const transaction_dispatch& dispatch) override;

private:
  transaction_run_evidence_store& evidence_;
  transaction_dispatch_recovery_context_source& context_;
};

} // namespace pkgctl
