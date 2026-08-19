// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_runtime.h>

#include "run_recovery_detail.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace pkgctl {
namespace {

class explicit_run_nonce_source final : public transaction_run_nonce_source {
public:
  explicit explicit_run_nonce_source(transaction_run_nonce nonce)
      : nonce_(std::move(nonce))
  {
  }

  transaction_run_nonce issue(const transaction_run&) override
  {
    return nonce_;
  }

private:
  transaction_run_nonce nonce_;
};

class fd_owner final {
public:
  explicit fd_owner(int fd = -1) noexcept : fd_(fd) {}
  fd_owner(const fd_owner&) = delete;
  fd_owner& operator=(const fd_owner&) = delete;
  fd_owner(fd_owner&& other) noexcept : fd_(other.release()) {}
  fd_owner& operator=(fd_owner&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      fd_ = other.release();
    }
    return *this;
  }
  ~fd_owner() { reset(); }

  [[nodiscard]] int get() const noexcept { return fd_; }
  [[nodiscard]] int release() noexcept
  {
    const int value = fd_;
    fd_ = -1;
    return value;
  }

private:
  void reset() noexcept
  {
    if (fd_ >= 0)
      (void)::close(fd_);
    fd_ = -1;
  }

  int fd_;
};

[[noreturn]] void runtime_failure(
    native_transaction_run_runtime_error_code code,
    std::string message,
    int system_error = 0)
{
  if (system_error != 0)
    message += ": " + std::string(std::strerror(system_error));
  throw native_transaction_run_runtime_error(
      code, system_error, std::move(message));
}

[[nodiscard]] bool path_prefix(
    const std::filesystem::path& prefix,
    const std::filesystem::path& value)
{
  auto left = prefix.begin();
  auto right = value.begin();
  for (; left != prefix.end(); ++left, ++right)
    if (right == value.end() || *left != *right)
      return false;
  return true;
}

[[nodiscard]] bool paths_overlap(
    const std::filesystem::path& first,
    const std::filesystem::path& second)
{
  return path_prefix(first, second) || path_prefix(second, first);
}

[[nodiscard]] std::filesystem::path normalize_runtime_path(
    std::filesystem::path path,
    std::string_view description)
{
  if (path.empty() || !path.is_absolute())
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        std::string(description) + " must be absolute");
  path = path.lexically_normal();
  if (path == path.root_path())
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        std::string(description) + " must not be the filesystem root");
  return path;
}

void validate_runtime_paths(native_transaction_run_runtime_paths& paths)
{
  paths.run_store = normalize_runtime_path(
      std::move(paths.run_store), "transaction-run store");
  paths.evidence_store = normalize_runtime_path(
      std::move(paths.evidence_store), "transaction evidence store");
  paths.effect_store = normalize_runtime_path(
      std::move(paths.effect_store), "effect journal store");
  if (paths.target_lock_store)
    paths.target_lock_store = normalize_runtime_path(
        std::move(*paths.target_lock_store), "target lock store");

  std::vector<std::pair<const std::filesystem::path*, const char*>> selected{
      {&paths.run_store, "transaction-run store"},
      {&paths.evidence_store, "transaction evidence store"},
      {&paths.effect_store, "effect journal store"},
  };
  if (paths.target_lock_store)
    selected.push_back({&*paths.target_lock_store, "target lock store"});
  for (std::size_t first = 0U; first < selected.size(); ++first)
    for (std::size_t second = first + 1U; second < selected.size(); ++second)
      if (paths_overlap(*selected[first].first, *selected[second].first))
        runtime_failure(
            native_transaction_run_runtime_error_code::directory_overlap,
            std::string(selected[first].second) + " overlaps " +
                selected[second].second);
}

[[nodiscard]] fd_owner open_runtime_directory(
    const std::filesystem::path& path,
    std::string_view description)
{
  const int fd = ::open(
      path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    runtime_failure(
        native_transaction_run_runtime_error_code::directory_open_failed,
        "cannot open " + std::string(description) + " '" + path.string() +
            "'",
        errno);
  return fd_owner(fd);
}

struct directory_authority final {
  dev_t device;
  ino_t inode;
};

[[nodiscard]] directory_authority inspect_runtime_directory(
    int fd,
    std::string_view description)
{
  if (fd < 0)
    runtime_failure(
        native_transaction_run_runtime_error_code::directory_invalid,
        std::string(description) + " descriptor is invalid", EBADF);
  struct stat status {};
  if (::fstat(fd, &status) != 0)
    runtime_failure(
        native_transaction_run_runtime_error_code::directory_invalid,
        "cannot inspect " + std::string(description), errno);
  if (!S_ISDIR(status.st_mode))
    runtime_failure(
        native_transaction_run_runtime_error_code::directory_invalid,
        std::string(description) + " authority is not a directory", ENOTDIR);
  return {status.st_dev, status.st_ino};
}

void validate_distinct_runtime_directories(
    int run_store_directory_fd,
    int evidence_store_directory_fd,
    int effect_store_directory_fd,
    std::optional<int> target_lock_directory_fd)
{
  std::vector<std::pair<directory_authority, const char*>> selected{
      {inspect_runtime_directory(
           run_store_directory_fd, "transaction-run store"),
       "transaction-run store"},
      {inspect_runtime_directory(
           evidence_store_directory_fd, "transaction evidence store"),
       "transaction evidence store"},
      {inspect_runtime_directory(
           effect_store_directory_fd, "effect journal store"),
       "effect journal store"},
  };
  if (target_lock_directory_fd)
    selected.push_back(
        {inspect_runtime_directory(*target_lock_directory_fd, "target lock store"),
         "target lock store"});
  for (std::size_t first = 0U; first < selected.size(); ++first)
    for (std::size_t second = first + 1U; second < selected.size(); ++second)
      if (selected[first].first.device == selected[second].first.device &&
          selected[first].first.inode == selected[second].first.inode)
        runtime_failure(
            native_transaction_run_runtime_error_code::directory_overlap,
            std::string(selected[first].second) + " and " +
                selected[second].second + " name the same directory");
}

void validate_native_configuration(
    const transaction_session& transaction,
    const native_transaction_session_configuration& sessions,
    const native_transaction_operation_configuration& operations)
{
  if (transaction.identity() != operations.transaction().identity())
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "native runtime session and operation authority name different "
        "transactions");

  const auto& session_roots = sessions.roots();
  const auto& lifecycle = operations.lifecycle();
  if (session_roots.root_view_path == lifecycle.execution_root_path &&
      session_roots.root_view != lifecycle.execution_root)
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "one execution-root path names contradictory root-view identities");
  if (session_roots.root_view_path != lifecycle.execution_root_path &&
      paths_overlap(
          session_roots.root_view_path, lifecycle.execution_root_path))
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "construction/check and lifecycle execution roots overlap without "
        "being the same authority path");
  if (paths_overlap(
          session_roots.root_view_path, lifecycle.target_root_path) ||
      paths_overlap(session_roots.root_view_path, lifecycle.session_root))
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "construction/check root-view authority overlaps target or lifecycle "
        "session authority");

  const std::array<const std::filesystem::path*, 6> mutable_roots{{
      &session_roots.content_store_root,
      &session_roots.construction_session_root,
      &session_roots.package_output_root,
      &session_roots.artifact_root,
      &session_roots.check_resource_root,
      &session_roots.check_temporary_root,
  }};
  for (const auto* root : mutable_roots)
  {
    if (paths_overlap(*root, lifecycle.execution_root_path) ||
        paths_overlap(*root, lifecycle.target_root_path) ||
        paths_overlap(*root, lifecycle.session_root))
      runtime_failure(
          native_transaction_run_runtime_error_code::invalid_configuration,
          "native construction/check storage overlaps lifecycle execution, "
          "target, or session authority");
  }
}

class native_transaction_progress_rehydration_context_source final
    : public transaction_progress_rehydration_context_source {
public:
  explicit native_transaction_progress_rehydration_context_source(
      native_transaction_operation_authority_source* operations)
      : operations_(operations)
  {
  }

  construction_dispatch_recovery_context construction(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const construction_dispatch_evidence_record& evidence) override
  {
    return detail::native_construction_recovery_context(
        record, partial_progress, dispatch, evidence);
  }

  check_dispatch_recovery_context check(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const check_dispatch_evidence_record& evidence) override
  {
    return detail::native_check_recovery_context(
        record, partial_progress, dispatch, evidence);
  }

  effect_restart_checkpoint operation(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const effect_attempt_record& evidence) override
  {
    if (operations_ == nullptr)
      runtime_failure(
          native_transaction_run_runtime_error_code::invalid_configuration,
          "operation progress recovery requested without target-operation "
          "authority");
    return operations_->rehydrate(
        record, partial_progress, dispatch, evidence);
  }

private:
  native_transaction_operation_authority_source* operations_;
};


void include_native_execution_scope(
    native_transaction_execution_scope& scopes,
    pkgtransaction::transaction_action_kind action)
{
  switch (action)
  {
    case pkgtransaction::transaction_action_kind::build:
      scopes.construction = true;
      return;
    case pkgtransaction::transaction_action_kind::check:
      scopes.check = true;
      return;
    case pkgtransaction::transaction_action_kind::lifecycle:
      scopes.lifecycle = true;
      return;
    case pkgtransaction::transaction_action_kind::install:
    case pkgtransaction::transaction_action_kind::upgrade:
    case pkgtransaction::transaction_action_kind::retain:
    case pkgtransaction::transaction_action_kind::remove:
      return;
  }
}

[[nodiscard]] bool contains_node(
    const std::vector<pkgtransaction::transaction_node_identity>& nodes,
    const pkgtransaction::transaction_node_identity& needle)
{
  return std::find(nodes.begin(), nodes.end(), needle) != nodes.end();
}

void include_unit_nodes(
    std::vector<pkgtransaction::transaction_node_identity>& result,
    const ready_transaction_unit& unit)
{
  for (const auto& node : unit.members())
  {
    if (!contains_node(result, node))
      result.push_back(node);
  }
}

[[nodiscard]] bool effect_recovery_stops_run(
    const effect_attempt_record& record)
{
  const auto assessment = assess_effect_restart(record);
  if (!assessment.automatically_continuable())
    return true;

  if (assessment.disposition() == effect_restart_disposition::terminal)
  {
    return record.terminal_outcome() !=
        std::optional<effectful_operation_outcome>(
            effectful_operation_outcome::completed);
  }

  if (assessment.disposition() != effect_restart_disposition::seal_terminal)
    return false;

  switch (record.stage())
  {
    case effect_attempt_stage::before_lifecycle_terminal:
      return !record.before().back().succeeded();
    case effect_attempt_stage::application_terminal:
      return record.application()->outcome() !=
          pkgapply::application_attempt_outcome::completed;
    case effect_attempt_stage::after_lifecycle_terminal:
      return !record.after().back().succeeded();
    case effect_attempt_stage::publication_terminal:
      return record.publication()->outcome() !=
          pkgstate::state_publication_outcome::published;
    case effect_attempt_stage::admitted:
    case effect_attempt_stage::before_lifecycle_intent:
    case effect_attempt_stage::application_intent:
    case effect_attempt_stage::after_lifecycle_intent:
    case effect_attempt_stage::publication_intent:
    case effect_attempt_stage::terminal:
      break;
  }
  throw std::runtime_error(
      "seal-terminal effect assessment has an invalid journal stage");
}

[[nodiscard]] bool effect_recovery_may_execute_lifecycle(
    const effect_attempt_record& record)
{
  const auto disposition = assess_effect_restart(record).disposition();
  switch (disposition)
  {
    case effect_restart_disposition::continue_before_lifecycle:
    case effect_restart_disposition::start_application:
    case effect_restart_disposition::resume_application:
    case effect_restart_disposition::continue_after_application:
    case effect_restart_disposition::continue_after_lifecycle:
      return record.before().size() < record.before_total() ||
          record.after().size() < record.after_total();
    case effect_restart_disposition::start_publication:
    case effect_restart_disposition::reconcile_publication:
    case effect_restart_disposition::seal_terminal:
    case effect_restart_disposition::terminal:
    case effect_restart_disposition::external_resolution_required:
      return false;
  }
  return false;
}

class transaction_run_runtime_engine final {
public:
  transaction_run_runtime_engine(
      transaction_run_journal_store& runs,
      transaction_run_evidence_store& evidence,
      effect_journal_store& effects,
      std::optional<int> target_lock_directory_fd,
      transaction_progress_rehydration_source& progress,
      transaction_dispatch_session_source& sessions,
      transaction_operation_execution_authority_source* operation_execution,
      transaction_operation_recovery_authority_source* operation_recovery,
      transaction_effect_archive_source* archives,
      pkgexec::execution_backend& construction_backend,
      pkgexec::execution_backend& check_backend,
      pkgapply::application_backend* application_backend,
      pkgapply::application_journal_store* application_journal_store,
      pkgexec::execution_backend* lifecycle_backend,
      pkgstate::canonical_store* state_backend,
      transaction_effect_body_sink* effect_bodies)
      : runs_(runs), evidence_(evidence), effects_(effects), progress_(progress),
        execution_(operation_execution != nullptr
                ? composed_transaction_dispatch_execution_authority_source(
                      sessions, *operation_execution)
                : composed_transaction_dispatch_execution_authority_source(
                      sessions)),
        recovery_context_(operation_recovery != nullptr
                ? native_transaction_dispatch_recovery_context_source(
                      *operation_recovery)
                : native_transaction_dispatch_recovery_context_source()),
        recovery_(evidence_, recovery_context_),
        construction_(construction_backend), check_(check_backend)
  {
    const bool any_operation_mechanism =
        target_lock_directory_fd.has_value() || archives != nullptr ||
        application_backend != nullptr || application_journal_store != nullptr ||
        lifecycle_backend != nullptr || state_backend != nullptr;
    const bool complete_operation_mechanism =
        target_lock_directory_fd.has_value() && archives != nullptr &&
        application_backend != nullptr && application_journal_store != nullptr &&
        lifecycle_backend != nullptr && state_backend != nullptr;
    if (any_operation_mechanism != complete_operation_mechanism)
      runtime_failure(
          native_transaction_run_runtime_error_code::invalid_configuration,
          "target-operation runtime mechanism is incomplete");
    if (complete_operation_mechanism)
      operation_ =
          posix_transaction_effect_driver_source::from_lock_directory_fd(
              *target_lock_directory_fd, *application_backend,
              *application_journal_store, *lifecycle_backend, *state_backend,
              *archives, effect_bodies);
  }

  transaction_run_launch_result launch(
      transaction_progress progress,
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy)
  {
    explicit_run_nonce_source run_nonces(std::move(run_nonce));
    return launch_transaction_run(
        std::move(progress), std::move(dispatch_policy),
        std::move(drive_policy), run_nonces, dispatch_nonces_,
        semantic_authorities(), drivers(), stores());
  }

  transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy)
  {
    return drive_transaction_run(
        std::move(journal), std::move(drive_policy), dispatch_nonces_,
        semantic_authorities(), drivers(), stores());
  }

private:
  transaction_run_advance_authorities semantic_authorities() noexcept
  {
    return transaction_run_advance_authorities{
        progress_, execution_, recovery_};
  }

  transaction_run_advance_drivers drivers() noexcept
  {
    return transaction_run_advance_drivers{
        &construction_, &check_, operation_.get()};
  }

  transaction_run_advance_stores stores() noexcept
  {
    return transaction_run_advance_stores{runs_, evidence_, &effects_};
  }

  transaction_run_journal_store& runs_;
  transaction_run_evidence_store& evidence_;
  effect_journal_store& effects_;
  transaction_progress_rehydration_source& progress_;
  composed_transaction_dispatch_execution_authority_source execution_;
  native_transaction_dispatch_recovery_context_source recovery_context_;
  stored_transaction_dispatch_recovery_authority_source recovery_;
  native_construction_driver construction_;
  native_transaction_check_driver check_;
  std::unique_ptr<posix_transaction_effect_driver_source> operation_;
  canonical_transaction_dispatch_nonce_source dispatch_nonces_;
};

} // namespace

bool native_transaction_execution_scope::any() const noexcept
{
  return construction || check || lifecycle;
}

native_transaction_execution_scope native_transaction_execution_scopes(
    const pkgtransaction::transaction_program& program)
{
  native_transaction_execution_scope result;
  for (const auto& node : program.nodes())
    include_native_execution_scope(result, node.action());
  return result;
}

native_transaction_execution_scope native_transaction_resume_execution_scopes(
    const pkgtransaction::transaction_program& program,
    const transaction_run_journal_record& record,
    int evidence_store_directory_fd,
    int effect_store_directory_fd)
{
  if (record.complete() || record.failed() || record.stopped())
    return {};

  auto evidence = posix_transaction_run_evidence_store::from_directory_fd(
      evidence_store_directory_fd);
  auto effects = posix_effect_journal_store::from_directory_fd(
      effect_store_directory_fd);

  native_transaction_execution_scope result;
  std::vector<pkgtransaction::transaction_node_identity> owned;
  for (const auto& retained : record.dispatches())
  {
    if (retained.state() != transaction_dispatch_state::completed &&
        retained.state() != transaction_dispatch_state::started)
    {
      continue;
    }

    include_unit_nodes(owned, retained.dispatch().unit());
    if (retained.state() != transaction_dispatch_state::started)
      continue;

    const auto& dispatch = retained.dispatch();
    if (dispatch.unit().kind() == transaction_unit_kind::construction ||
        dispatch.unit().kind() == transaction_unit_kind::check)
    {
      if (!retained.attempt_session())
        continue;
      const auto& attempt = *retained.attempt_session();
      if (dispatch.unit().kind() == transaction_unit_kind::construction)
      {
        if (!evidence.load_construction(
                record.journal(), dispatch.identity(), attempt) &&
            evidence.load_construction_attempt(
                record.journal(), dispatch.identity(), attempt))
          result.construction = true;
      }
      else
      {
        if (!evidence.load_check(
                record.journal(), dispatch.identity(), attempt) &&
            evidence.load_check_attempt(
                record.journal(), dispatch.identity(), attempt))
          result.check = true;
      }
      continue;
    }

    if (!retained.observations().empty())
      return {};

    if (!retained.effect_attempt())
    {
      for (const auto& member : retained.dispatch().unit().members())
      {
        const auto* node = program.find(member);
        if (node == nullptr)
          throw std::runtime_error(
              "retained operation dispatch names an unknown transaction node");
        include_native_execution_scope(result, node->action());
      }
      continue;
    }

    const auto effect = effects.load_latest(*retained.effect_attempt());
    if (!effect)
      throw std::runtime_error(
          "started operation dispatch lacks retained effect evidence");
    if (effect_recovery_stops_run(*effect))
      return {};
    if (effect_recovery_may_execute_lifecycle(*effect))
      result.lifecycle = true;
  }

  for (const auto& node : program.nodes())
  {
    if (!contains_node(owned, node.identity()))
      include_native_execution_scope(result, node.action());
  }
  return result;
}

bool native_transaction_requires_target_operation_authority(
    const transaction_session& transaction) noexcept
{
  for (const auto& node : transaction.program().nodes())
  {
    switch (node.action())
    {
      case pkgtransaction::transaction_action_kind::install:
      case pkgtransaction::transaction_action_kind::upgrade:
      case pkgtransaction::transaction_action_kind::remove:
      case pkgtransaction::transaction_action_kind::lifecycle:
        return true;
      case pkgtransaction::transaction_action_kind::build:
      case pkgtransaction::transaction_action_kind::check:
      case pkgtransaction::transaction_action_kind::retain:
        break;
    }
  }
  return false;
}

native_transaction_run_runtime_paths::native_transaction_run_runtime_paths(
    std::filesystem::path run_store_value,
    std::filesystem::path evidence_store_value,
    std::filesystem::path effect_store_value)
    : run_store(std::move(run_store_value)),
      evidence_store(std::move(evidence_store_value)),
      effect_store(std::move(effect_store_value)), target_lock_store(std::nullopt)
{
}

native_transaction_run_runtime_paths::native_transaction_run_runtime_paths(
    std::filesystem::path run_store_value,
    std::filesystem::path evidence_store_value,
    std::filesystem::path effect_store_value,
    std::filesystem::path target_lock_store_value)
    : run_store(std::move(run_store_value)),
      evidence_store(std::move(evidence_store_value)),
      effect_store(std::move(effect_store_value)),
      target_lock_store(std::move(target_lock_store_value))
{
}

native_transaction_run_runtime_error::native_transaction_run_runtime_error(
    native_transaction_run_runtime_error_code code,
    int system_error,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code),
      system_error_(system_error)
{
}

native_transaction_run_runtime_error_code
native_transaction_run_runtime_error::code() const noexcept
{
  return code_;
}

int native_transaction_run_runtime_error::system_error() const noexcept
{
  return system_error_;
}

native_transaction_run_runtime_configuration::
native_transaction_run_runtime_configuration(
    transaction_session transaction,
    native_transaction_session_configuration sessions,
    std::optional<native_transaction_operation_configuration> operations,
    std::vector<retained_transaction_effect_archive> archives)
    : transaction_(std::move(transaction)), sessions_(std::move(sessions)),
      operations_(std::move(operations)), archives_(std::move(archives))
{
}

native_transaction_run_runtime_configuration
native_transaction_run_runtime_configuration::make(
    transaction_session transaction,
    native_transaction_session_configuration sessions)
{
  if (native_transaction_requires_target_operation_authority(transaction))
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "target-operation transaction requires explicit operation authority");
  return native_transaction_run_runtime_configuration(
      std::move(transaction), std::move(sessions), std::nullopt, {});
}

native_transaction_run_runtime_configuration
native_transaction_run_runtime_configuration::make(
    transaction_session transaction,
    native_transaction_session_configuration sessions,
    native_transaction_operation_configuration operations,
    std::vector<retained_transaction_effect_archive> archives)
{
  if (!native_transaction_requires_target_operation_authority(transaction))
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "construction/check-only transaction refuses target-operation "
        "authority");
  validate_native_configuration(transaction, sessions, operations);
  return native_transaction_run_runtime_configuration(
      std::move(transaction), std::move(sessions), std::move(operations),
      std::move(archives));
}

const transaction_session&
native_transaction_run_runtime_configuration::transaction() const noexcept
{
  return transaction_;
}

const native_transaction_session_configuration&
native_transaction_run_runtime_configuration::sessions() const noexcept
{
  return sessions_;
}

const native_transaction_operation_configuration*
native_transaction_run_runtime_configuration::operations() const noexcept
{
  return operations_ ? &*operations_ : nullptr;
}

const std::vector<retained_transaction_effect_archive>&
native_transaction_run_runtime_configuration::archives() const noexcept
{
  return archives_;
}

native_transaction_run_runtime_authorities::
native_transaction_run_runtime_authorities(
    retained_installed_package_tree_source& installed_packages_value)
    : installed_packages(installed_packages_value),
      operation_specifications(nullptr), effect_restart_bodies(nullptr),
      archives(nullptr), effect_bodies(nullptr), operation_sessions(nullptr)
{
}

native_transaction_run_runtime_authorities::
native_transaction_run_runtime_authorities(
    retained_installed_package_tree_source& installed_packages_value,
    transaction_operation_specification_source& operation_specifications_value,
    transaction_effect_restart_body_source& effect_restart_bodies_value,
    transaction_effect_archive_source* archives_value,
    transaction_effect_body_sink* effect_bodies_value,
    transaction_operation_session_store* operation_sessions_value)
    : installed_packages(installed_packages_value),
      operation_specifications(&operation_specifications_value),
      effect_restart_bodies(&effect_restart_bodies_value),
      archives(archives_value), effect_bodies(effect_bodies_value),
      operation_sessions(operation_sessions_value)
{
}

native_transaction_run_runtime_backends::native_transaction_run_runtime_backends(
    pkgexec::execution_backend* construction_value,
    pkgexec::execution_backend* check_value,
    pkgimage::archive_backend& archive_value)
    : construction(construction_value), check(check_value), application(nullptr),
      application_journal(nullptr), lifecycle(nullptr), state(nullptr),
      archive(archive_value)
{
}

native_transaction_run_runtime_backends::native_transaction_run_runtime_backends(
    pkgexec::execution_backend* construction_value,
    pkgexec::execution_backend* check_value,
    pkgapply::application_backend& application_value,
    pkgapply::application_journal_store& application_journal_value,
    pkgexec::execution_backend* lifecycle_value,
    pkgstate::canonical_store& state_value,
    pkgimage::archive_backend& archive_value)
    : construction(construction_value), check(check_value),
      application(&application_value), application_journal(&application_journal_value),
      lifecycle(lifecycle_value), state(&state_value), archive(archive_value)
{
}

class posix_transaction_run_runtime::implementation final {
public:
  implementation(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
      transaction_run_runtime_authorities authorities,
      transaction_run_runtime_backends backends)
      : authorities_(authorities),
        runs_(posix_transaction_run_journal_store::from_directory_fd(
            run_store_directory_fd)),
        evidence_(posix_transaction_run_evidence_store::from_directory_fd(
            evidence_store_directory_fd)),
        effects_(posix_effect_journal_store::from_directory_fd(
            effect_store_directory_fd)),
        engine_(
            runs_, evidence_, effects_, target_lock_directory_fd,
            authorities_.progress, authorities_.sessions,
            &authorities_.operation_execution, &authorities_.operation_recovery,
            &authorities_.archives, backends.construction, backends.check,
            &backends.application, &backends.application_journal,
            &backends.lifecycle, &backends.state, nullptr)
  {
  }

  transaction_run_launch_result launch(
      transaction_progress progress,
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy)
  {
    return engine_.launch(
        std::move(progress), std::move(dispatch_policy), std::move(run_nonce),
        std::move(drive_policy));
  }

  transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy)
  {
    return engine_.drive(std::move(journal), std::move(drive_policy));
  }

private:
  transaction_run_runtime_authorities authorities_;
  posix_transaction_run_journal_store runs_;
  posix_transaction_run_evidence_store evidence_;
  posix_effect_journal_store effects_;
  transaction_run_runtime_engine engine_;
};

std::unique_ptr<posix_transaction_run_runtime>
posix_transaction_run_runtime::from_directory_fds(
    int run_store_directory_fd,
    int evidence_store_directory_fd,
    int effect_store_directory_fd,
    int target_lock_directory_fd,
    transaction_run_runtime_authorities authorities,
    transaction_run_runtime_backends backends)
{
  auto state = std::make_unique<implementation>(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, target_lock_directory_fd,
      authorities, backends);
  return std::unique_ptr<posix_transaction_run_runtime>(
      new posix_transaction_run_runtime(std::move(state)));
}

posix_transaction_run_runtime::posix_transaction_run_runtime(
    std::unique_ptr<implementation> state)
    : state_(std::move(state))
{
}

posix_transaction_run_runtime::~posix_transaction_run_runtime() = default;

transaction_run_launch_result posix_transaction_run_runtime::launch(
    transaction_progress progress,
    transaction_dispatch_policy dispatch_policy,
    transaction_run_nonce run_nonce,
    transaction_run_drive_policy drive_policy)
{
  return state_->launch(
      std::move(progress), std::move(dispatch_policy), std::move(run_nonce),
      std::move(drive_policy));
}

transaction_run_drive_result posix_transaction_run_runtime::drive(
    session_identity journal,
    transaction_run_drive_policy drive_policy)
{
  return state_->drive(std::move(journal), std::move(drive_policy));
}

void validate_native_runtime_composition(
    const native_transaction_run_runtime_configuration& configuration,
    const native_transaction_run_runtime_authorities& authorities,
    const native_transaction_run_runtime_backends& backends,
    bool has_target_lock_authority)
{
  const bool operation_capable = configuration.operations() != nullptr;
  if (operation_capable)
  {
    if (!has_target_lock_authority ||
        authorities.operation_specifications == nullptr ||
        authorities.effect_restart_bodies == nullptr ||
        backends.application == nullptr || backends.state == nullptr)
      runtime_failure(
          native_transaction_run_runtime_error_code::invalid_configuration,
          "target-operation transaction lacks complete runtime authority");
    return;
  }

  if (has_target_lock_authority ||
      authorities.operation_specifications != nullptr ||
      authorities.effect_restart_bodies != nullptr ||
      authorities.archives != nullptr || authorities.effect_bodies != nullptr ||
      authorities.operation_sessions != nullptr ||
      backends.application != nullptr || backends.lifecycle != nullptr ||
      backends.state != nullptr || !configuration.archives().empty())
    runtime_failure(
        native_transaction_run_runtime_error_code::invalid_configuration,
        "construction/check-only transaction refuses target-operation "
        "runtime authority");
}

class native_posix_transaction_run_runtime::implementation final {
private:
  class unavailable_execution_backend final : public pkgexec::execution_backend {
  public:
    unavailable_execution_backend()
        : capabilities_(pkgexec::backend_capability_profile::seal(
              pkgexec::backend_identity::from_sha256(std::string(64U, '0')),
              {}))
    {
    }

    [[nodiscard]] pkgexec::backend_capability_profile capabilities() const override
    {
      return capabilities_;
    }

    [[nodiscard]] pkgexec::execution_result execute(
        const pkgexec::execution_request&,
        const pkgexec::execution_resources&) override
    {
      throw std::runtime_error(
          "current execution backend is unavailable for durable recovery");
    }

  private:
    pkgexec::backend_capability_profile capabilities_;
  };

public:
  implementation(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      std::optional<int> target_lock_directory_fd,
      native_transaction_run_runtime_configuration configuration,
      native_transaction_run_runtime_authorities authorities,
      native_transaction_run_runtime_backends backends)
      : configuration_(std::move(configuration)),
        runs_(posix_transaction_run_journal_store::from_directory_fd(
            run_store_directory_fd)),
        evidence_(posix_transaction_run_evidence_store::from_directory_fd(
            evidence_store_directory_fd)),
        effects_(posix_effect_journal_store::from_directory_fd(
            effect_store_directory_fd)),
        sessions_(configuration_.sessions(), authorities.installed_packages),
        operations_(configuration_.operations() != nullptr
                ? std::make_unique<native_transaction_operation_authority_source>(
                      *configuration_.operations(),
                      *authorities.operation_specifications, effects_,
                      *authorities.effect_restart_bodies,
                      authorities.operation_sessions)
                : nullptr),
        owned_archives_(configuration_.operations() != nullptr &&
                authorities.archives == nullptr
                ? std::make_unique<explicit_transaction_effect_archive_source>(
                      explicit_transaction_effect_archive_source::make(
                          backends.archive, configuration_.archives()))
                : nullptr),
        archives_(configuration_.operations() != nullptr
                ? (authorities.archives != nullptr
                          ? authorities.archives
                          : owned_archives_.get())
                : nullptr),
        progress_context_(operations_.get()),
        progress_(
            configuration_.transaction(), evidence_, effects_,
            progress_context_),
        unavailable_execution_backend_(),
        engine_(
            runs_, evidence_, effects_, target_lock_directory_fd, progress_,
            sessions_, operations_.get(), operations_.get(), archives_,
            backends.construction != nullptr
                ? *backends.construction
                : static_cast<pkgexec::execution_backend&>(
                      unavailable_execution_backend_),
            backends.check != nullptr
                ? *backends.check
                : static_cast<pkgexec::execution_backend&>(
                      unavailable_execution_backend_),
            backends.application, backends.application_journal,
            configuration_.operations() != nullptr
                ? (backends.lifecycle != nullptr
                          ? backends.lifecycle
                          : static_cast<pkgexec::execution_backend*>(
                                &unavailable_execution_backend_))
                : nullptr,
            backends.state, authorities.effect_bodies)
  {
  }

  transaction_run_launch_result launch(
      transaction_dispatch_policy dispatch_policy,
      transaction_run_nonce run_nonce,
      transaction_run_drive_policy drive_policy)
  {
    return engine_.launch(
        transaction_progress::begin(configuration_.transaction()),
        std::move(dispatch_policy), std::move(run_nonce),
        std::move(drive_policy));
  }

  transaction_run_drive_result drive(
      session_identity journal,
      transaction_run_drive_policy drive_policy)
  {
    return engine_.drive(std::move(journal), std::move(drive_policy));
  }

private:
  native_transaction_run_runtime_configuration configuration_;
  posix_transaction_run_journal_store runs_;
  posix_transaction_run_evidence_store evidence_;
  posix_effect_journal_store effects_;
  native_transaction_dispatch_session_source sessions_;
  std::unique_ptr<native_transaction_operation_authority_source> operations_;
  std::unique_ptr<explicit_transaction_effect_archive_source> owned_archives_;
  transaction_effect_archive_source* archives_;
  native_transaction_progress_rehydration_context_source progress_context_;
  stored_transaction_progress_rehydration_source progress_;
  unavailable_execution_backend unavailable_execution_backend_;
  transaction_run_runtime_engine engine_;
};

std::unique_ptr<native_posix_transaction_run_runtime>
native_posix_transaction_run_runtime::open(
    native_transaction_run_runtime_paths paths,
    native_transaction_run_runtime_configuration configuration,
    native_transaction_run_runtime_authorities authorities,
    native_transaction_run_runtime_backends backends)
{
  validate_runtime_paths(paths);
  validate_native_runtime_composition(
      configuration, authorities, backends, paths.target_lock_store.has_value());
  auto runs = open_runtime_directory(paths.run_store, "transaction-run store");
  auto evidence = open_runtime_directory(
      paths.evidence_store, "transaction evidence store");
  auto effects = open_runtime_directory(
      paths.effect_store, "effect journal store");
  if (!paths.target_lock_store)
    return from_directory_fds(
        runs.get(), evidence.get(), effects.get(), std::move(configuration),
        authorities, backends);

  auto locks = open_runtime_directory(*paths.target_lock_store, "target lock store");
  return from_directory_fds(
      runs.get(), evidence.get(), effects.get(), locks.get(),
      std::move(configuration), authorities, backends);
}

std::unique_ptr<native_posix_transaction_run_runtime>
native_posix_transaction_run_runtime::from_directory_fds(
    int run_store_directory_fd,
    int evidence_store_directory_fd,
    int effect_store_directory_fd,
    native_transaction_run_runtime_configuration configuration,
    native_transaction_run_runtime_authorities authorities,
    native_transaction_run_runtime_backends backends)
{
  validate_native_runtime_composition(
      configuration, authorities, backends, false);
  validate_distinct_runtime_directories(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, std::nullopt);
  auto state = std::make_unique<implementation>(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, std::nullopt,
      std::move(configuration), authorities, backends);
  return std::unique_ptr<native_posix_transaction_run_runtime>(
      new native_posix_transaction_run_runtime(std::move(state)));
}

std::unique_ptr<native_posix_transaction_run_runtime>
native_posix_transaction_run_runtime::from_directory_fds(
    int run_store_directory_fd,
    int evidence_store_directory_fd,
    int effect_store_directory_fd,
    int target_lock_directory_fd,
    native_transaction_run_runtime_configuration configuration,
    native_transaction_run_runtime_authorities authorities,
    native_transaction_run_runtime_backends backends)
{
  validate_native_runtime_composition(
      configuration, authorities, backends, true);
  validate_distinct_runtime_directories(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, target_lock_directory_fd);
  auto state = std::make_unique<implementation>(
      run_store_directory_fd, evidence_store_directory_fd,
      effect_store_directory_fd, target_lock_directory_fd,
      std::move(configuration), authorities, backends);
  return std::unique_ptr<native_posix_transaction_run_runtime>(
      new native_posix_transaction_run_runtime(std::move(state)));
}

native_posix_transaction_run_runtime::native_posix_transaction_run_runtime(
    std::unique_ptr<implementation> state)
    : state_(std::move(state))
{
}

native_posix_transaction_run_runtime::~
native_posix_transaction_run_runtime() = default;

transaction_run_launch_result native_posix_transaction_run_runtime::launch(
    transaction_dispatch_policy dispatch_policy,
    transaction_run_nonce run_nonce,
    transaction_run_drive_policy drive_policy)
{
  return state_->launch(
      std::move(dispatch_policy), std::move(run_nonce),
      std::move(drive_policy));
}

transaction_run_drive_result native_posix_transaction_run_runtime::drive(
    session_identity journal,
    transaction_run_drive_policy drive_policy)
{
  return state_->drive(std::move(journal), std::move(drive_policy));
}

} // namespace pkgctl
