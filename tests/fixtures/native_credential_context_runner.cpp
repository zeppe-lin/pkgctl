// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <grp.h>
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
  if (argc < 4)
  {
    std::cerr << "usage: native-credential-context-runner UID GID PROGRAM [ARG ...]\n";
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

  if (::setgroups(0, nullptr) != 0)
    fail("setgroups");
  if (::setgid(static_cast<gid_t>(gid_value)) != 0)
    fail("setgid");
  if (::setuid(static_cast<uid_t>(uid_value)) != 0)
    fail("setuid");
  ::execv(argv[3], &argv[3]);
  fail("execv");
}
