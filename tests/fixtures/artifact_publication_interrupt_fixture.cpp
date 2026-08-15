// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr int unsupported_status = 77;
constexpr int harness_failure_status = 125;

[[nodiscard]] bool published_artifact_exists(const fs::path& root)
{
  std::error_code ec;
  bool artifact = false;
  bool temporary = false;
  for (fs::recursive_directory_iterator it(
           root, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec))
  {
    const auto name = it->path().filename().string();
    if (name.find(".tmp.") != std::string::npos)
      temporary = true;
    if (it->is_regular_file(ec) && it->path().extension() == ".tar")
      artifact = true;
    if (ec)
      break;
  }
  return !ec && artifact && !temporary;
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
  std::cerr << "artifact-publication-interrupt-fixture: cannot resume trace: "
            << std::strerror(errno) << '\n';
  return false;
}

[[nodiscard]] int kill_at_boundary(pid_t child, int& status)
{
  if (::kill(child, SIGKILL) != 0)
  {
    std::cerr << "artifact-publication-interrupt-fixture: cannot kill traced "
                 "command: "
              << std::strerror(errno) << '\n';
    return harness_failure_status;
  }
  if (::waitpid(child, &status, 0) != child || !WIFSIGNALED(status) ||
      WTERMSIG(status) != SIGKILL)
  {
    std::cerr << "artifact-publication-interrupt-fixture: traced command did "
                 "not terminate at the requested boundary\n";
    return harness_failure_status;
  }
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
#if !defined(__linux__) || !defined(__x86_64__)
  std::cerr << "artifact-publication-interrupt-fixture: Linux x86-64 is required\n";
  return unsupported_status;
#else
  if (argc < 4 || std::string_view(argv[2]) != "--")
  {
    std::cerr << "usage: artifact-publication-interrupt-fixture ARTIFACT_ROOT "
                 "-- COMMAND [ARG ...]\n";
    return EXIT_FAILURE;
  }

  const fs::path artifact_root(argv[1]);
  std::error_code ec;
  if (!fs::is_directory(artifact_root, ec) || ec)
  {
    std::cerr << "artifact-publication-interrupt-fixture: artifact root is not "
                 "a directory\n";
    return EXIT_FAILURE;
  }

  const pid_t child = ::fork();
  if (child < 0)
  {
    std::cerr << "artifact-publication-interrupt-fixture: cannot fork: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  if (child == 0)
  {
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0)
    {
      std::cerr << "artifact-publication-interrupt-fixture: ptrace unavailable: "
                << std::strerror(errno) << '\n';
      _exit(unsupported_status);
    }
    if (::raise(SIGSTOP) != 0)
      _exit(harness_failure_status);
    ::execvp(argv[3], argv + 3);
    std::cerr << "artifact-publication-interrupt-fixture: cannot execute command: "
              << std::strerror(errno) << '\n';
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child)
  {
    terminate_trace(child);
    return harness_failure_status;
  }
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
    return harness_failure_status;

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
    std::cerr << "artifact-publication-interrupt-fixture: ptrace unavailable: "
              << std::strerror(problem) << '\n';
    return problem == EPERM || problem == EACCES ? unsupported_status
                                                  : harness_failure_status;
  }

  bool entering_syscall = true;
  long long active_syscall = -1;
  if (!resume_trace(child, 0))
  {
    terminate_trace(child);
    return harness_failure_status;
  }

  for (;;)
  {
    if (::waitpid(child, &status, 0) != child)
    {
      terminate_trace(child);
      return harness_failure_status;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      if (WIFEXITED(status) && WEXITSTATUS(status) == unsupported_status)
        return unsupported_status;
      std::cerr << "artifact-publication-interrupt-fixture: command completed "
                   "before durable artifact publication\n";
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
        terminate_trace(child);
        return harness_failure_status;
      }
      if (entering_syscall)
      {
        active_syscall = static_cast<long long>(registers.orig_rax);
      }
      else if (active_syscall >= 0 &&
               published_artifact_exists(artifact_root))
      {
        const int killed = kill_at_boundary(child, status);
        return killed;
      }
      entering_syscall = !entering_syscall;
      if (!resume_trace(child, 0))
      {
        terminate_trace(child);
        return harness_failure_status;
      }
      continue;
    }

    const int deliver = signal == SIGTRAP || signal == SIGSTOP ? 0 : signal;
    if (!resume_trace(child, deliver))
    {
      terminate_trace(child);
      return harness_failure_status;
    }
  }
#endif
}
