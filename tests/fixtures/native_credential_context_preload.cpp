// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grp.h>
#include <limits>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr const char* uid_variable = "PKGCTL_TEST_SUPERVISOR_UID";
constexpr const char* gid_variable = "PKGCTL_TEST_SUPERVISOR_GID";
constexpr const char* previous_preload_variable =
    "PKGCTL_TEST_PREVIOUS_LD_PRELOAD";

[[noreturn]] void fail(const char* operation)
{
  char buffer[256]{};
  const auto count = std::snprintf(
      buffer, sizeof(buffer),
      "native-credential-context-preload: %s: %s\n", operation,
      std::strerror(errno));
  if (count > 0)
  {
    const auto length = static_cast<std::size_t>(count) < sizeof(buffer)
        ? static_cast<std::size_t>(count)
        : sizeof(buffer) - 1U;
    (void)::write(STDERR_FILENO, buffer, length);
  }
  ::_exit(126);
}

[[noreturn]] void fail_value(const char* name)
{
  char buffer[192]{};
  const auto count = std::snprintf(
      buffer, sizeof(buffer),
      "native-credential-context-preload: invalid %s\n", name);
  if (count > 0)
  {
    const auto length = static_cast<std::size_t>(count) < sizeof(buffer)
        ? static_cast<std::size_t>(count)
        : sizeof(buffer) - 1U;
    (void)::write(STDERR_FILENO, buffer, length);
  }
  ::_exit(126);
}

template <typename Id>
[[nodiscard]] Id parse_id(const char* text, const char* name)
{
  if (text == nullptr || *text == '\0')
    fail_value(name);

  errno = 0;
  char* end = nullptr;
  const auto value = std::strtoull(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' ||
      value > static_cast<unsigned long long>(std::numeric_limits<Id>::max()))
    fail_value(name);
  return static_cast<Id>(value);
}

void restore_loader_environment()
{
  const char* previous = std::getenv(previous_preload_variable);
  if (previous != nullptr)
  {
    if (::setenv("LD_PRELOAD", previous, 1) != 0)
      fail("restore LD_PRELOAD");
  }
  else if (::unsetenv("LD_PRELOAD") != 0)
  {
    fail("clear LD_PRELOAD");
  }

  if (::unsetenv(uid_variable) != 0 || ::unsetenv(gid_variable) != 0 ||
      ::unsetenv(previous_preload_variable) != 0)
    fail("clear credential-context environment");
}

} // namespace

extern "C" __attribute__((constructor)) void pkgctl_test_drop_supervisor_credentials()
{
  const auto uid = parse_id<uid_t>(std::getenv(uid_variable), "uid");
  const auto gid = parse_id<gid_t>(std::getenv(gid_variable), "gid");

  // The loader has already mapped the executable dependency closure. Remove the
  // test preload contract before pkgctl can launch any subordinate process.
  restore_loader_environment();

  if (::setgroups(0, nullptr) != 0)
    fail("setgroups");
  if (::setgid(gid) != 0)
    fail("setgid");
  if (::setuid(uid) != 0)
    fail("setuid");
}
