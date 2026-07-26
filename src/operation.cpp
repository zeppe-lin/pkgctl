// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/operation.h>

#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <utility>

namespace pkgctl {
namespace {

bool
valid_identifier_character(unsigned char value) noexcept
{
  return value > 0x20U && value != 0x7fU && value != '/' && value != '\\';
}

} // namespace

operation_id::operation_id(std::string value) : value_(std::move(value))
{
}

operation_id
operation_id::parse(std::string_view value)
{
  if (value.empty())
    throw error(error_code::invalid_operation, "operation identity is empty");

  for (const unsigned char character : value)
  {
    if (!valid_identifier_character(character))
    {
      throw error(error_code::invalid_operation,
                  "operation identity is not a line-safe path component");
    }
  }
  return operation_id(std::string(value));
}

const std::string&
operation_id::string() const noexcept
{
  return value_;
}

bool
operator==(const operation_id& lhs, const operation_id& rhs) noexcept
{
  return lhs.value_ == rhs.value_;
}

bool
operator!=(const operation_id& lhs, const operation_id& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator<(const operation_id& lhs, const operation_id& rhs) noexcept
{
  return lhs.value_ < rhs.value_;
}

package_operation::package_operation(
    operation_id id,
    package_name package,
    operation_kind kind,
    std::vector<operation_id> prerequisites)
    : id_(std::move(id)),
      package_(std::move(package)),
      kind_(kind),
      prerequisites_(std::move(prerequisites))
{
}

package_operation
package_operation::make(operation_id id,
                        package_name package,
                        operation_kind kind,
                        std::vector<operation_id> prerequisites)
{
  std::sort(prerequisites.begin(), prerequisites.end());
  const auto duplicate = std::adjacent_find(
      prerequisites.begin(), prerequisites.end());
  if (duplicate != prerequisites.end())
  {
    throw error(error_code::invalid_operation,
                "operation '" + id.string() +
                    "' contains duplicate prerequisite '" +
                    duplicate->string() + "'");
  }
  if (std::binary_search(prerequisites.begin(), prerequisites.end(), id))
  {
    throw error(error_code::invalid_operation,
                "operation '" + id.string() + "' depends on itself");
  }

  return package_operation(std::move(id), std::move(package), kind,
                           std::move(prerequisites));
}

const operation_id&
package_operation::id() const noexcept
{
  return id_;
}

const package_name&
package_operation::package() const noexcept
{
  return package_;
}

operation_kind
package_operation::kind() const noexcept
{
  return kind_;
}

const std::vector<operation_id>&
package_operation::prerequisites() const noexcept
{
  return prerequisites_;
}

operation_graph::operation_graph(
    std::vector<package_operation> operations,
    std::vector<operation_id> execution_order)
    : operations_(std::move(operations)),
      execution_order_(std::move(execution_order))
{
}

operation_graph
operation_graph::make(std::vector<package_operation> operations)
{
  std::sort(operations.begin(), operations.end(),
            [](const package_operation& lhs, const package_operation& rhs) {
              return lhs.id() < rhs.id();
            });

  std::map<operation_id, std::size_t> by_id;
  std::map<package_name, operation_id> by_package;
  for (std::size_t index = 0; index < operations.size(); ++index)
  {
    const package_operation& operation = operations[index];
    if (!by_id.emplace(operation.id(), index).second)
    {
      throw error(error_code::duplicate_operation,
                  "duplicate operation identity '" +
                      operation.id().string() + "'");
    }
    const auto package_insert = by_package.emplace(
        operation.package(), operation.id());
    if (!package_insert.second)
    {
      throw error(error_code::duplicate_operation,
                  "multiple operations target package '" +
                      operation.package().string() + "'");
    }
  }

  std::vector<std::size_t> indegree(operations.size(), 0);
  std::vector<std::vector<std::size_t>> dependents(operations.size());
  for (std::size_t index = 0; index < operations.size(); ++index)
  {
    for (const operation_id& prerequisite : operations[index].prerequisites())
    {
      const auto found = by_id.find(prerequisite);
      if (found == by_id.end())
      {
        throw error(error_code::missing_prerequisite,
                    "operation '" + operations[index].id().string() +
                        "' requires absent operation '" +
                        prerequisite.string() + "'");
      }
      ++indegree[index];
      dependents[found->second].push_back(index);
    }
  }

  std::priority_queue<std::size_t,
                      std::vector<std::size_t>,
                      std::greater<std::size_t>> ready;
  for (std::size_t index = 0; index < indegree.size(); ++index)
  {
    if (indegree[index] == 0)
      ready.push(index);
  }

  std::vector<operation_id> order;
  order.reserve(operations.size());
  while (!ready.empty())
  {
    const std::size_t current = ready.top();
    ready.pop();
    order.push_back(operations[current].id());

    for (const std::size_t dependent : dependents[current])
    {
      --indegree[dependent];
      if (indegree[dependent] == 0)
        ready.push(dependent);
    }
  }

  if (order.size() != operations.size())
  {
    throw error(error_code::cyclic_operation_graph,
                "package operation graph contains a dependency cycle");
  }

  return operation_graph(std::move(operations), std::move(order));
}

const std::vector<package_operation>&
operation_graph::operations() const noexcept
{
  return operations_;
}

const std::vector<operation_id>&
operation_graph::execution_order() const noexcept
{
  return execution_order_;
}

const package_operation*
operation_graph::find(const operation_id& id) const noexcept
{
  const auto found = std::lower_bound(
      operations_.begin(), operations_.end(), id,
      [](const package_operation& operation, const operation_id& value) {
        return operation.id() < value;
      });
  return found != operations_.end() && found->id() == id ? &*found : nullptr;
}

} // namespace pkgctl
