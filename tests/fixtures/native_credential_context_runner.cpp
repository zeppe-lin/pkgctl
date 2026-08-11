// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace {

[[nodiscard]] unsigned long long parse_id(const char* text, const char* name)
{
  errno = 0;
  char* end = nullptr;
  const auto value = std::strtoull(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0')
  {
    std::cerr << "native-credential-context-runner: invalid " << name << '\n';
    std::exit(2);
  }
  return value;
}

[[noreturn]] void fail(const char* operation)
{
  std::cerr << "native-credential-context-runner: " << operation << ": "
            << std::strerror(errno) << '\n';
  std::exit(1);
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 5)
  {
    std::cerr << "usage: native-credential-context-runner UID GID PRELOAD PROGRAM [ARG ...]\n";
    return 2;
  }

  const auto uid_value = parse_id(argv[1], "uid");
  const auto gid_value = parse_id(argv[2], "gid");
  if (uid_value > static_cast<unsigned long long>(
                      std::numeric_limits<uid_t>::max()) ||
      gid_value > static_cast<unsigned long long>(
                      std::numeric_limits<gid_t>::max()))
  {
    std::cerr << "native-credential-context-runner: uid/gid is too large\n";
    return 2;
  }

  const char* previous_preload = std::getenv("LD_PRELOAD");
  if (previous_preload != nullptr && *previous_preload != '\0')
  {
    if (::setenv("PKGCTL_TEST_PREVIOUS_LD_PRELOAD", previous_preload, 1) != 0)
      fail("retain LD_PRELOAD");
  }
  else if (::unsetenv("PKGCTL_TEST_PREVIOUS_LD_PRELOAD") != 0)
  {
    fail("clear retained LD_PRELOAD");
  }

  std::string preload = argv[3];
  if (previous_preload != nullptr && *previous_preload != '\0')
    preload += ":" + std::string(previous_preload);

  if (::setenv("LD_PRELOAD", preload.c_str(), 1) != 0 ||
      ::setenv("PKGCTL_TEST_SUPERVISOR_UID", argv[1], 1) != 0 ||
      ::setenv("PKGCTL_TEST_SUPERVISOR_GID", argv[2], 1) != 0)
    fail("prepare credential-context environment");

  // Exec while the original supervisor still owns loader/path authority. The
  // preload constructor drops credentials after the loader has mapped pkgctl.
  ::execv(argv[4], &argv[4]);
  fail("execv");
}
