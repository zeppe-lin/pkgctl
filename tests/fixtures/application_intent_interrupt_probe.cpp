// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

constexpr const char* reference_name =
    "active-request-v1-sha256-"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.ref";
constexpr const char* reference_body =
    "v1:sha256:"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";

[[nodiscard]] bool write_all(int fd, const char* data, std::size_t size)
{
  std::size_t offset = 0U;
  while (offset < size)
  {
    const ssize_t written = ::write(fd, data + offset, size - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return false;
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

[[nodiscard]] bool synchronize_directory(const std::string& directory)
{
  const int fd = ::open(
      directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return false;
  const bool result = ::fsync(fd) == 0;
  static_cast<void>(::close(fd));
  return result;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cerr << "usage: application-intent-interrupt-probe "
              << "DIRECTORY POST_SYNC_MARKER\n";
    return EXIT_FAILURE;
  }

  const std::string directory(argv[1]);
  const std::string marker(argv[2]);
  const std::string temporary = directory + "/.active-reference.tmp";
  const std::string reference = directory + "/" + reference_name;

  if (!synchronize_directory(directory))
  {
    std::cerr << "application-intent-interrupt-probe: initial directory sync "
              << "failed: " << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }

  const int file = ::open(
      temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
      0600);
  if (file < 0)
  {
    std::cerr << "application-intent-interrupt-probe: cannot create reference: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  const bool stored = write_all(file, reference_body, std::strlen(reference_body)) &&
      ::fsync(file) == 0;
  const int close_result = ::close(file);
  if (!stored || close_result != 0)
  {
    std::cerr << "application-intent-interrupt-probe: cannot synchronize "
              << "reference\n";
    return EXIT_FAILURE;
  }
  if (::rename(temporary.c_str(), reference.c_str()) != 0)
  {
    std::cerr << "application-intent-interrupt-probe: cannot publish reference: "
              << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  if (!synchronize_directory(directory))
  {
    std::cerr << "application-intent-interrupt-probe: final directory sync "
              << "failed: " << std::strerror(errno) << '\n';
    return EXIT_FAILURE;
  }

  const int marker_fd = ::open(
      marker.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
      0600);
  if (marker_fd < 0 || ::close(marker_fd) != 0)
  {
    std::cerr << "application-intent-interrupt-probe: cannot create post-sync "
              << "marker\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
