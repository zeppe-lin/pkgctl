// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/session.h>

#include <algorithm>
#include <utility>

namespace pkgctl {
namespace {

void validate_catalog_request(const catalog_request& request,
                              const pkgcatalog::catalog_snapshot& catalog)
{
  if (request.collections().size() != catalog.collections().size())
    throw error(error_code::invalid_session,
                "catalog session collection count mismatch");

  for (const auto& specification : request.collections())
  {
    const auto found = std::find_if(
        catalog.collections().begin(), catalog.collections().end(),
        [&](const pkgcatalog::catalog_collection& value) {
          return value.precedence() == specification.precedence();
        });
    if (found == catalog.collections().end() ||
        found->collection().name() != specification.name())
    {
      throw error(error_code::invalid_session,
                  "catalog session collection authority mismatch");
    }
  }
}

} // namespace

catalog_session::catalog_session(catalog_request request,
                                 pkgcatalog::catalog_snapshot catalog,
                                 session_identity identity)
    : request_(std::move(request)), catalog_(std::move(catalog)),
      identity_(std::move(identity))
{
}

catalog_session catalog_session::seal(catalog_request request,
                                      pkgcatalog::catalog_snapshot catalog)
{
  validate_catalog_request(request, catalog);
  session_identity identity = make_session_identity(
      "pkgctl/catalog-session/1", {catalog.identity().hex()});
  return catalog_session(std::move(request), std::move(catalog),
                         std::move(identity));
}

const catalog_request& catalog_session::request() const noexcept
{ return request_; }
const pkgcatalog::catalog_snapshot& catalog_session::catalog() const noexcept
{ return catalog_; }
const session_identity& catalog_session::identity() const noexcept
{ return identity_; }

resolution_session::resolution_session(
    resolution_request request,
    catalog_session catalog,
    pkgstate::snapshot installed,
    pkgresolve::resolution_result resolution,
    session_identity identity)
    : request_(std::move(request)), catalog_(std::move(catalog)),
      installed_(std::move(installed)), resolution_(std::move(resolution)),
      identity_(std::move(identity))
{
}

resolution_session resolution_session::seal(
    resolution_request request,
    catalog_session catalog,
    pkgstate::snapshot installed,
    pkgresolve::resolution_result resolution)
{
  const pkgresolve::resolution_request& native = resolution.request();
  if (native.catalog().identity() != catalog.catalog().identity() ||
      native.installed().identity() != installed.identity() ||
      native.architectures() != request.architectures() ||
      native.goals() != request.goals() ||
      native.policy() != request.policy() ||
      installed.target_binding() != request.state().target_binding())
  {
    throw error(error_code::invalid_session,
                "resolution session authority mismatch");
  }

  session_identity identity = make_session_identity(
      "pkgctl/resolution-session/1",
      {catalog.identity().hex(), installed.identity().string(),
       resolution.identity().hex()});
  return resolution_session(std::move(request), std::move(catalog),
                            std::move(installed), std::move(resolution),
                            std::move(identity));
}

const resolution_request& resolution_session::request() const noexcept
{ return request_; }
const catalog_session& resolution_session::catalog() const noexcept
{ return catalog_; }
const pkgstate::snapshot& resolution_session::installed() const noexcept
{ return installed_; }
const pkgresolve::resolution_result&
resolution_session::resolution() const noexcept { return resolution_; }
const session_identity& resolution_session::identity() const noexcept
{ return identity_; }

transaction_session::transaction_session(
    transaction_request request,
    resolution_session resolution,
    pkgtransaction::transaction_program program,
    session_identity identity)
    : request_(std::move(request)), resolution_(std::move(resolution)),
      program_(std::move(program)), identity_(std::move(identity))
{
}

transaction_session transaction_session::seal(
    transaction_request request,
    resolution_session resolution,
    pkgtransaction::transaction_program program)
{
  if (program.request().resolution().identity() !=
          resolution.resolution().identity() ||
      program.request().policy() != request.convergence())
  {
    throw error(error_code::invalid_session,
                "transaction session authority mismatch");
  }

  session_identity identity = make_session_identity(
      "pkgctl/transaction-session/1",
      {resolution.identity().hex(), program.identity().hex()});
  return transaction_session(std::move(request), std::move(resolution),
                             std::move(program), std::move(identity));
}

const transaction_request& transaction_session::request() const noexcept
{ return request_; }
const resolution_session& transaction_session::resolution() const noexcept
{ return resolution_; }
const pkgtransaction::transaction_program&
transaction_session::program() const noexcept { return program_; }
const session_identity& transaction_session::identity() const noexcept
{ return identity_; }

} // namespace pkgctl
