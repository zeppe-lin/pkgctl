// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace {
void create_empty(int directory, const std::string& name)
{
  const int fd = ::openat(
      directory, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0)
  {
    std::cerr << "lifecycle-intent-interrupt-probe: create failed: "
              << std::strerror(errno) << '\n';
    std::exit(EXIT_FAILURE);
  }
  static_cast<void>(::close(fd));
}

void publish_head(int effects, unsigned int record_index)
{
  const std::string attempt(64U, 'a');
  const char record_digit = static_cast<char>('a' + record_index - 1U);
  const std::string record(64U, record_digit);
  const std::string temporary = ".tmp-effect-head-" +
      std::to_string(static_cast<unsigned long long>(::getpid())) + "-" + record;
  const std::string head = attempt + ".pjeh";
  create_empty(effects, temporary);
  if (::renameat(effects, temporary.c_str(), effects, head.c_str()) != 0 ||
      ::fsync(effects) != 0)
  {
    std::cerr << "lifecycle-intent-interrupt-probe: head publication failed: "
              << std::strerror(errno) << '\n';
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cerr << "usage: lifecycle-intent-interrupt-probe EFFECT_STORE MARKER_STORE\n";
    return EXIT_FAILURE;
  }
  const int effects = ::open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  const int markers = ::open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (effects < 0 || markers < 0)
    return EXIT_FAILURE;

  constexpr unsigned int publications[] = {1U, 1U, 2U, 3U, 4U};
  for (unsigned int index = 0U; index < 5U; ++index)
  {
    publish_head(effects, publications[index]);
    create_empty(markers, "after-head-" + std::to_string(index + 1U));
  }
  create_empty(markers, "completed");
  return EXIT_SUCCESS;
}
