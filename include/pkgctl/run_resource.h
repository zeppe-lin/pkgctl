// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file run_resource.h
 *  \brief Native installed-package resource preparation before pure location.
 */
#pragma once

#include <pkgctl/run_locator.h>

namespace pkgobject {
class store;
}

namespace pkgctl {

/*! \brief Effectful native preparation of exact installed BUILD/CHECK trees.
 *
 * For each installed package input named by one fresh construction/check
 * dispatch, this source requires the exact retained package object selected by
 * canonical state, verifies/replays the exact retained package-image identity
 * through libpkgimage-exec, and mints a concrete call-scoped execution resource
 * identity.  Only after every required tree is prepared does it delegate to
 * native_transaction_dispatch_session_source, whose locate path remains pure.
 * Fresh construction additionally requires the provider even with no installed
 * input because its sealed artifact must later enter the reservoir before
 * retirement; an installed-input-free CHECK does not borrow that authority.
 *
 * No package is selected here.  Missing/corrupt package objects are resource
 * failures and are never repaired by catalog lookup, transaction-history
 * discovery, or re-resolution.
 */
class native_transaction_resource_session_source final
    : public transaction_dispatch_session_source {
public:
  native_transaction_resource_session_source(
      native_transaction_session_configuration configuration,
      pkgobject::store* package_objects);

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
  pkgobject::store* package_objects_;
};

} // namespace pkgctl
