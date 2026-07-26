// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file constraint.h
 *  \brief Typed transaction constraints with separate semantic axes.
 */
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include <pkgctl/package.h>

namespace pkgctl {

/*! \brief Native constraint class. */
enum class constraint_kind : std::uint8_t {
  exclude_target = 1,
  forbid_node = 2,
  prune_subtree = 3,
  hold_installed_release = 4,
  require_candidate = 5,
};

class exclude_target final {
public:
  explicit exclude_target(package_name package);
  [[nodiscard]] const package_name& package() const noexcept;
private:
  package_name package_;
};

class forbid_node final {
public:
  explicit forbid_node(package_name package);
  [[nodiscard]] const package_name& package() const noexcept;
private:
  package_name package_;
};

class prune_subtree final {
public:
  explicit prune_subtree(package_name package);
  [[nodiscard]] const package_name& package() const noexcept;
private:
  package_name package_;
};

class hold_installed_release final {
public:
  explicit hold_installed_release(package_name package);
  [[nodiscard]] const package_name& package() const noexcept;
private:
  package_name package_;
};

class require_candidate final {
public:
  explicit require_candidate(package_name package);
  [[nodiscard]] const package_name& package() const noexcept;
private:
  package_name package_;
};

using transaction_constraint = std::variant<exclude_target,
                                            forbid_node,
                                            prune_subtree,
                                            hold_installed_release,
                                            require_candidate>;

[[nodiscard]] constraint_kind kind(
    const transaction_constraint& constraint) noexcept;
[[nodiscard]] const package_name& constrained_package(
    const transaction_constraint& constraint) noexcept;

/*! \brief Canonically ordered set of distinct typed constraints. */
class constraint_set final {
public:
  [[nodiscard]] static constraint_set make(
      std::vector<transaction_constraint> constraints);
  [[nodiscard]] const std::vector<transaction_constraint>&
  constraints() const noexcept;
private:
  explicit constraint_set(std::vector<transaction_constraint> constraints);
  std::vector<transaction_constraint> constraints_;
};

} // namespace pkgctl
