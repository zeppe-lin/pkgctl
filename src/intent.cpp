// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/intent.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace pkgctl {
namespace {

std::vector<package_name>
normalize_targets(std::vector<package_name> targets)
{
  if (targets.empty())
    throw error(error_code::invalid_intent, "package intent has no targets");

  std::sort(targets.begin(), targets.end());
  const auto duplicate = std::adjacent_find(targets.begin(), targets.end());
  if (duplicate != targets.end())
  {
    throw error(error_code::invalid_intent,
                "package intent contains duplicate target '" +
                    duplicate->string() + "'");
  }
  return targets;
}

const std::vector<package_name>&
empty_targets() noexcept
{
  static const std::vector<package_name> empty;
  return empty;
}

} // namespace

install_intent::install_intent(std::vector<package_name> targets)
    : targets_(std::move(targets))
{
}

install_intent
install_intent::make(std::vector<package_name> targets)
{
  return install_intent(normalize_targets(std::move(targets)));
}

const std::vector<package_name>&
install_intent::targets() const noexcept
{
  return targets_;
}

update_intent::update_intent(std::vector<package_name> targets)
    : targets_(std::move(targets))
{
}

update_intent
update_intent::make(std::vector<package_name> targets)
{
  return update_intent(normalize_targets(std::move(targets)));
}

const std::vector<package_name>&
update_intent::targets() const noexcept
{
  return targets_;
}

remove_intent::remove_intent(std::vector<package_name> targets)
    : targets_(std::move(targets))
{
}

remove_intent
remove_intent::make(std::vector<package_name> targets)
{
  return remove_intent(normalize_targets(std::move(targets)));
}

const std::vector<package_name>&
remove_intent::targets() const noexcept
{
  return targets_;
}

download_intent::download_intent(std::vector<package_name> targets)
    : targets_(std::move(targets))
{
}

download_intent
download_intent::make(std::vector<package_name> targets)
{
  return download_intent(normalize_targets(std::move(targets)));
}

const std::vector<package_name>&
download_intent::targets() const noexcept
{
  return targets_;
}

intent_kind
kind(const user_intent& intent) noexcept
{
  return std::visit([](const auto& value) {
    using value_type = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<value_type, install_intent>)
      return intent_kind::install;
    if constexpr (std::is_same_v<value_type, update_intent>)
      return intent_kind::update;
    if constexpr (std::is_same_v<value_type, remove_intent>)
      return intent_kind::remove;
    if constexpr (std::is_same_v<value_type, system_update_intent>)
      return intent_kind::system_update;
    return intent_kind::download;
  }, intent);
}

const std::vector<package_name>&
target_packages(const user_intent& intent) noexcept
{
  return std::visit([](const auto& value) -> const std::vector<package_name>& {
    using value_type = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<value_type, system_update_intent>)
      return empty_targets();
    else
      return value.targets();
  }, intent);
}

} // namespace pkgctl
