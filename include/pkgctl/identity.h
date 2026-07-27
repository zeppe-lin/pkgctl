// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file identity.h
 *  \brief Domain-separated read-only controller session identities.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace pkgctl {

class session_identity final {
public:
  [[nodiscard]] const std::string& hex() const noexcept;
  friend bool operator==(const session_identity& lhs,
                         const session_identity& rhs) noexcept;
  friend bool operator!=(const session_identity& lhs,
                         const session_identity& rhs) noexcept;
  friend bool operator<(const session_identity& lhs,
                        const session_identity& rhs) noexcept;
private:
  friend session_identity make_session_identity(
      std::string_view, const std::vector<std::string>&);
  explicit session_identity(std::string hex);
  std::string hex_;
};

[[nodiscard]] session_identity make_session_identity(
    std::string_view domain,
    const std::vector<std::string>& fields);

} // namespace pkgctl
