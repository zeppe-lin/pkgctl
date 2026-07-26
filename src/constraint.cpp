// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/constraint.h>
#include <pkgctl/error.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace pkgctl {

exclude_target::exclude_target(package_name package)
    : package_(std::move(package))
{
}
const package_name& exclude_target::package() const noexcept { return package_; }

forbid_node::forbid_node(package_name package) : package_(std::move(package)) {}
const package_name& forbid_node::package() const noexcept { return package_; }

prune_subtree::prune_subtree(package_name package)
    : package_(std::move(package))
{
}
const package_name& prune_subtree::package() const noexcept { return package_; }

hold_installed_release::hold_installed_release(package_name package)
    : package_(std::move(package))
{
}
const package_name& hold_installed_release::package() const noexcept
{
  return package_;
}

require_candidate::require_candidate(package_name package)
    : package_(std::move(package))
{
}
const package_name& require_candidate::package() const noexcept
{
  return package_;
}

constraint_kind
kind(const transaction_constraint& constraint) noexcept
{
  return std::visit([](const auto& value) {
    using value_type = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<value_type, exclude_target>)
      return constraint_kind::exclude_target;
    if constexpr (std::is_same_v<value_type, forbid_node>)
      return constraint_kind::forbid_node;
    if constexpr (std::is_same_v<value_type, prune_subtree>)
      return constraint_kind::prune_subtree;
    if constexpr (std::is_same_v<value_type, hold_installed_release>)
      return constraint_kind::hold_installed_release;
    return constraint_kind::require_candidate;
  }, constraint);
}

const package_name&
constrained_package(const transaction_constraint& constraint) noexcept
{
  return std::visit([](const auto& value) -> const package_name& {
    return value.package();
  }, constraint);
}

constraint_set::constraint_set(
    std::vector<transaction_constraint> constraints)
    : constraints_(std::move(constraints))
{
}

constraint_set
constraint_set::make(std::vector<transaction_constraint> constraints)
{
  std::sort(constraints.begin(), constraints.end(),
            [](const transaction_constraint& lhs,
               const transaction_constraint& rhs) {
              if (kind(lhs) != kind(rhs))
              {
                return static_cast<unsigned>(kind(lhs)) <
                       static_cast<unsigned>(kind(rhs));
              }
              return constrained_package(lhs) < constrained_package(rhs);
            });

  for (std::size_t index = 1; index < constraints.size(); ++index)
  {
    if (kind(constraints[index - 1]) == kind(constraints[index]) &&
        constrained_package(constraints[index - 1]) ==
            constrained_package(constraints[index]))
    {
      throw error(error_code::invalid_constraint,
                  "duplicate typed constraint for package '" +
                      constrained_package(constraints[index]).string() + "'");
    }
  }

  return constraint_set(std::move(constraints));
}

const std::vector<transaction_constraint>&
constraint_set::constraints() const noexcept
{
  return constraints_;
}

} // namespace pkgctl
