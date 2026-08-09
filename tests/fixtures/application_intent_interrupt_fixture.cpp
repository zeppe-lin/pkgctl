// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string_view>
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

[[nodiscard]] bool is_hexadecimal(std::string_view value) noexcept
{
  for (const char character : value)
  {
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f')))
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool active_reference_name(std::string_view name) noexcept
{
  constexpr std::string_view prefix = "active-request-v1-sha256-";
  constexpr std::string_view suffix = ".ref";
  if (name.size() != prefix.size() + 64U + suffix.size() ||
      name.substr(0U, prefix.size()) != prefix ||
      name.substr(name.size() - suffix.size()) != suffix)
  {
    return false;
  }
  return is_hexadecimal(name.substr(prefix.size(), 64U));
}

[[nodiscard]] bool has_active_reference(int directory_fd)
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
    if (active_reference_name(entry->d_name))
    {
      found = true;
      break;
    }
  }
  const int read_error = errno;
  static_cast<void>(::closedir(directory));
  return found && read_error == 0;
}

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

[[nodiscard]] int child_exit_status(int status)
{
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSIGNALED(status))
  {
    std::cerr << "application-intent-interrupt-fixture: traced command died "
              << "before the interruption boundary with signal "
              << WTERMSIG(status) << '\n';
  }
  return harness_failure_status;
}

[[nodiscard]] bool resume_trace(pid_t child, int signal)
{
  if (::ptrace(PTRACE_SYSCALL, child, nullptr,
               reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0)
  {
    return true;
  }
  std::cerr << "application-intent-interrupt-fixture: cannot resume trace: "
            << std::strerror(errno) << '\n';
  return false;
}

} // namespace

int main(int argc, char** argv)
{
#if !defined(__linux__) || !defined(__x86_64__)
  std::cerr << "application-intent-interrupt-fixture: Linux x86-64 is required\n";
  return unsupported_status;
#else
  if (argc < 4 || std::string_view(argv[2]) != "--")
  {
    std::cerr << "usage: application-intent-interrupt-fixture "
              << "APPLICATION_JOURNAL_DIRECTORY -- COMMAND [ARG ...]\n";
    return EXIT_FAILURE;
  }

  const int directory_fd = ::open(
      argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (directory_fd < 0)
  {
    std::cerr << "application-intent-interrupt-fixture: cannot open application "
              << "journal directory: " << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  struct stat directory_status {};
  if (::fstat(directory_fd, &directory_status) != 0 ||
      !S_ISDIR(directory_status.st_mode))
  {
    std::cerr << "application-intent-interrupt-fixture: invalid application "
              << "journal directory\n";
    static_cast<void>(::close(directory_fd));
    return EXIT_FAILURE;
  }

  const pid_t child = ::fork();
  if (child < 0)
  {
    std::cerr << "application-intent-interrupt-fixture: cannot fork: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(directory_fd));
    return EXIT_FAILURE;
  }
  if (child == 0)
  {
    static_cast<void>(::close(directory_fd));
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0)
    {
      std::cerr << "application-intent-interrupt-fixture: ptrace unavailable: "
                << std::strerror(errno) << '\n';
      _exit(unsupported_status);
    }
    if (::raise(SIGSTOP) != 0)
      _exit(harness_failure_status);
    ::execvp(argv[3], argv + 3);
    std::cerr << "application-intent-interrupt-fixture: cannot execute command: "
              << std::strerror(errno) << '\n';
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child)
  {
    std::cerr << "application-intent-interrupt-fixture: cannot wait for "
              << "initial trace stop: " << std::strerror(errno) << '\n';
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
    std::cerr << "application-intent-interrupt-fixture: child did not enter "
              << "the trace stop\n";
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
    std::cerr << "application-intent-interrupt-fixture: ptrace unavailable: "
              << std::strerror(problem) << '\n';
    return problem == EPERM || problem == EACCES
        ? unsupported_status
        : harness_failure_status;
  }

  bool entering_syscall = true;
  long long active_syscall = -1;
  unsigned long long active_fd = 0U;
  if (!resume_trace(child, 0))
  {
    static_cast<void>(::kill(child, SIGKILL));
    static_cast<void>(::waitpid(child, &status, 0));
    static_cast<void>(::close(directory_fd));
    return harness_failure_status;
  }

  for (;;)
  {
    if (::waitpid(child, &status, 0) != child)
    {
      std::cerr << "application-intent-interrupt-fixture: cannot wait for "
                << "traced command: " << std::strerror(errno) << '\n';
      static_cast<void>(::close(directory_fd));
      return harness_failure_status;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      const int result = child_exit_status(status);
      static_cast<void>(::close(directory_fd));
      if (result == EXIT_SUCCESS)
      {
        std::cerr << "application-intent-interrupt-fixture: command completed "
                  << "before the durable application-journal boundary\n";
        return harness_failure_status;
      }
      return result;
    }
    if (!WIFSTOPPED(status))
      continue;

    const int signal = WSTOPSIG(status);
    if (signal == (SIGTRAP | 0x80))
    {
      struct user_regs_struct registers {};
      if (::ptrace(PTRACE_GETREGS, child, nullptr, &registers) != 0)
      {
        std::cerr << "application-intent-interrupt-fixture: cannot inspect "
                  << "syscall: " << std::strerror(errno) << '\n';
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
        static_cast<void>(::close(directory_fd));
        return harness_failure_status;
      }

      if (entering_syscall)
      {
        active_syscall = static_cast<long long>(registers.orig_rax);
        active_fd = registers.rdi;
      }
      else if (active_syscall == SYS_fsync &&
               static_cast<long long>(registers.rax) == 0 &&
               same_directory(child, active_fd, directory_status) &&
               has_active_reference(directory_fd))
      {
        if (::kill(child, SIGKILL) != 0)
        {
          std::cerr << "application-intent-interrupt-fixture: cannot kill "
                    << "traced command: " << std::strerror(errno) << '\n';
          static_cast<void>(::close(directory_fd));
          return harness_failure_status;
        }
        if (::waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
            WTERMSIG(status) != SIGKILL)
        {
          std::cerr << "application-intent-interrupt-fixture: traced command "
                    << "did not terminate at the requested boundary\n";
          static_cast<void>(::close(directory_fd));
          return harness_failure_status;
        }
        static_cast<void>(::close(directory_fd));
        return EXIT_SUCCESS;
      }
      entering_syscall = !entering_syscall;
      if (!resume_trace(child, 0))
      {
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
        static_cast<void>(::close(directory_fd));
        return harness_failure_status;
      }
      continue;
    }

    const int forwarded = signal == SIGTRAP ? 0 : signal;
    if (!resume_trace(child, forwarded))
    {
      static_cast<void>(::kill(child, SIGKILL));
      static_cast<void>(::waitpid(child, &status, 0));
      static_cast<void>(::close(directory_fd));
      return harness_failure_status;
    }
  }
#endif
}
