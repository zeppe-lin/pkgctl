// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_native.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include <libpkgapply-posix/mutation_lease.h>
#include <libpkgstate-apply/state_projection.h>

namespace pkgctl {
namespace {

class fd_owner final {
public:
  explicit fd_owner(int fd = -1) noexcept : fd_(fd) {}
  fd_owner(const fd_owner&) = delete;
  fd_owner& operator=(const fd_owner&) = delete;
  ~fd_owner()
  {
    if (fd_ >= 0)
      (void)::close(fd_);
  }

  [[nodiscard]] int release() noexcept
  {
    const int value = fd_;
    fd_ = -1;
    return value;
  }

private:
  int fd_;
};

[[noreturn]] void native_source_failure(
    native_effect_source_error_code code,
    const std::string& message,
    int system_error = 0)
{
  std::string text = message;
  if (system_error != 0)
    text += ": " + std::string(std::strerror(system_error));
  throw native_effect_source_error(code, system_error, std::move(text));
}

int duplicate_lock_directory(int directory_fd)
{
  if (directory_fd < 0)
    native_source_failure(
        native_effect_source_error_code::lock_directory_invalid,
        "native effect source lock-directory descriptor is invalid",
        EBADF);

  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 0);
  if (duplicate < 0)
    native_source_failure(
        native_effect_source_error_code::lock_directory_duplicate_failed,
        "cannot duplicate native effect source lock directory",
        errno);

  struct stat status {};
  if (::fstat(duplicate, &status) != 0)
  {
    const int failure = errno;
    (void)::close(duplicate);
    native_source_failure(
        native_effect_source_error_code::lock_directory_invalid,
        "cannot inspect native effect source lock directory",
        failure);
  }
  if (!S_ISDIR(status.st_mode))
  {
    (void)::close(duplicate);
    native_source_failure(
        native_effect_source_error_code::lock_directory_invalid,
        "native effect source lock authority is not a directory",
        ENOTDIR);
  }
  return duplicate;
}

struct observation_runtime final {
  observation_runtime(
      std::unique_ptr<pkgapply::posix::target_mutation_lease> value,
      pkgstate::canonical_store& store_value)
      : lease(std::move(value)), store(store_value)
  {
  }

  std::unique_ptr<pkgapply::posix::target_mutation_lease> lease;
  pkgstate::canonical_store& store;
};

struct continuation_runtime final {
  continuation_runtime(
      std::unique_ptr<pkgapply::posix::target_mutation_lease> lease_value,
      pkgstate::apply_adapter::lease_bound_application_state state_value,
      std::optional<pkgstate::apply_adapter::lease_bound_application_state>
          publication_state_value,
      std::unique_ptr<pkgimage::package_archive> archive_value,
      pkgapply::application_backend& application_value,
      pkgexec::execution_backend& lifecycle_value,
      pkgstate::canonical_store& store_value)
      : lease(std::move(lease_value)), state(std::move(state_value)),
        publication_state(std::move(publication_state_value)),
        archive(std::move(archive_value)), application(application_value),
        lifecycle(lifecycle_value), store(store_value)
  {
  }

  std::unique_ptr<pkgapply::posix::target_mutation_lease> lease;
  pkgstate::apply_adapter::lease_bound_application_state state;
  std::optional<pkgstate::apply_adapter::lease_bound_application_state>
      publication_state;
  std::unique_ptr<pkgimage::package_archive> archive;
  pkgapply::application_backend& application;
  pkgexec::execution_backend& lifecycle;
  pkgstate::canonical_store& store;
};

class owned_native_continuation final : public transaction_effect_driver {
public:
  explicit owned_native_continuation(
      std::shared_ptr<continuation_runtime> runtime)
      : runtime_(std::move(runtime)), driver_(
            runtime_->state.projection(), *runtime_->lease,
            runtime_->application, runtime_->archive.get(),
            runtime_->lifecycle, runtime_->store,
            runtime_->publication_state
                ? &runtime_->publication_state->projection() : nullptr)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return driver_.lease();
  }

  const pkgapply::lease_bound_state_projection&
  state_projection() const noexcept override
  {
    return driver_.state_projection();
  }

  const pkgapply::lease_bound_state_projection&
  publication_state_projection() const noexcept override
  {
    return driver_.publication_state_projection();
  }

  pkgapply_exec::lifecycle_execution_result execute_lifecycle(
      const pkgapply_exec::admitted_lifecycle_session& session) override
  {
    return driver_.execute_lifecycle(session);
  }

  pkgapply::application_receipt apply_application(
      const pkgapply::package_application_request& request) override
  {
    return driver_.apply_application(request);
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    return driver_.publish_state(request);
  }

  pkgapply::application_receipt resume_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_journal_record& journal) override
  {
    return driver_.resume_application(request, journal);
  }

private:
  std::shared_ptr<continuation_runtime> runtime_;
  native_transaction_effect_driver driver_;
};

class owned_continuation_state_observer final
    : public transaction_effect_state_observer {
public:
  explicit owned_continuation_state_observer(
      std::shared_ptr<continuation_runtime> runtime)
      : runtime_(std::move(runtime))
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return *runtime_->lease;
  }

  pkgstate::snapshot read_state() const override
  {
    return runtime_->store.read();
  }

private:
  std::shared_ptr<continuation_runtime> runtime_;
};

class owned_native_state_observer final
    : public transaction_effect_state_observer {
public:
  explicit owned_native_state_observer(
      std::shared_ptr<observation_runtime> runtime)
      : runtime_(std::move(runtime))
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return *runtime_->lease;
  }

  pkgstate::snapshot read_state() const override
  {
    return runtime_->store.read();
  }

private:
  std::shared_ptr<observation_runtime> runtime_;
};

class owned_native_publication_driver final
    : public transaction_effect_publication_driver {
public:
  explicit owned_native_publication_driver(
      std::shared_ptr<observation_runtime> runtime)
      : runtime_(std::move(runtime)), driver_(*runtime_->lease, runtime_->store)
  {
  }

  pkgapply::target_mutation_lease& lease() noexcept override
  {
    return driver_.lease();
  }

  pkgstate::snapshot read_state() const override
  {
    return driver_.read_state();
  }

  pkgstate::state_publication_receipt publish_state(
      const pkgstate::state_publication_request& request) override
  {
    return driver_.publish_state(request);
  }

private:
  std::shared_ptr<observation_runtime> runtime_;
  native_transaction_effect_publication_driver driver_;
};

} // namespace

native_effect_source_error::native_effect_source_error(
    native_effect_source_error_code code,
    int system_error,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code),
      system_error_(system_error)
{
}

native_effect_source_error_code
native_effect_source_error::code() const noexcept
{
  return code_;
}

int native_effect_source_error::system_error() const noexcept
{
  return system_error_;
}

std::unique_ptr<pkgimage::package_archive>
acquire_transaction_effect_archive(
    transaction_effect_archive_source& source,
    const pkgapply::package_application_request& request)
{
  const auto* incoming = request.incoming();
  if (incoming == nullptr)
    return nullptr;

  auto archive = source.open_archive(*incoming);
  if (!archive)
    native_source_failure(
        native_effect_source_error_code::archive_missing,
        "native effect archive source returned no replayable archive");

  const auto& expected = incoming->image();
  if (archive->image().identity() != expected.image().identity())
    native_source_failure(
        native_effect_source_error_code::archive_image_mismatch,
        "native effect archive belongs to another package image");
  if (archive->inspection_receipt().identity() !=
      expected.receipt().identity())
    native_source_failure(
        native_effect_source_error_code::archive_receipt_mismatch,
        "native effect archive carries another inspection receipt");
  return archive;
}

class posix_transaction_effect_driver_source::implementation final {
public:
  implementation(
      int lock_directory_fd,
      pkgapply::application_backend& application_backend,
      pkgexec::execution_backend& lifecycle_backend,
      pkgstate::canonical_store& state_store,
      transaction_effect_archive_source& archives,
      transaction_effect_body_sink* bodies)
      : lock_directory_fd_(lock_directory_fd),
        application_backend_(application_backend),
        lifecycle_backend_(lifecycle_backend), state_store_(state_store),
        archives_(archives), bodies_(bodies)
  {
  }

  ~implementation()
  {
    if (lock_directory_fd_ >= 0)
      (void)::close(lock_directory_fd_);
  }

  std::shared_ptr<continuation_runtime> acquire_continuation(
      const effectful_operation_session& session,
      const pkgapply::application_journal_header* historical = nullptr)
  {
    const auto& request = session.request().application();
    auto archive = acquire_transaction_effect_archive(archives_, request);
    auto lease = pkgapply::posix::target_mutation_lease::acquire(
        request.target(), lock_directory_fd_);
    auto state = pkgstate::apply_adapter::read_application_state(
        request, *lease, state_store_);
    std::optional<pkgstate::apply_adapter::lease_bound_application_state>
        publication_state;
    if (historical != nullptr)
    {
      publication_state =
          pkgstate::apply_adapter::read_historical_application_state(
              request, *historical, *lease, state_store_);
    }
    return std::make_shared<continuation_runtime>(
        std::move(lease), std::move(state), std::move(publication_state),
        std::move(archive), application_backend_, lifecycle_backend_,
        state_store_);
  }

  transaction_effect_body_sink* bodies() const noexcept
  {
    return bodies_;
  }

  std::shared_ptr<observation_runtime> acquire_observation(
      const effectful_operation_session& session)
  {
    auto lease = pkgapply::posix::target_mutation_lease::acquire(
        session.request().application().target(), lock_directory_fd_);
    return std::make_shared<observation_runtime>(
        std::move(lease), state_store_);
  }

private:
  int lock_directory_fd_;
  pkgapply::application_backend& application_backend_;
  pkgexec::execution_backend& lifecycle_backend_;
  pkgstate::canonical_store& state_store_;
  transaction_effect_archive_source& archives_;
  transaction_effect_body_sink* bodies_;
};

std::unique_ptr<posix_transaction_effect_driver_source>
posix_transaction_effect_driver_source::from_lock_directory_fd(
    int lock_directory_fd,
    pkgapply::application_backend& application_backend,
    pkgexec::execution_backend& lifecycle_backend,
    pkgstate::canonical_store& state_store,
    transaction_effect_archive_source& archives,
    transaction_effect_body_sink* bodies)
{
  fd_owner retained(duplicate_lock_directory(lock_directory_fd));
  auto state = std::make_unique<implementation>(
      retained.release(), application_backend, lifecycle_backend,
      state_store, archives, bodies);
  return std::unique_ptr<posix_transaction_effect_driver_source>(
      new posix_transaction_effect_driver_source(std::move(state)));
}

posix_transaction_effect_driver_source::
posix_transaction_effect_driver_source(
    std::unique_ptr<implementation> state)
    : state_(std::move(state))
{
}

posix_transaction_effect_driver_source::~
posix_transaction_effect_driver_source() = default;

transaction_effect_execution_drivers
posix_transaction_effect_driver_source::acquire_execution_drivers(
    const transaction_dispatch_execution_handoff& handoff)
{
  const auto* authority = handoff.operation();
  if (authority == nullptr)
    native_source_failure(
        native_effect_source_error_code::operation_handoff_missing,
        "native effect source received a non-operation execution handoff");

  auto runtime = state_->acquire_continuation(authority->session);
  return transaction_effect_execution_drivers{
      std::make_unique<owned_native_continuation>(runtime),
      std::make_unique<owned_continuation_state_observer>(
          std::move(runtime)),
      state_->bodies()};
}

transaction_effect_recovery_drivers
posix_transaction_effect_driver_source::acquire_recovery_drivers(
    const transaction_dispatch_recovery_handoff& handoff)
{
  const auto* checkpoint = handoff.operation();
  if (checkpoint == nullptr)
    native_source_failure(
        native_effect_source_error_code::operation_handoff_missing,
        "native effect source received a non-operation recovery handoff");

  const bool continuation =
      operation_reconciliation_requires_continuation_driver(*checkpoint);
  const bool state =
      operation_reconciliation_requires_state_observer(*checkpoint);
  const bool publication =
      operation_reconciliation_requires_publication_driver(*checkpoint);

  if (publication && (continuation || state))
    native_source_failure(
        native_effect_source_error_code::recovery_authority_shape,
        "effect recovery requested publication together with other authority");
  if (continuation && !state)
    native_source_failure(
        native_effect_source_error_code::recovery_authority_shape,
        "effect continuation recovery lacks resulting-state observation");

  transaction_effect_recovery_drivers drivers;
  if (continuation)
  {
    const pkgapply::application_journal_header* historical = nullptr;
    if (checkpoint->application() && !checkpoint->record().application() &&
        checkpoint->application_journal())
    {
      historical = &checkpoint->application_journal()->header();
    }
    auto runtime =
        state_->acquire_continuation(checkpoint->session(), historical);
    drivers.continuation =
        std::make_unique<owned_native_continuation>(runtime);
    drivers.resulting_state =
        std::make_unique<owned_continuation_state_observer>(
            std::move(runtime));
  }
  else if (publication)
  {
    auto runtime = state_->acquire_observation(checkpoint->session());
    drivers.publication =
        std::make_unique<owned_native_publication_driver>(
            std::move(runtime));
  }
  else if (state)
  {
    auto runtime = state_->acquire_observation(checkpoint->session());
    drivers.resulting_state =
        std::make_unique<owned_native_state_observer>(
            std::move(runtime));
  }
  drivers.bodies = state_->bodies();
  return drivers;
}

} // namespace pkgctl
