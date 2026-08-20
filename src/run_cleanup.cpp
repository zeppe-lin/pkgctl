// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_cleanup.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pkgctl {
namespace {

class fd_owner final {
public:
  explicit fd_owner(int fd = -1) noexcept : fd_(fd) {}
  fd_owner(const fd_owner&) = delete;
  fd_owner& operator=(const fd_owner&) = delete;
  fd_owner(fd_owner&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  fd_owner& operator=(fd_owner&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }
  ~fd_owner() { reset(); }
  [[nodiscard]] int get() const noexcept { return fd_; }

private:
  void reset() noexcept
  {
    if (fd_ >= 0)
      (void)::close(fd_);
    fd_ = -1;
  }
  int fd_;
};

struct directory_closer final {
  void operator()(DIR* directory) const noexcept
  {
    if (directory != nullptr)
      (void)::closedir(directory);
  }
};
using directory_owner = std::unique_ptr<DIR, directory_closer>;

[[noreturn]] void cleanup_failure(
    transaction_run_cleanup_error_code code,
    std::string message,
    int system_error = 0)
{
  throw transaction_run_cleanup_error(code, system_error, std::move(message));
}

[[nodiscard]] std::string kind_name(
    transaction_run_private_realization_kind kind)
{
  switch (kind)
  {
    case transaction_run_private_realization_kind::construction_session:
      return "construction session";
    case transaction_run_private_realization_kind::package_output:
      return "package output";
    case transaction_run_private_realization_kind::installed_resource:
      return "installed resource";
    case transaction_run_private_realization_kind::check_resource:
      return "check resource";
    case transaction_run_private_realization_kind::check_temporary:
      return "check temporary";
  }
  return "private realization";
}

[[nodiscard]] fd_owner open_root(
    const transaction_run_private_realization& target)
{
  const int fd = ::open(
      target.root().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd >= 0)
    return fd_owner(fd);
  if (errno == ENOENT)
    return fd_owner();
  cleanup_failure(
      transaction_run_cleanup_error_code::root_open_failed,
      "cannot open " + kind_name(target.kind()) + " cleanup root '" +
          target.root().string() + "': " + std::strerror(errno),
      errno);
}

[[nodiscard]] fd_owner open_child_directory(
    int parent,
    const std::string& name,
    transaction_run_cleanup_error_code code,
    std::string_view description)
{
  const int fd = ::openat(
      parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd >= 0)
    return fd_owner(fd);
  if (errno == ENOENT)
    return fd_owner();
  cleanup_failure(
      code,
      "cannot open " + std::string(description) + " '" + name + "': " +
          std::strerror(errno),
      errno);
}

void make_opened_private_directory_removable(int directory_fd)
{
  struct stat status {};
  if (::fstat(directory_fd, &status) != 0)
  {
    cleanup_failure(
        transaction_run_cleanup_error_code::directory_prepare_failed,
        "cannot inspect opened private realization directory: " +
            std::string(std::strerror(errno)),
        errno);
  }
  if (!S_ISDIR(status.st_mode))
  {
    cleanup_failure(
        transaction_run_cleanup_error_code::directory_prepare_failed,
        "opened private realization is no longer a directory");
  }

  if ((status.st_mode & S_IRWXU) == S_IRWXU)
    return;

  const auto current = ::geteuid();
  if (current != 0 && status.st_uid != current)
    return;

  if (::fchmod(directory_fd, status.st_mode | S_IRWXU) != 0)
  {
    cleanup_failure(
        transaction_run_cleanup_error_code::directory_prepare_failed,
        "cannot make opened private realization directory removable: " +
            std::string(std::strerror(errno)),
        errno);
  }
}

void remove_directory_at(int parent, const std::string& name);

void remove_directory_contents(int directory_fd)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    cleanup_failure(
        transaction_run_cleanup_error_code::directory_enumeration_failed,
        "cannot duplicate private realization directory: " +
            std::string(std::strerror(errno)),
        errno);

  directory_owner directory(::fdopendir(duplicate));
  if (!directory)
  {
    const int problem = errno;
    (void)::close(duplicate);
    cleanup_failure(
        transaction_run_cleanup_error_code::directory_enumeration_failed,
        "cannot enumerate private realization directory: " +
            std::string(std::strerror(problem)),
        problem);
  }

  while (true)
  {
    errno = 0;
    dirent* entry = ::readdir(directory.get());
    if (entry == nullptr)
    {
      if (errno != 0)
        cleanup_failure(
            transaction_run_cleanup_error_code::directory_enumeration_failed,
            "cannot enumerate private realization directory: " +
                std::string(std::strerror(errno)),
            errno);
      break;
    }

    const std::string name(entry->d_name);
    if (name == "." || name == "..")
      continue;

    struct stat status {};
    if (::fstatat(
            directory_fd, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0)
    {
      if (errno == ENOENT)
        continue;
      cleanup_failure(
          transaction_run_cleanup_error_code::entry_inspect_failed,
          "cannot inspect private realization entry '" + name + "': " +
              std::strerror(errno),
          errno);
    }

    if (S_ISDIR(status.st_mode))
    {
      remove_directory_at(directory_fd, name);
      continue;
    }

    if (::unlinkat(directory_fd, name.c_str(), 0) != 0 && errno != ENOENT)
      cleanup_failure(
          transaction_run_cleanup_error_code::entry_remove_failed,
          "cannot remove private realization entry '" + name + "': " +
              std::strerror(errno),
          errno);
  }
}

void remove_directory_at(int parent, const std::string& name)
{
  struct stat status {};
  if (::fstatat(parent, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0)
  {
    if (errno == ENOENT)
      return;
    cleanup_failure(
        transaction_run_cleanup_error_code::target_inspect_failed,
        "cannot inspect private realization directory '" + name + "': " +
            std::strerror(errno),
        errno);
  }
  if (!S_ISDIR(status.st_mode))
    cleanup_failure(
        transaction_run_cleanup_error_code::target_type_invalid,
        "private realization target '" + name + "' is not a directory");

  auto directory = open_child_directory(
      parent, name,
      transaction_run_cleanup_error_code::directory_open_failed,
      "private realization directory");
  if (directory.get() < 0)
    return;
  make_opened_private_directory_removable(directory.get());
  remove_directory_contents(directory.get());
  directory = fd_owner();

  if (::unlinkat(parent, name.c_str(), AT_REMOVEDIR) != 0 && errno != ENOENT)
    cleanup_failure(
        transaction_run_cleanup_error_code::target_remove_failed,
        "cannot remove private realization directory '" + name + "': " +
            std::strerror(errno),
        errno);
}

transaction_run_cleanup_disposition cleanup_disposition(
    const transaction_run_journal_record& record) noexcept
{
  if (record.complete() && !record.failed() && !record.stopped())
    return transaction_run_cleanup_disposition::completed;
  if (record.failed() || record.stopped())
    return transaction_run_cleanup_disposition::stopped_after_failure;
  return transaction_run_cleanup_disposition::incomplete;
}

} // namespace

transaction_run_private_realization::transaction_run_private_realization(
    transaction_run_private_realization_kind kind,
    session_identity dispatch,
    std::filesystem::path root,
    std::filesystem::path relative_path)
    : kind_(kind), dispatch_(std::move(dispatch)), root_(std::move(root)),
      relative_path_(std::move(relative_path))
{
}

transaction_run_private_realization_kind
transaction_run_private_realization::kind() const noexcept
{
  return kind_;
}

const session_identity&
transaction_run_private_realization::dispatch() const noexcept
{
  return dispatch_;
}

const std::filesystem::path&
transaction_run_private_realization::root() const noexcept
{
  return root_;
}

const std::filesystem::path&
transaction_run_private_realization::relative_path() const noexcept
{
  return relative_path_;
}

std::filesystem::path transaction_run_private_realization::path() const
{
  return root_ / relative_path_;
}

transaction_run_cleanup_plan::transaction_run_cleanup_plan(
    transaction_run_cleanup_disposition disposition,
    session_identity journal,
    session_identity record,
    std::vector<transaction_run_private_realization> targets)
    : disposition_(disposition), journal_(std::move(journal)),
      record_(std::move(record)), targets_(std::move(targets))
{
}

transaction_run_cleanup_plan transaction_run_cleanup_plan::make(
    const transaction_run_journal_record& record,
    const native_transaction_session_configuration& configuration)
{
  const auto disposition = cleanup_disposition(record);
  std::vector<transaction_run_private_realization> targets;
  if (disposition == transaction_run_cleanup_disposition::completed)
  {
    const auto& roots = configuration.roots();
    for (const auto& dispatch_record : record.dispatches())
    {
      if (dispatch_record.state() != transaction_dispatch_state::completed)
        continue;

      const auto& dispatch = dispatch_record.dispatch();
      const auto relative = std::filesystem::path(record.journal().hex()) /
          dispatch.identity().hex();
      if (dispatch.unit().kind() == transaction_unit_kind::construction ||
          dispatch.unit().kind() == transaction_unit_kind::check)
        targets.push_back(transaction_run_private_realization(
            transaction_run_private_realization_kind::installed_resource,
            dispatch.identity(), roots.installed_resource_root, relative));
      switch (dispatch.unit().kind())
      {
        case transaction_unit_kind::construction:
          targets.push_back(transaction_run_private_realization(
              transaction_run_private_realization_kind::construction_session,
              dispatch.identity(), roots.construction_session_root, relative));
          targets.push_back(transaction_run_private_realization(
              transaction_run_private_realization_kind::package_output,
              dispatch.identity(), roots.package_output_root, relative));
          break;
        case transaction_unit_kind::check:
          targets.push_back(transaction_run_private_realization(
              transaction_run_private_realization_kind::check_resource,
              dispatch.identity(), roots.check_resource_root, relative));
          targets.push_back(transaction_run_private_realization(
              transaction_run_private_realization_kind::check_temporary,
              dispatch.identity(), roots.check_temporary_root, relative));
          break;
        case transaction_unit_kind::operation:
          break;
      }
    }
  }
  return transaction_run_cleanup_plan(
      disposition, record.journal(), record.identity(), std::move(targets));
}

transaction_run_cleanup_disposition
transaction_run_cleanup_plan::disposition() const noexcept
{
  return disposition_;
}

const session_identity& transaction_run_cleanup_plan::journal() const noexcept
{
  return journal_;
}

const session_identity& transaction_run_cleanup_plan::record() const noexcept
{
  return record_;
}

bool transaction_run_cleanup_plan::eligible() const noexcept
{
  return disposition_ == transaction_run_cleanup_disposition::completed;
}

const std::vector<transaction_run_private_realization>&
transaction_run_cleanup_plan::targets() const noexcept
{
  return targets_;
}

transaction_run_cleanup_error::transaction_run_cleanup_error(
    transaction_run_cleanup_error_code code,
    int system_error,
    std::string message)
    : std::runtime_error(std::move(message)), code_(code),
      system_error_(system_error)
{
}

transaction_run_cleanup_error_code
transaction_run_cleanup_error::code() const noexcept
{
  return code_;
}

int transaction_run_cleanup_error::system_error() const noexcept
{
  return system_error_;
}

void posix_transaction_run_private_realization_cleaner::remove(
    const transaction_run_private_realization& target)
{
  auto root = open_root(target);
  if (root.get() < 0)
    return;

  const auto journal = target.relative_path().parent_path().filename().string();
  const auto dispatch = target.relative_path().filename().string();
  if (journal.empty() || dispatch.empty() ||
      target.relative_path() != std::filesystem::path(journal) / dispatch)
  {
    cleanup_failure(
        transaction_run_cleanup_error_code::target_inspect_failed,
        "private realization cleanup target is not one journal/dispatch leaf");
  }

  auto journal_directory = open_child_directory(
      root.get(), journal,
      transaction_run_cleanup_error_code::journal_open_failed,
      "private realization journal directory");
  if (journal_directory.get() < 0)
    return;
  make_opened_private_directory_removable(journal_directory.get());

  remove_directory_at(journal_directory.get(), dispatch);
  journal_directory = fd_owner();
  if (::unlinkat(root.get(), journal.c_str(), AT_REMOVEDIR) != 0 &&
      errno != ENOENT && errno != ENOTEMPTY && errno != EEXIST)
  {
    cleanup_failure(
        transaction_run_cleanup_error_code::target_remove_failed,
        "cannot prune private realization journal directory '" + journal +
            "': " + std::strerror(errno),
        errno);
  }
}

transaction_run_cleanup_result::transaction_run_cleanup_result(
    transaction_run_cleanup_plan plan,
    std::size_t cleaned,
    std::vector<transaction_run_cleanup_failure> failures)
    : plan_(std::move(plan)), cleaned_(cleaned),
      failures_(std::move(failures))
{
}

const transaction_run_cleanup_plan&
transaction_run_cleanup_result::plan() const noexcept
{
  return plan_;
}

std::size_t transaction_run_cleanup_result::cleaned() const noexcept
{
  return cleaned_;
}

const std::vector<transaction_run_cleanup_failure>&
transaction_run_cleanup_result::failures() const noexcept
{
  return failures_;
}

bool transaction_run_cleanup_result::complete() const noexcept
{
  return plan_.eligible() && cleaned_ == plan_.targets().size() &&
      failures_.empty();
}

transaction_run_cleanup_result cleanup_transaction_run_private_realizations(
    transaction_run_cleanup_plan plan,
    transaction_run_private_realization_cleaner& cleaner)
{
  std::size_t cleaned = 0U;
  std::vector<transaction_run_cleanup_failure> failures;
  if (plan.eligible())
  {
    for (const auto& target : plan.targets())
    {
      try
      {
        cleaner.remove(target);
        ++cleaned;
      }
      catch (const std::exception& problem)
      {
        failures.push_back({target, problem.what()});
      }
      catch (...)
      {
        failures.push_back({target, "unknown cleanup failure"});
      }
    }
  }
  return transaction_run_cleanup_result(
      std::move(plan), cleaned, std::move(failures));
}

} // namespace pkgctl
