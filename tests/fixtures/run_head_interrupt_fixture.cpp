// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr int unsupported_status = 77;
constexpr int harness_failure_status = 125;
constexpr std::array<unsigned char, 8> head_magic{
    'P', 'K', 'G', 'R', 'U', 'N', 'H', '1'};
constexpr std::size_t sequence_offset = 8U + 2U + 32U;
constexpr std::size_t sequence_end = sequence_offset + 8U;

[[nodiscard]] bool same_directory(
    pid_t child,
    unsigned long long fd_value,
    const struct stat& expected)
{
  if (fd_value > static_cast<unsigned long long>(INT_MAX))
    return false;
  const auto fd = static_cast<int>(fd_value);
  const std::string path = "/proc/" + std::to_string(child) + "/fd/" +
      std::to_string(fd);
  struct stat actual {};
  return ::stat(path.c_str(), &actual) == 0 && S_ISDIR(actual.st_mode) &&
      actual.st_dev == expected.st_dev && actual.st_ino == expected.st_ino;
}

[[nodiscard]] bool head_name(std::string_view name) noexcept
{
  constexpr std::string_view suffix = ".pjh";
  return name.size() == 64U + suffix.size() && name.substr(64U) == suffix;
}

[[nodiscard]] bool read_exact(int fd, unsigned char* data, std::size_t size)
{
  std::size_t offset = 0U;
  while (offset < size)
  {
    const ssize_t count = ::read(fd, data + offset, size - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return false;
    offset += static_cast<std::size_t>(count);
  }
  return true;
}

[[nodiscard]] bool head_has_sequence(int directory_fd, std::uint64_t expected)
{
  const int scan_fd = ::openat(
      directory_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (scan_fd < 0)
    return false;
  DIR* directory = ::fdopendir(scan_fd);
  if (directory == nullptr)
  {
    static_cast<void>(::close(scan_fd));
    return false;
  }

  bool found = false;
  errno = 0;
  while (const auto* entry = ::readdir(directory))
  {
    if (!head_name(entry->d_name))
      continue;
    const int fd = ::openat(
        directory_fd, entry->d_name,
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
      continue;
    std::array<unsigned char, sequence_end> prefix{};
    const bool read = read_exact(fd, prefix.data(), prefix.size());
    static_cast<void>(::close(fd));
    if (!read || !std::equal(head_magic.begin(), head_magic.end(), prefix.begin()))
      continue;
    std::uint64_t sequence = 0U;
    for (std::size_t index = sequence_offset; index < sequence_end; ++index)
      sequence = (sequence << 8U) | prefix[index];
    if (sequence == expected)
    {
      found = true;
      break;
    }
  }
  const int read_error = errno;
  static_cast<void>(::closedir(directory));
  return found && read_error == 0;
}

void terminate_trace(pid_t child) noexcept
{
  static_cast<void>(::kill(child, SIGKILL));
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR)
  {
  }
}

[[nodiscard]] bool resume_trace(pid_t child, int signal)
{
  if (::ptrace(PTRACE_SYSCALL, child, nullptr,
               reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0)
    return true;
  std::cerr << "run-head-interrupt-fixture: cannot resume trace: "
            << std::strerror(errno) << '\n';
  return false;
}

[[nodiscard]] int kill_at_boundary(pid_t child, int& status)
{
  if (::kill(child, SIGKILL) != 0)
  {
    std::cerr << "run-head-interrupt-fixture: cannot kill traced command: "
              << std::strerror(errno) << '\n';
    return harness_failure_status;
  }
  if (::waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
      WTERMSIG(status) != SIGKILL)
  {
    std::cerr << "run-head-interrupt-fixture: traced command did not terminate "
                 "at the requested boundary\n";
    return harness_failure_status;
  }
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
#if !defined(__linux__) || !defined(__x86_64__)
  std::cerr << "run-head-interrupt-fixture: Linux x86-64 is required\n";
  return unsupported_status;
#else
  if (argc < 5 || std::string_view(argv[3]) != "--")
  {
    std::cerr << "usage: run-head-interrupt-fixture RUN_STORE SEQUENCE -- "
                 "COMMAND [ARG ...]\n";
    return EXIT_FAILURE;
  }

  char* end = nullptr;
  errno = 0;
  const unsigned long long parsed = std::strtoull(argv[2], &end, 10);
  if (errno != 0 || end == argv[2] || *end != '\0')
  {
    std::cerr << "run-head-interrupt-fixture: invalid sequence\n";
    return EXIT_FAILURE;
  }
  const auto target = static_cast<std::uint64_t>(parsed);

  const int directory_fd = ::open(
      argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (directory_fd < 0)
  {
    std::cerr << "run-head-interrupt-fixture: cannot open run store: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  struct stat directory_status {};
  if (::fstat(directory_fd, &directory_status) != 0 ||
      !S_ISDIR(directory_status.st_mode))
  {
    std::cerr << "run-head-interrupt-fixture: run store is not a directory\n";
    static_cast<void>(::close(directory_fd));
    return EXIT_FAILURE;
  }

  const pid_t child = ::fork();
  if (child < 0)
  {
    std::cerr << "run-head-interrupt-fixture: cannot fork: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(directory_fd));
    return EXIT_FAILURE;
  }
  if (child == 0)
  {
    static_cast<void>(::close(directory_fd));
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0)
    {
      std::cerr << "run-head-interrupt-fixture: ptrace unavailable: "
                << std::strerror(errno) << '\n';
      _exit(unsupported_status);
    }
    if (::raise(SIGSTOP) != 0)
      _exit(harness_failure_status);
    ::execvp(argv[4], argv + 4);
    std::cerr << "run-head-interrupt-fixture: cannot execute command: "
              << std::strerror(errno) << '\n';
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child)
  {
    std::cerr << "run-head-interrupt-fixture: cannot wait for initial trace "
                 "stop: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(directory_fd));
    return harness_failure_status;
  }
  if (WIFEXITED(status))
  {
    const int result = WEXITSTATUS(status);
    static_cast<void>(::close(directory_fd));
    return result;
  }
  if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
  {
    std::cerr << "run-head-interrupt-fixture: child did not enter trace stop\n";
    static_cast<void>(::close(directory_fd));
    return harness_failure_status;
  }

  long options = PTRACE_O_TRACESYSGOOD;
#ifdef PTRACE_O_EXITKILL
  options |= PTRACE_O_EXITKILL;
#endif
  if (::ptrace(PTRACE_SETOPTIONS, child, nullptr,
               reinterpret_cast<void*>(options)) != 0)
  {
    const int problem = errno;
    static_cast<void>(::kill(child, SIGKILL));
    static_cast<void>(::waitpid(child, &status, 0));
    static_cast<void>(::close(directory_fd));
    std::cerr << "run-head-interrupt-fixture: ptrace unavailable: "
              << std::strerror(problem) << '\n';
    return problem == EPERM || problem == EACCES ? unsupported_status
                                                  : harness_failure_status;
  }

  bool entering_syscall = true;
  bool active_directory_sync = false;
  long long active_syscall = -1;
  if (!resume_trace(child, 0))
  {
    terminate_trace(child);
    static_cast<void>(::close(directory_fd));
    return harness_failure_status;
  }

  for (;;)
  {
    if (::waitpid(child, &status, 0) != child)
    {
      std::cerr << "run-head-interrupt-fixture: cannot wait for traced command: "
                << std::strerror(errno) << '\n';
      terminate_trace(child);
      static_cast<void>(::close(directory_fd));
      return harness_failure_status;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      if (WIFEXITED(status) && WEXITSTATUS(status) == unsupported_status)
        return unsupported_status;
      std::cerr << "run-head-interrupt-fixture: command completed before run "
                   "head sequence "
                << target << " became durable\n";
      static_cast<void>(::close(directory_fd));
      return harness_failure_status;
    }
    if (!WIFSTOPPED(status))
      continue;

    const int signal = WSTOPSIG(status);
    if (signal == (SIGTRAP | 0x80))
    {
      struct user_regs_struct registers {};
      if (::ptrace(PTRACE_GETREGS, child, nullptr, &registers) != 0)
      {
        std::cerr << "run-head-interrupt-fixture: cannot inspect syscall: "
                  << std::strerror(errno) << '\n';
        terminate_trace(child);
        static_cast<void>(::close(directory_fd));
        return harness_failure_status;
      }
      if (entering_syscall)
      {
        active_syscall = static_cast<long long>(registers.orig_rax);
        active_directory_sync = active_syscall == SYS_fsync &&
            same_directory(child, registers.rdi, directory_status);
      }
      else if (active_syscall == SYS_fsync && active_directory_sync &&
               static_cast<long long>(registers.rax) == 0 &&
               head_has_sequence(directory_fd, target))
      {
        const int killed = kill_at_boundary(child, status);
        static_cast<void>(::close(directory_fd));
        return killed;
      }
      entering_syscall = !entering_syscall;
      if (!resume_trace(child, 0))
      {
        terminate_trace(child);
        static_cast<void>(::close(directory_fd));
        return harness_failure_status;
      }
      continue;
    }

    const int deliver = signal == SIGTRAP || signal == SIGSTOP ? 0 : signal;
    if (!resume_trace(child, deliver))
    {
      terminate_trace(child);
      static_cast<void>(::close(directory_fd));
      return harness_failure_status;
    }
  }
#endif
}
