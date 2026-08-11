// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_runtime.h>

#include "run_recovery_detail.h"

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
  paths.target_lock_store = normalize_runtime_path(
      std::move(paths.target_lock_store), "target lock store");

  const std::array<std::pair<const std::filesystem::path*, const char*>, 4>
      selected{{
          {&paths.run_store, "transaction-run store"},
          {&paths.evidence_store, "transaction evidence store"},
          {&paths.effect_store, "effect journal store"},
          {&paths.target_lock_store, "target lock store"},
      }};
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
    int target_lock_directory_fd)
{
  const std::array<std::pair<directory_authority, const char*>, 4> selected{{
      {inspect_runtime_directory(
           run_store_directory_fd, "transaction-run store"),
       "transaction-run store"},
      {inspect_runtime_directory(
           evidence_store_directory_fd, "transaction evidence store"),
       "transaction evidence store"},
      {inspect_runtime_directory(
           effect_store_directory_fd, "effect journal store"),
       "effect journal store"},
      {inspect_runtime_directory(
           target_lock_directory_fd, "target lock store"),
       "target lock store"},
  }};
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

  const std::array<const std::filesystem::path*, 5> mutable_roots{{
      &session_roots.content_store_root,
      &session_roots.construction_session_root,
      &session_roots.package_output_root,
      &session_roots.artifact_root,
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
  native_transaction_progress_rehydration_context_source(
      transaction_dispatch_session_source& sessions,
      pkgexec::backend_capability_profile construction_backend,
      pkgexec::backend_capability_profile check_backend,
      native_transaction_operation_authority_source& operations)
      : sessions_(sessions), construction_backend_(std::move(construction_backend)),
        check_backend_(std::move(check_backend)), operations_(operations)
  {
  }

  construction_dispatch_recovery_context construction(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const construction_dispatch_evidence_record& evidence) override
  {
    return detail::native_construction_recovery_context(
        record, partial_progress, dispatch, evidence, sessions_,
        construction_backend_);
  }

  check_dispatch_recovery_context check(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const check_dispatch_evidence_record& evidence) override
  {
    return detail::native_check_recovery_context(
        record, partial_progress, dispatch, evidence, sessions_,
        check_backend_);
  }

  effect_restart_checkpoint operation(
      const transaction_run_journal_record& record,
      const transaction_progress& partial_progress,
      const transaction_dispatch& dispatch,
      const effect_attempt_record& evidence) override
  {
    return operations_.rehydrate(
        record, partial_progress, dispatch, evidence);
  }

private:
  transaction_dispatch_session_source& sessions_;
  pkgexec::backend_capability_profile construction_backend_;
  pkgexec::backend_capability_profile check_backend_;
  native_transaction_operation_authority_source& operations_;
};

class transaction_run_runtime_engine final {
public:
  transaction_run_runtime_engine(
      transaction_run_journal_store& runs,
      transaction_run_evidence_store& evidence,
      effect_journal_store& effects,
      int target_lock_directory_fd,
      transaction_progress_rehydration_source& progress,
      transaction_dispatch_session_source& sessions,
      transaction_operation_execution_authority_source& operation_execution,
      transaction_operation_recovery_authority_source& operation_recovery,
      transaction_effect_archive_source& archives,
      pkgexec::backend_capability_profile construction_recovery_backend,
      pkgexec::backend_capability_profile check_recovery_backend,
      transaction_run_runtime_backends backends,
      transaction_effect_body_sink* effect_bodies)
      : runs_(runs), evidence_(evidence), effects_(effects), progress_(progress),
        execution_(sessions, operation_execution),
        recovery_context_(
            sessions, std::move(construction_recovery_backend),
            std::move(check_recovery_backend), operation_recovery),
        recovery_(evidence_, recovery_context_),
        construction_(backends.construction), check_(backends.check),
        operation_(
            posix_transaction_effect_driver_source::from_lock_directory_fd(
                target_lock_directory_fd, backends.application,
                backends.lifecycle, backends.state, archives, effect_bodies))
  {
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
    native_transaction_operation_configuration operations,
    std::vector<retained_transaction_effect_archive> archives)
    : transaction_(std::move(transaction)), sessions_(std::move(sessions)),
      operations_(std::move(operations)), archives_(std::move(archives))
{
}

native_transaction_run_runtime_configuration
native_transaction_run_runtime_configuration::make(
    transaction_session transaction,
    native_transaction_session_configuration sessions,
    native_transaction_operation_configuration operations,
    std::vector<retained_transaction_effect_archive> archives)
{
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

const native_transaction_operation_configuration&
native_transaction_run_runtime_configuration::operations() const noexcept
{
  return operations_;
}

const std::vector<retained_transaction_effect_archive>&
native_transaction_run_runtime_configuration::archives() const noexcept
{
  return archives_;
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
            authorities_.operation_execution, authorities_.operation_recovery,
            authorities_.archives, backends.construction.capabilities(),
            backends.check.capabilities(), backends, nullptr)
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

class native_posix_transaction_run_runtime::implementation final {
public:
  implementation(
      int run_store_directory_fd,
      int evidence_store_directory_fd,
      int effect_store_directory_fd,
      int target_lock_directory_fd,
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
        operations_(
            configuration_.operations(), authorities.operation_specifications,
            effects_, authorities.effect_restart_bodies,
            authorities.operation_sessions),
        owned_archives_(authorities.archives == nullptr
                ? std::make_unique<explicit_transaction_effect_archive_source>(
                      explicit_transaction_effect_archive_source::make(
                          backends.archive, configuration_.archives()))
                : nullptr),
        archives_(authorities.archives != nullptr
                ? authorities.archives
                : owned_archives_.get()),
        construction_recovery_backend_(
            authorities.construction_recovery_backend != nullptr
                ? *authorities.construction_recovery_backend
                : backends.construction.capabilities()),
        check_recovery_backend_(
            authorities.check_recovery_backend != nullptr
                ? *authorities.check_recovery_backend
                : backends.check.capabilities()),
        progress_context_(
            sessions_, construction_recovery_backend_, check_recovery_backend_,
            operations_),
        progress_(
            configuration_.transaction(), evidence_, effects_,
            progress_context_),
        engine_(
            runs_, evidence_, effects_, target_lock_directory_fd, progress_,
            sessions_, operations_, operations_, *archives_,
            construction_recovery_backend_, check_recovery_backend_,
            {backends.construction, backends.check, backends.application,
             backends.lifecycle, backends.state},
            authorities.effect_bodies)
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
  native_transaction_operation_authority_source operations_;
  std::unique_ptr<explicit_transaction_effect_archive_source> owned_archives_;
  transaction_effect_archive_source* archives_;
  pkgexec::backend_capability_profile construction_recovery_backend_;
  pkgexec::backend_capability_profile check_recovery_backend_;
  native_transaction_progress_rehydration_context_source progress_context_;
  stored_transaction_progress_rehydration_source progress_;
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
  auto runs = open_runtime_directory(paths.run_store, "transaction-run store");
  auto evidence = open_runtime_directory(
      paths.evidence_store, "transaction evidence store");
  auto effects = open_runtime_directory(
      paths.effect_store, "effect journal store");
  auto locks = open_runtime_directory(
      paths.target_lock_store, "target lock store");
  return from_directory_fds(
      runs.get(), evidence.get(), effects.get(), locks.get(),
      std::move(configuration), authorities, backends);
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
