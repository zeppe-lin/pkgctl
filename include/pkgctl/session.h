// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file session.h
 *  \brief Exact authority handoffs retained by read-only controller sessions.
 */
#pragma once

#include <libpkgcatalog/catalog.h>
#include <libpkgresolve/result.h>
#include <libpkgstate/snapshot.h>
#include <libpkgtransaction/program.h>

#include <pkgctl/identity.h>
#include <pkgctl/request.h>

namespace pkgctl {

class catalog_session final {
public:
  [[nodiscard]] static catalog_session seal(catalog_request request,
                                            pkgcatalog::catalog_snapshot catalog);
  [[nodiscard]] const catalog_request& request() const noexcept;
  [[nodiscard]] const pkgcatalog::catalog_snapshot& catalog() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
private:
  catalog_session(catalog_request request,
                  pkgcatalog::catalog_snapshot catalog,
                  session_identity identity);
  catalog_request request_;
  pkgcatalog::catalog_snapshot catalog_;
  session_identity identity_;
};

class resolution_session final {
public:
  [[nodiscard]] static resolution_session seal(
      resolution_request request,
      catalog_session catalog,
      pkgstate::snapshot installed,
      pkgresolve::resolution_result resolution);
  [[nodiscard]] const resolution_request& request() const noexcept;
  [[nodiscard]] const catalog_session& catalog() const noexcept;
  [[nodiscard]] const pkgstate::snapshot& installed() const noexcept;
  [[nodiscard]] const pkgresolve::resolution_result& resolution() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
private:
  resolution_session(resolution_request request,
                     catalog_session catalog,
                     pkgstate::snapshot installed,
                     pkgresolve::resolution_result resolution,
                     session_identity identity);
  resolution_request request_;
  catalog_session catalog_;
  pkgstate::snapshot installed_;
  pkgresolve::resolution_result resolution_;
  session_identity identity_;
};

class transaction_session final {
public:
  [[nodiscard]] static transaction_session seal(
      transaction_request request,
      resolution_session resolution,
      pkgtransaction::transaction_program program);
  [[nodiscard]] const transaction_request& request() const noexcept;
  [[nodiscard]] const resolution_session& resolution() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_program&
  program() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;
private:
  transaction_session(transaction_request request,
                      resolution_session resolution,
                      pkgtransaction::transaction_program program,
                      session_identity identity);
  transaction_request request_;
  resolution_session resolution_;
  pkgtransaction::transaction_program program_;
  session_identity identity_;
};

} // namespace pkgctl
