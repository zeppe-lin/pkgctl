// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file intent.h
 *  \brief Closed user-intent vocabulary.
 */
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include <pkgctl/package.h>

namespace pkgctl {

/*! \brief Native user request class. */
enum class intent_kind : std::uint8_t {
  install = 1,
  update = 2,
  remove = 3,
  system_update = 4,
  download = 5,
};

class install_intent final {
public:
  [[nodiscard]] static install_intent make(std::vector<package_name> targets);
  [[nodiscard]] const std::vector<package_name>& targets() const noexcept;
private:
  explicit install_intent(std::vector<package_name> targets);
  std::vector<package_name> targets_;
};

class update_intent final {
public:
  [[nodiscard]] static update_intent make(std::vector<package_name> targets);
  [[nodiscard]] const std::vector<package_name>& targets() const noexcept;
private:
  explicit update_intent(std::vector<package_name> targets);
  std::vector<package_name> targets_;
};

class remove_intent final {
public:
  [[nodiscard]] static remove_intent make(std::vector<package_name> targets);
  [[nodiscard]] const std::vector<package_name>& targets() const noexcept;
private:
  explicit remove_intent(std::vector<package_name> targets);
  std::vector<package_name> targets_;
};

/*! \brief Request to update every eligible installed package. */
struct system_update_intent final {};

class download_intent final {
public:
  [[nodiscard]] static download_intent make(std::vector<package_name> targets);
  [[nodiscard]] const std::vector<package_name>& targets() const noexcept;
private:
  explicit download_intent(std::vector<package_name> targets);
  std::vector<package_name> targets_;
};

/*! \brief Closed native user-intent envelope. */
using user_intent = std::variant<install_intent,
                                 update_intent,
                                 remove_intent,
                                 system_update_intent,
                                 download_intent>;

[[nodiscard]] intent_kind kind(const user_intent& intent) noexcept;
[[nodiscard]] const std::vector<package_name>&
target_packages(const user_intent& intent) noexcept;

} // namespace pkgctl
