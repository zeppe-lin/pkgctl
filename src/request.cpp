// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/request.h>

#include <algorithm>
#include <utility>

namespace pkgctl {

catalog_request::catalog_request(
    std::vector<pkgcatalog::acquire::collection_specification> collections,
    pkgcatalog::acquire::limits limits)
    : collections_(std::move(collections)), limits_(std::move(limits))
{
}

catalog_request catalog_request::make(
    std::vector<pkgcatalog::acquire::collection_specification> collections,
    pkgcatalog::acquire::limits limits)
{
  if (collections.empty())
    throw error(error_code::invalid_request,
                "controller catalog request has no collections");
  return catalog_request(std::move(collections), std::move(limits));
}

const std::vector<pkgcatalog::acquire::collection_specification>&
catalog_request::collections() const noexcept { return collections_; }
const pkgcatalog::acquire::limits& catalog_request::limits() const noexcept
{ return limits_; }

state_location::state_location(
    std::filesystem::path canonical_store,
    pkgstate::state_target_binding target_binding)
    : canonical_store_(std::move(canonical_store)),
      target_binding_(std::move(target_binding))
{
}

state_location state_location::make(
    std::filesystem::path canonical_store,
    pkgstate::state_target_binding target_binding)
{
  if (canonical_store.empty() || !canonical_store.is_absolute())
    throw error(error_code::invalid_request,
                "canonical state store path must be absolute");
  return state_location(std::move(canonical_store), std::move(target_binding));
}

const std::filesystem::path& state_location::canonical_store() const noexcept
{ return canonical_store_; }
const pkgstate::state_target_binding&
state_location::target_binding() const noexcept { return target_binding_; }

resolution_request::resolution_request(
    catalog_request catalog,
    state_location state,
    pkgresolve::architecture_context architectures,
    std::vector<pkgresolve::resolution_goal> goals,
    pkgresolve::resolution_policy policy)
    : catalog_(std::move(catalog)), state_(std::move(state)),
      architectures_(std::move(architectures)), goals_(std::move(goals)),
      policy_(std::move(policy))
{
}

resolution_request resolution_request::make(
    catalog_request catalog,
    state_location state,
    pkgresolve::architecture_context architectures,
    std::vector<pkgresolve::resolution_goal> goals,
    pkgresolve::resolution_policy policy)
{
  if (goals.empty())
    throw error(error_code::invalid_request,
                "controller resolution request has no goals");
  std::sort(goals.begin(), goals.end());
  for (std::size_t index = 1; index < goals.size(); ++index)
  {
    if (goals[index - 1] == goals[index])
      throw error(error_code::invalid_request,
                  "controller resolution request has a duplicate goal");
  }
  return resolution_request(std::move(catalog), std::move(state),
                            std::move(architectures), std::move(goals),
                            std::move(policy));
}

const catalog_request& resolution_request::catalog() const noexcept
{ return catalog_; }
const state_location& resolution_request::state() const noexcept
{ return state_; }
const pkgresolve::architecture_context&
resolution_request::architectures() const noexcept { return architectures_; }
const std::vector<pkgresolve::resolution_goal>&
resolution_request::goals() const noexcept { return goals_; }
const pkgresolve::resolution_policy& resolution_request::policy() const noexcept
{ return policy_; }

transaction_request::transaction_request(
    resolution_request resolution,
    pkgtransaction::convergence_policy convergence)
    : resolution_(std::move(resolution)), convergence_(std::move(convergence))
{
}

transaction_request transaction_request::make(
    resolution_request resolution,
    pkgtransaction::convergence_policy convergence)
{
  return transaction_request(std::move(resolution), std::move(convergence));
}

const resolution_request& transaction_request::resolution() const noexcept
{ return resolution_; }
const pkgtransaction::convergence_policy&
transaction_request::convergence() const noexcept { return convergence_; }

} // namespace pkgctl
