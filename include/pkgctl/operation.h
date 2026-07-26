// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file operation.h
 *  \brief Immutable package-operation graph.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pkgctl/package.h>

namespace pkgctl {

/*! \brief One stable orchestrator-local operation identifier. */
class operation_id final {
public:
  [[nodiscard]] static operation_id parse(std::string_view value);
  [[nodiscard]] const std::string& string() const noexcept;

  friend bool operator==(const operation_id& lhs,
                         const operation_id& rhs) noexcept;
  friend bool operator!=(const operation_id& lhs,
                         const operation_id& rhs) noexcept;
  friend bool operator<(const operation_id& lhs,
                        const operation_id& rhs) noexcept;
private:
  explicit operation_id(std::string value);
  std::string value_;
};

/*! \brief Selected package transition class. */
enum class operation_kind : std::uint8_t {
  install = 1,
  upgrade = 2,
  remove = 3,
  download = 4,
};

/*! \brief One immutable selected package operation. */
class package_operation final {
public:
  [[nodiscard]] static package_operation make(
      operation_id id,
      package_name package,
      operation_kind kind,
      std::vector<operation_id> prerequisites = {});

  [[nodiscard]] const operation_id& id() const noexcept;
  [[nodiscard]] const package_name& package() const noexcept;
  [[nodiscard]] operation_kind kind() const noexcept;
  [[nodiscard]] const std::vector<operation_id>& prerequisites() const noexcept;

private:
  package_operation(operation_id id,
                    package_name package,
                    operation_kind kind,
                    std::vector<operation_id> prerequisites);

  operation_id id_;
  package_name package_;
  operation_kind kind_;
  std::vector<operation_id> prerequisites_;
};

/*! \brief Canonical validated package-operation DAG. */
class operation_graph final {
public:
  [[nodiscard]] static operation_graph make(
      std::vector<package_operation> operations);

  /*! \brief Operations in canonical identifier order. */
  [[nodiscard]] const std::vector<package_operation>& operations() const noexcept;

  /*! \brief Canonical dependency-safe execution order. */
  [[nodiscard]] const std::vector<operation_id>& execution_order() const noexcept;

  [[nodiscard]] const package_operation* find(
      const operation_id& id) const noexcept;

private:
  operation_graph(std::vector<package_operation> operations,
                  std::vector<operation_id> execution_order);

  std::vector<package_operation> operations_;
  std::vector<operation_id> execution_order_;
};

} // namespace pkgctl
