// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
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
constexpr int harness_failure_status = 78;
constexpr std::size_t maximum_remote_path = 512U;

[[nodiscard]] bool same_directory(
    pid_t child,
    unsigned long long fd_value,
    const struct stat& expected)
{
  const int fd = static_cast<int>(fd_value);
  const std::string path = "/proc/" + std::to_string(child) + "/fd/" +
      std::to_string(fd);
  struct stat actual {};
  return ::stat(path.c_str(), &actual) == 0 && S_ISDIR(actual.st_mode) &&
      actual.st_dev == expected.st_dev && actual.st_ino == expected.st_ino;
}

[[nodiscard]] bool read_remote_string(
    pid_t child,
    unsigned long long address,
    std::string& result)
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

[[nodiscard]] bool effect_head_temporary_name(std::string_view value) noexcept
{
  constexpr std::string_view prefix = ".tmp-effect-head-";
  return value.size() > prefix.size() && value.substr(0U, prefix.size()) == prefix;
}

[[nodiscard]] bool hexadecimal(char value) noexcept
{
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

[[nodiscard]] bool effect_head_name(std::string_view value) noexcept
{
  constexpr std::string_view suffix = ".pjeh";
  if (value.size() != 64U + suffix.size() || value.substr(64U) != suffix)
    return false;
  for (std::size_t index = 0U; index < 64U; ++index)
  {
    if (!hexadecimal(value[index]))
      return false;
  }
  return true;
}

[[nodiscard]] bool head_rename_in_directory(
    pid_t child,
    const struct user_regs_struct& registers,
    const struct stat& directory)
{
  if (registers.orig_rax != SYS_renameat ||
      !same_directory(child, registers.rdi, directory) ||
      !same_directory(child, registers.rdx, directory))
    return false;

  std::string old_name;
  std::string new_name;
  return read_remote_string(child, registers.rsi, old_name) &&
      read_remote_string(child, registers.r10, new_name) &&
      effect_head_temporary_name(old_name) && effect_head_name(new_name);
}

[[nodiscard]] bool resume_trace(pid_t child, int signal)
{
  if (::ptrace(PTRACE_SYSCALL, child, nullptr,
               reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0)
    return true;
  std::cerr << "lifecycle-intent-interrupt-fixture: cannot resume trace: "
            << std::strerror(errno) << '\n';
  return false;
}

[[nodiscard]] int kill_at_boundary(pid_t child, int& status)
{
  if (::kill(child, SIGKILL) != 0)
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: cannot kill traced command: "
              << std::strerror(errno) << '\n';
    return harness_failure_status;
  }
  if (::waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
      WTERMSIG(status) != SIGKILL)
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: traced command did not terminate at the requested boundary\n";
    return harness_failure_status;
  }
  return EXIT_SUCCESS;
}

[[nodiscard]] std::size_t requested_head_commit(std::string_view mode)
{
  if (mode == "before-lifecycle-intent")
    return 2U;
  if (mode == "after-lifecycle-intent")
    return 4U;
  return 0U;
}
} // namespace

int main(int argc, char** argv)
{
#if !defined(__linux__) || !defined(__x86_64__)
  std::cerr << "lifecycle-intent-interrupt-fixture: Linux x86-64 is required\n";
  return unsupported_status;
#else
  if (argc < 5 || std::string_view(argv[3]) != "--")
  {
    std::cerr << "usage: lifecycle-intent-interrupt-fixture MODE EFFECT_STORE -- COMMAND [ARG ...]\n";
    return EXIT_FAILURE;
  }

  const std::size_t requested_commit = requested_head_commit(argv[1]);
  if (requested_commit == 0U)
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: unknown mode\n";
    return EXIT_FAILURE;
  }

  const int effect_fd = ::open(
      argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (effect_fd < 0)
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: cannot open effect store: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  struct stat effect_status {};
  if (::fstat(effect_fd, &effect_status) != 0 || !S_ISDIR(effect_status.st_mode))
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: effect store is not a directory\n";
    static_cast<void>(::close(effect_fd));
    return EXIT_FAILURE;
  }

  const pid_t child = ::fork();
  if (child < 0)
  {
    std::cerr << "lifecycle-intent-interrupt-fixture: cannot fork: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(effect_fd));
    return EXIT_FAILURE;
  }
  if (child == 0)
  {
    static_cast<void>(::close(effect_fd));
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0)
    {
      std::cerr << "lifecycle-intent-interrupt-fixture: ptrace unavailable: "
                << std::strerror(errno) << '\n';
      _exit(unsupported_status);
    }
    if (::raise(SIGSTOP) != 0)
      _exit(harness_failure_status);
    ::execvp(argv[4], argv + 4);
    std::cerr << "lifecycle-intent-interrupt-fixture: cannot execute command: "
              << std::strerror(errno) << '\n';
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child || !WIFSTOPPED(status) ||
      WSTOPSIG(status) != SIGSTOP)
  {
    if (WIFEXITED(status))
    {
      const int result = WEXITSTATUS(status);
      static_cast<void>(::close(effect_fd));
      return result;
    }
    std::cerr << "lifecycle-intent-interrupt-fixture: child did not enter the trace stop\n";
    static_cast<void>(::close(effect_fd));
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
    static_cast<void>(::close(effect_fd));
    std::cerr << "lifecycle-intent-interrupt-fixture: ptrace unavailable: "
              << std::strerror(problem) << '\n';
    return problem == EPERM || problem == EACCES ? unsupported_status
                                                  : harness_failure_status;
  }

  bool entering_syscall = true;
  bool active_head_rename = false;
  bool active_effect_sync = false;
  bool head_published = false;
  long long active_syscall = -1;
  std::size_t committed_heads = 0U;

  if (!resume_trace(child, 0))
    return harness_failure_status;

  for (;;)
  {
    if (::waitpid(child, &status, 0) != child)
    {
      std::cerr << "lifecycle-intent-interrupt-fixture: cannot wait for traced command: "
                << std::strerror(errno) << '\n';
      return harness_failure_status;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      if (WIFEXITED(status) && WEXITSTATUS(status) == unsupported_status)
        return unsupported_status;
      std::cerr << "lifecycle-intent-interrupt-fixture: command completed before requested durable effect-head boundary\n";
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
        std::cerr << "lifecycle-intent-interrupt-fixture: cannot inspect syscall: "
                  << std::strerror(errno) << '\n';
        return harness_failure_status;
      }

      if (entering_syscall)
      {
        active_syscall = static_cast<long long>(registers.orig_rax);
        active_head_rename = false;
        active_effect_sync = false;
        if (active_syscall == SYS_renameat)
          active_head_rename =
              head_rename_in_directory(child, registers, effect_status);
        else if (active_syscall == SYS_fsync && head_published)
          active_effect_sync =
              same_directory(child, registers.rdi, effect_status);
      }
      else
      {
        const auto result = static_cast<long long>(registers.rax);
        if (active_syscall == SYS_renameat && active_head_rename && result == 0)
          head_published = true;
        else if (active_syscall == SYS_fsync && active_effect_sync && result == 0)
        {
          head_published = false;
          ++committed_heads;
          if (committed_heads == requested_commit)
          {
            const int killed = kill_at_boundary(child, status);
            static_cast<void>(::close(effect_fd));
            return killed;
          }
        }
      }
      entering_syscall = !entering_syscall;
      if (!resume_trace(child, 0))
        return harness_failure_status;
      continue;
    }

    const int deliver = signal == SIGTRAP || signal == SIGSTOP ? 0 : signal;
    if (!resume_trace(child, deliver))
      return harness_failure_status;
  }
#endif
}
