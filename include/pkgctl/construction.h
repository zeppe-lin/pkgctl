// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file construction.h
 *  \brief Exact one-candidate source materialization and build execution.
 */
#pragma once

#include <filesystem>
#include <vector>

#include <libpkgbuild-exec/libpkgbuild-exec.h>
#include <libpkgfetch/libpkgfetch.h>

#include <pkgctl/session.h>

namespace pkgctl {

/*! \brief One exact transaction build node and semantic construction policy. */
class construction_request final {
public:
  [[nodiscard]] static construction_request make(
      transaction_session transaction,
      pkgtransaction::transaction_node_identity build_node,
      std::vector<pkgbuild::materialized_package_input> package_inputs,
      pkgbuild::build_policy build_policy,
      pkgfetch::acquisition_policy acquisition_policy =
          pkgfetch::acquisition_policy::defaults());

  [[nodiscard]] const transaction_session& transaction() const noexcept;
  [[nodiscard]] const pkgtransaction::transaction_node_identity&
  build_node() const noexcept;
  [[nodiscard]] const pkgsource::source_snapshot& source() const noexcept;
  [[nodiscard]] const std::vector<pkgbuild::materialized_package_input>&
  package_inputs() const noexcept;
  [[nodiscard]] const pkgresolve::architecture_context&
  architectures() const noexcept;
  [[nodiscard]] const pkgbuild::build_policy& build_policy() const noexcept;
  [[nodiscard]] const pkgfetch::acquisition_policy&
  acquisition_policy() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  construction_request(
      transaction_session transaction,
      pkgtransaction::transaction_node_identity build_node,
      pkgsource::source_snapshot source,
      std::vector<pkgbuild::materialized_package_input> package_inputs,
      pkgresolve::architecture_context architectures,
      pkgbuild::build_policy build_policy,
      pkgfetch::acquisition_policy acquisition_policy,
      session_identity identity);

  transaction_session transaction_;
  pkgtransaction::transaction_node_identity build_node_;
  pkgsource::source_snapshot source_;
  std::vector<pkgbuild::materialized_package_input> package_inputs_;
  pkgresolve::architecture_context architectures_;
  pkgbuild::build_policy build_policy_;
  pkgfetch::acquisition_policy acquisition_policy_;
  session_identity identity_;
};

/*! \brief Explicit host coordinates for one source and build realization. */
struct construction_paths final {
  std::filesystem::path local_source_root;
  std::filesystem::path content_store_root;
  pkgbuild_exec::session_paths build;
};

/*! \brief One admitted construction request plus concrete call-scoped resources. */
class construction_session final {
public:
  [[nodiscard]] static construction_session admit(
      construction_request request,
      construction_paths paths,
      std::vector<pkgbuild_exec::package_input_tree> package_input_trees,
      pkgbuild_exec::execution_identity execution_identity,
      pkgbuild::artifact_compression compression =
          pkgbuild::artifact_compression::none);

  [[nodiscard]] const construction_request& request() const noexcept;
  [[nodiscard]] const construction_paths& paths() const noexcept;
  [[nodiscard]] const std::vector<pkgbuild_exec::package_input_tree>&
  package_input_trees() const noexcept;
  [[nodiscard]] const pkgbuild_exec::execution_identity&
  execution_identity() const noexcept;
  [[nodiscard]] pkgbuild::artifact_compression compression() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  construction_session(
      construction_request request,
      construction_paths paths,
      std::vector<pkgbuild_exec::package_input_tree> package_input_trees,
      pkgbuild_exec::execution_identity execution_identity,
      pkgbuild::artifact_compression compression,
      session_identity identity);

  construction_request request_;
  construction_paths paths_;
  std::vector<pkgbuild_exec::package_input_tree> package_input_trees_;
  pkgbuild_exec::execution_identity execution_identity_;
  pkgbuild::artifact_compression compression_;
  session_identity identity_;
};

/*! \brief Effect authority borrowed by the construction controller. */
class construction_driver {
public:
  virtual ~construction_driver() = default;

  [[nodiscard]] virtual pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) = 0;

  [[nodiscard]] virtual pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session& session) = 0;
};

/*! \brief Native composition of libpkgfetch and libpkgbuild-exec. */
class native_construction_driver final : public construction_driver {
public:
  explicit native_construction_driver(pkgexec::execution_backend& backend);

  [[nodiscard]] pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) override;

  [[nodiscard]] pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session& session) override;

private:
  pkgexec::execution_backend& backend_;
};

enum class construction_outcome {
  build_failed,
  completed,
};

/*! \brief Complete source and build evidence for one construction session. */
class construction_result final {
public:
  [[nodiscard]] construction_outcome outcome() const noexcept;
  [[nodiscard]] bool succeeded() const noexcept;
  [[nodiscard]] const construction_session& session() const noexcept;
  [[nodiscard]] const pkgfetch::source_materialization&
  materialization() const noexcept;
  [[nodiscard]] const pkgbuild_exec::build_execution_result&
  build() const noexcept;
  [[nodiscard]] const session_identity& identity() const noexcept;

private:
  friend construction_result execute_construction(
      construction_session, construction_driver&);

  construction_result(
      construction_session session,
      pkgfetch::source_materialization materialization,
      pkgbuild_exec::build_execution_result build,
      construction_outcome outcome,
      session_identity identity);

  construction_session session_;
  pkgfetch::source_materialization materialization_;
  pkgbuild_exec::build_execution_result build_;
  construction_outcome outcome_;
  session_identity identity_;
};

/*! \brief Materialize and build one exact transaction construction node. */
[[nodiscard]] construction_result execute_construction(
    construction_session session,
    construction_driver& driver);

} // namespace pkgctl
