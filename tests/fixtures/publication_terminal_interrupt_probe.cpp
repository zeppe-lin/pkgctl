// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
void write_marker(int directory, const char* name)
{
  const int fd = ::openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0)
  {
    std::cerr << "publication-terminal-interrupt-probe: marker open failed: "
              << std::strerror(errno) << '\n';
    std::exit(EXIT_FAILURE);
  }
  static_cast<void>(::close(fd));
}
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cerr << "usage: publication-terminal-interrupt-probe CANONICAL_STORE EFFECT_STORE\n";
    return EXIT_FAILURE;
  }
  const int state = ::open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  const int effects = ::open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (state < 0 || effects < 0)
    return EXIT_FAILURE;

  const std::string selector = "current.tmp." + std::to_string(static_cast<unsigned long long>(::getpid()));
  write_marker(state, selector.c_str());
  if (::renameat(state, selector.c_str(), state, "current") != 0)
    return EXIT_FAILURE;

  const std::string record(64U, 'a');
  const std::string temporary = ".tmp-effect-head-" +
      std::to_string(static_cast<unsigned long long>(::getpid())) + "-" + record;
  const std::string head = std::string(64U, 'b') + ".pjeh";
  write_marker(effects, temporary.c_str());
  if (::renameat(effects, temporary.c_str(), effects, head.c_str()) != 0)
    return EXIT_FAILURE;
  if (::fsync(effects) != 0)
    return EXIT_FAILURE;
  write_marker(state, "after-publication-terminal");
  return EXIT_SUCCESS;
}
