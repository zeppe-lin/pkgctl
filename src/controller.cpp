// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/controller.h>

#include <libpkgresolve/resolver.h>
#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgtransaction/composer.h>

#include <utility>

namespace pkgctl {

catalog_session acquire_catalog(catalog_request request)
{
  pkgcatalog::catalog_snapshot catalog =
      pkgcatalog::acquire::acquire_catalog(request.collections(),
                                           request.limits());
  return catalog_session::seal(std::move(request), std::move(catalog));
}

resolution_session resolve_packages(resolution_request request)
{
  catalog_session catalog = acquire_catalog(request.catalog());
  const state_location& location = request.state();
  pkgstate::posix::canonical_generation_store store =
      pkgstate::posix::canonical_generation_store::open_existing(
          location.canonical_store(), location.target_binding());
  pkgstate::snapshot installed = store.read();

  pkgresolve::resolution_request native_request =
      pkgresolve::resolution_request::seal(
          catalog.catalog(), installed, request.architectures(),
          request.goals(), request.policy());
  pkgresolve::resolution_result resolution =
      pkgresolve::resolve(std::move(native_request));

  return resolution_session::seal(std::move(request), std::move(catalog),
                                  std::move(installed),
                                  std::move(resolution));
}

transaction_session compose_transaction(transaction_request request)
{
  resolution_session resolution = resolve_packages(request.resolution());
  pkgtransaction::transaction_request native_request =
      pkgtransaction::transaction_request::seal(
          resolution.resolution(), request.convergence());
  pkgtransaction::transaction_program program =
      pkgtransaction::compose(std::move(native_request));

  return transaction_session::seal(std::move(request), std::move(resolution),
                                   std::move(program));
}

} // namespace pkgctl
