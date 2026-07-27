// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file request.h
 *  \brief Explicit read-only controller requests.
 */
#pragma once

#include <filesystem>
#include <vector>

#include <libpkgcatalog-acquire/acquire.h>
#include <libpkgresolve/model.h>
#include <libpkgstate/state_target_binding.h>
#include <libpkgtransaction/model.h>

namespace pkgctl {

class catalog_request final {
public:
  [[nodiscard]] static catalog_request make(
      std::vector<pkgcatalog::acquire::collection_specification> collections,
      pkgcatalog::acquire::limits limits = pkgcatalog::acquire::limits());
  [[nodiscard]] const std::vector<
      pkgcatalog::acquire::collection_specification>&
  collections() const noexcept;
  [[nodiscard]] const pkgcatalog::acquire::limits& limits() const noexcept;
private:
  catalog_request(
      std::vector<pkgcatalog::acquire::collection_specification> collections,
      pkgcatalog::acquire::limits limits);
  std::vector<pkgcatalog::acquire::collection_specification> collections_;
  pkgcatalog::acquire::limits limits_;
};

class state_location final {
public:
  [[nodiscard]] static state_location make(
      std::filesystem::path canonical_store,
      pkgstate::state_target_binding target_binding);
  [[nodiscard]] const std::filesystem::path& canonical_store() const noexcept;
  [[nodiscard]] const pkgstate::state_target_binding&
  target_binding() const noexcept;
private:
  state_location(std::filesystem::path canonical_store,
                 pkgstate::state_target_binding target_binding);
  std::filesystem::path canonical_store_;
  pkgstate::state_target_binding target_binding_;
};

class resolution_request final {
public:
  [[nodiscard]] static resolution_request make(
      catalog_request catalog,
      state_location state,
      pkgresolve::architecture_context architectures,
      std::vector<pkgresolve::resolution_goal> goals,
      pkgresolve::resolution_policy policy = pkgresolve::resolution_policy());
  [[nodiscard]] const catalog_request& catalog() const noexcept;
  [[nodiscard]] const state_location& state() const noexcept;
  [[nodiscard]] const pkgresolve::architecture_context&
  architectures() const noexcept;
  [[nodiscard]] const std::vector<pkgresolve::resolution_goal>&
  goals() const noexcept;
  [[nodiscard]] const pkgresolve::resolution_policy& policy() const noexcept;
private:
  resolution_request(catalog_request catalog,
                     state_location state,
                     pkgresolve::architecture_context architectures,
                     std::vector<pkgresolve::resolution_goal> goals,
                     pkgresolve::resolution_policy policy);
  catalog_request catalog_;
  state_location state_;
  pkgresolve::architecture_context architectures_;
  std::vector<pkgresolve::resolution_goal> goals_;
  pkgresolve::resolution_policy policy_;
};

class transaction_request final {
public:
  [[nodiscard]] static transaction_request make(
      resolution_request resolution,
      pkgtransaction::convergence_policy convergence =
          pkgtransaction::convergence_policy::preserve_unselected());
  [[nodiscard]] const resolution_request& resolution() const noexcept;
  [[nodiscard]] const pkgtransaction::convergence_policy&
  convergence() const noexcept;
private:
  transaction_request(resolution_request resolution,
                      pkgtransaction::convergence_policy convergence);
  resolution_request resolution_;
  pkgtransaction::convergence_policy convergence_;
};

} // namespace pkgctl
