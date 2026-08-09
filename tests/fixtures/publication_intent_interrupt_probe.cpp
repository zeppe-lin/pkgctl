// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

namespace {

[[nodiscard]] bool
write_all(int fd, std::string_view text)
{
  std::size_t offset = 0U;
  while (offset < text.size())
  {
    const ssize_t written =
        ::write(fd, text.data() + offset, text.size() - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return false;
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

[[nodiscard]] bool
write_text(int directory, const char* name, std::string_view text)
{
  const int fd = ::openat(
      directory, name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
      0600);
  if (fd < 0)
    return false;
  const bool stored = write_all(fd, text);
  const int saved = errno;
  static_cast<void>(::close(fd));
  errno = saved;
  return stored;
}

} // namespace

int
main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cerr << "usage: publication-intent-interrupt-probe CANONICAL_STORE\n";
    return EXIT_FAILURE;
  }

  const int root =
      ::open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (root < 0)
  {
    std::cerr << "publication-intent-interrupt-probe: cannot open root: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }

  if (!write_text(root, "current", "old\n") ||
      !write_text(root, "unrelated.tmp", "unrelated\n") ||
      ::renameat(root, "unrelated.tmp", root, "unrelated.done") != 0 ||
      !write_text(root, "pre-selection", "present\n") ||
      !write_text(root, "current.tmp.probe", "new\n"))
  {
    std::cerr << "publication-intent-interrupt-probe: setup failed: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(root));
    return EXIT_FAILURE;
  }

  if (::renameat(root, "current.tmp.probe", root, "current") != 0)
  {
    std::cerr << "publication-intent-interrupt-probe: selection rename failed: "
              << std::strerror(errno) << '\n';
    static_cast<void>(::close(root));
    return EXIT_FAILURE;
  }

  if (!write_text(root, "post-selection", "crossed\n"))
  {
    std::cerr << "publication-intent-interrupt-probe: post-selection marker "
              << "failed: " << std::strerror(errno) << '\n';
    static_cast<void>(::close(root));
    return EXIT_FAILURE;
  }

  static_cast<void>(::close(root));
  return EXIT_SUCCESS;
}
