// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
constexpr std::size_t maximum_remote_path = 256U;

enum class interruption_point {
  before_selection,
  after_selection,
};

[[nodiscard]] bool
parse_interruption_point(std::string_view value, interruption_point& result)
{
  if (value == "before-selection")
  {
    result = interruption_point::before_selection;
    return true;
  }
  if (value == "after-selection")
  {
    result = interruption_point::after_selection;
    return true;
  }
  return false;
}

[[nodiscard]] bool
current_temporary_name(std::string_view value) noexcept
{
  constexpr std::string_view prefix = "current.tmp.";
  return value.size() > prefix.size() &&
      value.substr(0U, prefix.size()) == prefix;
}

[[nodiscard]] bool
same_directory(pid_t child, unsigned long long fd_value, const struct stat& expected)
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

[[nodiscard]] bool
read_remote_string(pid_t child, unsigned long long address, std::string& result)
{
  result.clear();
  if (address == 0U)
    return false;

  for (std::size_t offset = 0U; offset < maximum_remote_path;
       offset += sizeof(long))
  {
    errno = 0;
    const long word = ::ptrace(
        PTRACE_PEEKDATA, child,
        reinterpret_cast<void*>(static_cast<uintptr_t>(address + offset)),
        nullptr);
    if (word == -1 && errno != 0)
      return false;

    const auto* bytes = reinterpret_cast<const unsigned char*>(&word);
    for (std::size_t index = 0U; index < sizeof(long); ++index)
    {
      if (bytes[index] == '\0')
        return true;
      result.push_back(static_cast<char>(bytes[index]));
      if (result.size() >= maximum_remote_path)
        return false;
    }
  }
  return false;
}

[[nodiscard]] bool
selection_rename(
    pid_t child, const struct user_regs_struct& registers,
    const struct stat& root_status)
{
  if (registers.orig_rax != SYS_renameat ||
      !same_directory(child, registers.rdi, root_status) ||
      !same_directory(child, registers.rdx, root_status))
  {
    return false;
  }

  std::string old_name;
  std::string new_name;
  return read_remote_string(child, registers.rsi, old_name) &&
      read_remote_string(child, registers.r10, new_name) &&
      current_temporary_name(old_name) && new_name == "current";
}

[[nodiscard]] int
child_exit_status(int status)
{
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSIGNALED(status))
  {
    std::cerr << "publication-intent-interrupt-fixture: traced command died "
              << "before the publication-selection boundary with signal "
              << WTERMSIG(status) << '\n';
  }
  return harness_failure_status;
}

[[nodiscard]] bool
resume_trace(pid_t child, int signal)
{
  if (::ptrace(PTRACE_SYSCALL, child, nullptr,
               reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0)
  {
    return true;
  }
  std::cerr << "publication-intent-interrupt-fixture: cannot resume trace: "
            << std::strerror(errno) << '\n';
  return false;
}

[[nodiscard]] int
kill_at_boundary(pid_t child, int& status)
{
  if (::kill(child, SIGKILL) != 0)
  {
    std::cerr << "publication-intent-interrupt-fixture: cannot kill traced "
              << "command: " << std::strerror(errno) << '\n';
    return harness_failure_status;
  }
  if (::waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
      WTERMSIG(status) != SIGKILL)
  {
    std::cerr << "publication-intent-interrupt-fixture: traced command did not "
              << "terminate at the requested boundary\n";
    return harness_failure_status;
  }
  return EXIT_SUCCESS;
}

} // namespace

int
main(int argc, char** argv)
{
#if !defined(__linux__) || !defined(__x86_64__)
  std::cerr << "publication-intent-interrupt-fixture: Linux x86-64 is required\n";
  return unsupported_status;
#else
  interruption_point point = interruption_point::after_selection;
  if (argc < 5 || !parse_interruption_point(argv[1], point) ||
      std::string_view(argv[3]) != "--")
  {
    std::cerr << "usage: publication-intent-interrupt-fixture "
              << "{before-selection|after-selection} CANONICAL_STORE -- "
              << "COMMAND [ARG ...]\n";
    return EXIT_FAILURE;
  }

  const int root_fd =
      ::open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (root_fd < 0)
  {
    std::cerr << "publication-intent-interrupt-fixture: cannot open canonical "
              << "store: " << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  struct stat root_status {};
  if (::fstat(root_fd, &root_status) != 0 || !S_ISDIR(root_status.st_mode))
  {
    std::cerr << "publication-intent-interrupt-fixture: invalid canonical store\n";
    static_cast<void>(::close(root_fd));
    return EXIT_FAILURE;
  }

  const pid_t child = ::fork();
  if (child < 0)
  {
    std::cerr << "publication-intent-interrupt-fixture: cannot fork: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(root_fd));
    return EXIT_FAILURE;
  }
  if (child == 0)
  {
    static_cast<void>(::close(root_fd));
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0)
    {
      std::cerr << "publication-intent-interrupt-fixture: ptrace unavailable: "
                << std::strerror(errno) << '\n';
      _exit(unsupported_status);
    }
    if (::raise(SIGSTOP) != 0)
      _exit(harness_failure_status);
    ::execvp(argv[4], argv + 4);
    std::cerr << "publication-intent-interrupt-fixture: cannot execute command: "
              << std::strerror(errno) << '\n';
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child)
  {
    std::cerr << "publication-intent-interrupt-fixture: cannot wait for initial "
              << "trace stop: " << std::strerror(errno) << '\n';
    static_cast<void>(::close(root_fd));
    return harness_failure_status;
  }
  if (WIFEXITED(status))
  {
    const int result = WEXITSTATUS(status);
    static_cast<void>(::close(root_fd));
    return result;
  }
  if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
  {
    std::cerr << "publication-intent-interrupt-fixture: child did not enter the "
              << "trace stop\n";
    static_cast<void>(::close(root_fd));
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
    static_cast<void>(::close(root_fd));
    std::cerr << "publication-intent-interrupt-fixture: ptrace unavailable: "
              << std::strerror(problem) << '\n';
    return problem == EPERM || problem == EACCES ? unsupported_status
                                                  : harness_failure_status;
  }

  bool entering_syscall = true;
  bool active_selection = false;
  long long active_syscall = -1;
  if (!resume_trace(child, 0))
  {
    static_cast<void>(::kill(child, SIGKILL));
    static_cast<void>(::waitpid(child, &status, 0));
    static_cast<void>(::close(root_fd));
    return harness_failure_status;
  }

  for (;;)
  {
    if (::waitpid(child, &status, 0) != child)
    {
      std::cerr << "publication-intent-interrupt-fixture: cannot wait for traced "
                << "command: " << std::strerror(errno) << '\n';
      static_cast<void>(::close(root_fd));
      return harness_failure_status;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      const int result = child_exit_status(status);
      static_cast<void>(::close(root_fd));
      if (result == EXIT_SUCCESS)
      {
        std::cerr << "publication-intent-interrupt-fixture: command completed "
                  << "before the publication-selection boundary\n";
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
        std::cerr << "publication-intent-interrupt-fixture: cannot inspect "
                  << "syscall: " << std::strerror(errno) << '\n';
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
        static_cast<void>(::close(root_fd));
        return harness_failure_status;
      }

      if (entering_syscall)
      {
        active_syscall = static_cast<long long>(registers.orig_rax);
        active_selection = selection_rename(child, registers, root_status);
        if (active_selection && point == interruption_point::before_selection)
        {
          const int result = kill_at_boundary(child, status);
          static_cast<void>(::close(root_fd));
          return result;
        }
      }
      else if (active_syscall == SYS_renameat && active_selection &&
               point == interruption_point::after_selection &&
               static_cast<long long>(registers.rax) == 0)
      {
        const int result = kill_at_boundary(child, status);
        static_cast<void>(::close(root_fd));
        return result;
      }

      entering_syscall = !entering_syscall;
      if (!resume_trace(child, 0))
      {
        static_cast<void>(::kill(child, SIGKILL));
        static_cast<void>(::waitpid(child, &status, 0));
        static_cast<void>(::close(root_fd));
        return harness_failure_status;
      }
      continue;
    }

    const int forwarded = signal == SIGTRAP ? 0 : signal;
    if (!resume_trace(child, forwarded))
    {
      static_cast<void>(::kill(child, SIGKILL));
      static_cast<void>(::waitpid(child, &status, 0));
      static_cast<void>(::close(root_fd));
      return harness_failure_status;
    }
  }
#endif
}
