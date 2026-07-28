// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_store.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

class fd_guard final {
public:
  explicit fd_guard(int fd = -1) noexcept : fd_(fd) {}
  fd_guard(const fd_guard&) = delete;
  fd_guard& operator=(const fd_guard&) = delete;
  fd_guard(fd_guard&& other) noexcept : fd_(other.release()) {}
  fd_guard& operator=(fd_guard&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      fd_ = other.release();
    }
    return *this;
  }
  ~fd_guard() { reset(); }
  [[nodiscard]] int get() const noexcept { return fd_; }
  [[nodiscard]] int release() noexcept
  {
    const int value = fd_;
    fd_ = -1;
    return value;
  }
  void reset(int fd = -1) noexcept
  {
    if (fd_ >= 0)
      (void)::close(fd_);
    fd_ = fd;
  }
private:
  int fd_;
};

class directory_guard final {
public:
  explicit directory_guard(DIR* directory) : directory_(directory) {}
  directory_guard(const directory_guard&) = delete;
  directory_guard& operator=(const directory_guard&) = delete;
  ~directory_guard()
  {
    if (directory_ != nullptr)
      (void)::closedir(directory_);
  }
  [[nodiscard]] DIR* get() const noexcept { return directory_; }
private:
  DIR* directory_;
};

[[noreturn]] void io_error(effect_journal_error_code code,
                           const std::string& message)
{
  const int value = errno;
  throw effect_journal_error(code,
                             message + ": " + std::strerror(value), value);
}

bool lowercase_hex(std::string_view value)
{
  if (value.size() != 64U)
    return false;
  return std::all_of(value.begin(), value.end(), [](const char digit) {
    return (digit >= '0' && digit <= '9') ||
           (digit >= 'a' && digit <= 'f');
  });
}

std::string sequence_text(std::uint64_t sequence)
{
  std::array<char, 32> buffer{};
  const int length = std::snprintf(
      buffer.data(), buffer.size(), "%020llu",
      static_cast<unsigned long long>(sequence));
  if (length != 20)
    throw effect_journal_error(
        effect_journal_error_code::invalid_record,
        "cannot encode journal sequence");
  return std::string(buffer.data(), static_cast<std::size_t>(length));
}

std::string record_name(const effect_attempt_record& record)
{
  return record.attempt().hex() + "-" + sequence_text(record.sequence()) +
         "-" + record.identity().hex() + ".pje";
}

struct parsed_name final {
  std::uint64_t sequence;
  std::string identity;
};

std::optional<parsed_name> parse_name(
    std::string_view name, const session_identity& attempt)
{
  constexpr std::size_t suffix_size = 4U;
  const std::string prefix = attempt.hex() + "-";
  if (name.size() != prefix.size() + 20U + 1U + 64U + suffix_size ||
      name.substr(0U, prefix.size()) != prefix ||
      name.substr(name.size() - suffix_size) != ".pje")
    return std::nullopt;
  const auto sequence_part = name.substr(prefix.size(), 20U);
  if (!std::all_of(sequence_part.begin(), sequence_part.end(),
                   [](const char digit) { return digit >= '0' && digit <= '9'; }))
    return std::nullopt;
  std::uint64_t sequence = 0U;
  for (const char digit : sequence_part)
  {
    const auto value = static_cast<unsigned int>(digit - '0');
    if (sequence >
        (std::numeric_limits<std::uint64_t>::max() - value) / 10U)
      return std::nullopt;
    sequence = sequence * 10U + value;
  }
  const auto identity = name.substr(prefix.size() + 21U, 64U);
  if (!lowercase_hex(identity))
    return std::nullopt;
  return parsed_name{sequence, std::string(identity)};
}

fd_guard lock_store(int directory_fd)
{
  fd_guard lock(::openat(directory_fd, ".pkgctl-effect.lock",
                         O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (lock.get() < 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot open controller journal lock");
  if (::flock(lock.get(), LOCK_EX) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot acquire controller journal lock");
  return lock;
}

effect_attempt_encoding read_encoding(int directory_fd, const std::string& name)
{
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (file.get() < 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot open controller journal snapshot");
  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot inspect controller journal snapshot");
  if (!S_ISREG(status.st_mode) || (status.st_mode & 0222) != 0 ||
      status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) >
          maximum_effect_attempt_encoding_size)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "controller journal snapshot has invalid type or size");
  effect_attempt_encoding encoding(static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0U;
  while (offset < encoding.size())
  {
    const ssize_t count = ::read(file.get(), encoding.data() + offset,
                                 encoding.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(effect_journal_error_code::store_read_failed,
               "cannot read controller journal snapshot");
    }
    if (count == 0)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal snapshot ended early");
    offset += static_cast<std::size_t>(count);
  }
  return encoding;
}

std::optional<effect_attempt_record> load_latest_unlocked(
    int directory_fd, const session_identity& attempt)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot duplicate controller journal directory");
  directory_guard directory(::fdopendir(duplicate));
  if (directory.get() == nullptr)
  {
    (void)::close(duplicate);
    io_error(effect_journal_error_code::store_read_failed,
             "cannot enumerate controller journal directory");
  }
  ::rewinddir(directory.get());

  std::map<std::uint64_t, effect_attempt_record> records;
  errno = 0;
  while (dirent* entry = ::readdir(directory.get()))
  {
    const auto parsed = parse_name(entry->d_name, attempt);
    if (!parsed)
      continue;
    effect_attempt_record record = decode_effect_attempt_record(
        read_encoding(directory_fd, entry->d_name));
    if (record.attempt() != attempt ||
        record.sequence() != parsed->sequence ||
        record.identity().hex() != parsed->identity)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal filename and content disagree");
    if (!records.emplace(record.sequence(), std::move(record)).second)
      throw effect_journal_error(
          effect_journal_error_code::store_conflict,
          "controller journal contains a sequence fork");
  }
  if (errno != 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot enumerate controller journal directory");
  if (records.empty())
    return std::nullopt;

  const effect_attempt_record* previous = nullptr;
  std::uint64_t expected_sequence = 0U;
  for (const auto& [sequence, record] : records)
  {
    if (sequence != expected_sequence)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal sequence has a gap");
    if (previous == nullptr)
    {
      if (record.previous())
        throw effect_journal_error(
            effect_journal_error_code::store_corrupt,
            "controller journal admission has a predecessor");
    }
    else if (!record.previous() ||
             *record.previous() != previous->identity())
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal predecessor chain is invalid");
    previous = &record;
    ++expected_sequence;
  }
  return records.rbegin()->second;
}

void write_all(int fd, const effect_attempt_encoding& encoding)
{
  std::size_t offset = 0U;
  while (offset < encoding.size())
  {
    const ssize_t count = ::write(fd, encoding.data() + offset,
                                  encoding.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(effect_journal_error_code::store_write_failed,
               "cannot write controller journal snapshot");
    }
    if (count == 0)
      throw effect_journal_error(
          effect_journal_error_code::store_write_failed,
          "controller journal snapshot write made no progress");
    offset += static_cast<std::size_t>(count);
  }
}

void validate_successor(
    const std::optional<effect_attempt_record>& latest,
    const effect_attempt_record& record)
{
  if (!latest)
  {
    if (record.sequence() != 0U || record.previous())
      throw effect_journal_error(
          effect_journal_error_code::store_conflict,
          "controller journal must begin with sequence zero");
    return;
  }
  if (record.sequence() != latest->sequence() + 1U ||
      !record.previous() || *record.previous() != latest->identity() ||
      record.attempt() != latest->attempt() ||
      record.session() != latest->session() ||
      record.nonce() != latest->nonce() ||
      record.before_total() != latest->before_total() ||
      record.after_total() != latest->after_total())
    throw effect_journal_error(
        effect_journal_error_code::store_conflict,
        "controller journal snapshot is not the exact successor");
}

} // namespace

posix_effect_journal_store::posix_effect_journal_store(int directory_fd) noexcept
    : directory_fd_(directory_fd)
{
}

posix_effect_journal_store posix_effect_journal_store::open(
    const std::string& directory)
{
  fd_guard fd(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                        O_NOFOLLOW));
  if (fd.get() < 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot open controller journal directory");
  return posix_effect_journal_store(fd.release());
}

posix_effect_journal_store posix_effect_journal_store::from_directory_fd(
    int directory_fd)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot duplicate controller journal directory");
  fd_guard fd(duplicate);
  struct stat status{};
  if (::fstat(fd.get(), &status) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot inspect controller journal directory");
  if (!S_ISDIR(status.st_mode))
    throw effect_journal_error(
        effect_journal_error_code::store_open_failed,
        "controller journal descriptor is not a directory");
  return posix_effect_journal_store(fd.release());
}

posix_effect_journal_store::posix_effect_journal_store(
    posix_effect_journal_store&& other) noexcept
    : directory_fd_(other.directory_fd_)
{
  other.directory_fd_ = -1;
}

posix_effect_journal_store& posix_effect_journal_store::operator=(
    posix_effect_journal_store&& other) noexcept
{
  if (this != &other)
  {
    if (directory_fd_ >= 0)
      (void)::close(directory_fd_);
    directory_fd_ = other.directory_fd_;
    other.directory_fd_ = -1;
  }
  return *this;
}

posix_effect_journal_store::~posix_effect_journal_store()
{
  if (directory_fd_ >= 0)
    (void)::close(directory_fd_);
}

std::optional<effect_attempt_record>
posix_effect_journal_store::load_latest(
    const session_identity& attempt) const
{
  auto lock = lock_store(directory_fd_);
  return load_latest_unlocked(directory_fd_, attempt);
}

effect_attempt_record posix_effect_journal_store::append(
    const effect_attempt_record& record)
{
  auto lock = lock_store(directory_fd_);
  const auto latest = load_latest_unlocked(directory_fd_, record.attempt());
  validate_successor(latest, record);

  const auto encoding = encode_effect_attempt_record(record);
  const std::string final_name = record_name(record);
  const std::string temporary_name =
      ".tmp-" + std::to_string(static_cast<unsigned long long>(::getpid())) +
      "-" + record.identity().hex();
  if (::unlinkat(directory_fd_, temporary_name.c_str(), 0) != 0 &&
      errno != ENOENT)
    io_error(effect_journal_error_code::store_write_failed,
             "cannot remove stale temporary controller journal snapshot");
  fd_guard temporary(::openat(directory_fd_, temporary_name.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600));
  if (temporary.get() < 0)
    io_error(effect_journal_error_code::store_write_failed,
             "cannot create temporary controller journal snapshot");
  try
  {
    write_all(temporary.get(), encoding);
    if (::fchmod(temporary.get(), 0444) != 0)
      io_error(effect_journal_error_code::store_write_failed,
               "cannot seal controller journal snapshot permissions");
    if (::fsync(temporary.get()) != 0)
      io_error(effect_journal_error_code::store_sync_failed,
               "cannot synchronize controller journal snapshot");
    if (::linkat(directory_fd_, temporary_name.c_str(), directory_fd_,
                 final_name.c_str(), 0) != 0)
    {
      if (errno == EEXIST)
        throw effect_journal_error(
            effect_journal_error_code::store_conflict,
            "controller journal snapshot already exists", errno);
      io_error(effect_journal_error_code::store_write_failed,
               "cannot publish controller journal snapshot");
    }
    if (::unlinkat(directory_fd_, temporary_name.c_str(), 0) != 0)
      io_error(effect_journal_error_code::store_write_failed,
               "cannot remove temporary controller journal snapshot");
    temporary.reset();
    if (::fsync(directory_fd_) != 0)
      io_error(effect_journal_error_code::store_sync_failed,
               "cannot synchronize controller journal directory");
  }
  catch (...)
  {
    (void)::unlinkat(directory_fd_, temporary_name.c_str(), 0);
    throw;
  }
  return record;
}

} // namespace pkgctl
