// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr std::string_view ready_name = ".pkgctl-test-lease-loss-ready";
constexpr std::string_view acknowledge_name = ".pkgctl-test-lease-loss-ack";

class fd_guard final {
public:
  explicit fd_guard(int value = -1) noexcept : value_(value) {}
  fd_guard(const fd_guard&) = delete;
  fd_guard& operator=(const fd_guard&) = delete;
  fd_guard(fd_guard&& other) noexcept : value_(other.value_)
  {
    other.value_ = -1;
  }
  fd_guard& operator=(fd_guard&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      value_ = other.value_;
      other.value_ = -1;
    }
    return *this;
  }
  ~fd_guard()
  {
    reset();
  }

  [[nodiscard]] int get() const noexcept { return value_; }

private:
  void reset() noexcept
  {
    if (value_ >= 0)
      static_cast<void>(::close(value_));
    value_ = -1;
  }

  int value_;
};

class fifo_pair final {
public:
  explicit fifo_pair(int target_fd) : target_fd_(target_fd)
  {
    create(ready_name);
    try
    {
      create(acknowledge_name);
    }
    catch (...)
    {
      remove(ready_name);
      throw;
    }
  }

  fifo_pair(const fifo_pair&) = delete;
  fifo_pair& operator=(const fifo_pair&) = delete;

  ~fifo_pair()
  {
    remove(acknowledge_name);
    remove(ready_name);
  }

private:
  void create(std::string_view name)
  {
    const std::string path(name);
    if (::mkfifoat(target_fd_, path.c_str(), 0600) != 0)
      throw std::runtime_error(
          "cannot create lifecycle synchronization fifo: " +
          std::string(std::strerror(errno)));
  }

  void remove(std::string_view name) noexcept
  {
    const std::string path(name);
    if (::unlinkat(target_fd_, path.c_str(), 0) != 0 && errno != ENOENT)
    {
      std::cerr << "native-target-lock-revoker: cannot remove fifo " << path
                << ": " << std::strerror(errno) << '\n';
    }
  }

  int target_fd_;
};

[[nodiscard]] std::filesystem::path normalized_absolute(
    const char* value,
    std::string_view role)
{
  std::filesystem::path path(value);
  if (!path.is_absolute() || path.empty() || path != path.lexically_normal())
    throw std::invalid_argument(
        std::string(role) + " must be an absolute normalized path");
  return path;
}

void read_signal(int fd)
{
  char byte = 0;
  for (;;)
  {
    const auto count = ::read(fd, &byte, 1U);
    if (count > 0)
      return;
    if (count == 0)
      throw std::runtime_error("lifecycle synchronization fifo closed early");
    if (errno != EINTR)
      throw std::runtime_error(
          "cannot read lifecycle synchronization fifo: " +
          std::string(std::strerror(errno)));
  }
}

void write_all(int fd, std::string_view value)
{
  std::size_t offset = 0U;
  while (offset < value.size())
  {
    const auto count = ::write(fd, value.data() + offset, value.size() - offset);
    if (count > 0)
    {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    throw std::runtime_error(
        "cannot write lifecycle synchronization fifo: " +
        std::string(std::strerror(errno)));
  }
}

[[nodiscard]] std::vector<std::string> lock_entries(int lock_fd)
{
  const int scan_fd = ::dup(lock_fd);
  if (scan_fd < 0)
    throw std::runtime_error(
        "cannot duplicate target-lock directory: " +
        std::string(std::strerror(errno)));

  DIR* raw = ::fdopendir(scan_fd);
  if (raw == nullptr)
  {
    static_cast<void>(::close(scan_fd));
    throw std::runtime_error(
        "cannot enumerate target-lock directory: " +
        std::string(std::strerror(errno)));
  }

  std::vector<std::string> entries;
  errno = 0;
  while (const auto* entry = ::readdir(raw))
  {
    const std::string_view name(entry->d_name);
    if (name == "." || name == "..")
      continue;

    struct stat status {};
    if (::fstatat(lock_fd, entry->d_name, &status, AT_SYMLINK_NOFOLLOW) != 0)
    {
      const auto problem = std::string(std::strerror(errno));
      static_cast<void>(::closedir(raw));
      throw std::runtime_error("cannot inspect target-lock entry: " + problem);
    }
    if (!S_ISREG(status.st_mode))
    {
      static_cast<void>(::closedir(raw));
      throw std::runtime_error("target-lock directory contains non-file entry");
    }
    entries.emplace_back(entry->d_name);
  }
  if (errno != 0)
  {
    const auto problem = std::string(std::strerror(errno));
    static_cast<void>(::closedir(raw));
    throw std::runtime_error("cannot enumerate target-lock directory: " + problem);
  }
  static_cast<void>(::closedir(raw));
  return entries;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::cerr << "usage: native-target-lock-revoker LOCK_ROOT TARGET_ROOT\n";
    return EXIT_FAILURE;
  }

  try {
    const auto lock_root = normalized_absolute(argv[1], "lock root");
    const auto target_root = normalized_absolute(argv[2], "target root");
    if (lock_root.filename() != "target-locks")
      throw std::invalid_argument("lock root is not a target-locks authority");

    fd_guard lock_fd(::open(
        lock_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (lock_fd.get() < 0)
      throw std::runtime_error(
          "cannot open target-lock directory: " +
          std::string(std::strerror(errno)));

    fd_guard target_fd(::open(
        target_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (target_fd.get() < 0)
      throw std::runtime_error(
          "cannot open target root: " + std::string(std::strerror(errno)));

    fifo_pair synchronization(target_fd.get());
    std::cout << "ready\n" << std::flush;

    const std::string ready_path(ready_name);
    fd_guard ready_fd(::openat(
        target_fd.get(), ready_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (ready_fd.get() < 0)
      throw std::runtime_error(
          "cannot open lifecycle-ready fifo: " +
          std::string(std::strerror(errno)));
    read_signal(ready_fd.get());

    const auto entries = lock_entries(lock_fd.get());
    if (entries.size() != 1U)
      throw std::runtime_error(
          "target-lock directory does not contain exactly one anchored lease");
    if (::unlinkat(lock_fd.get(), entries.front().c_str(), 0) != 0)
      throw std::runtime_error(
          "cannot revoke anchored target lock: " +
          std::string(std::strerror(errno)));

    const std::string acknowledge_path(acknowledge_name);
    fd_guard acknowledge_fd(::openat(
        target_fd.get(), acknowledge_path.c_str(),
        O_WRONLY | O_CLOEXEC | O_NOFOLLOW));
    if (acknowledge_fd.get() < 0)
      throw std::runtime_error(
          "cannot open lifecycle-acknowledgement fifo: " +
          std::string(std::strerror(errno)));
    write_all(acknowledge_fd.get(), "revoked\n");

    std::cout << "revoked " << entries.front() << '\n' << std::flush;
    return EXIT_SUCCESS;
  }
  catch (const std::exception& problem)
  {
    std::cerr << "native-target-lock-revoker: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
}
