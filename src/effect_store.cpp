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

[[noreturn]] void throw_io_error(
    effect_journal_error_code code,
    const std::string& message,
    int system_error)
{
  throw effect_journal_error(
      code, message + ": " + std::strerror(system_error), system_error);
}

[[noreturn]] void io_error(
    effect_journal_error_code code,
    const std::string& message)
{
  throw_io_error(code, message, errno);
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

std::string head_name(const session_identity& attempt)
{
  return attempt.hex() + ".pjeh";
}

struct effect_journal_head final {
  std::uint64_t sequence;
  session_identity record;
};

constexpr std::array<std::uint8_t, 8> head_magic{
    'P', 'C', 'T', 'L', 'E', 'F', 'H', '1'};
constexpr std::uint16_t head_encoding_version = 1U;
constexpr std::size_t head_encoding_size =
    head_magic.size() + 2U + 32U + 8U + 32U + 32U;

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  throw effect_journal_error(
      effect_journal_error_code::invalid_record,
      "effect-journal head identity contains invalid hex");
}

void append_u16(effect_attempt_encoding& output, std::uint16_t value)
{
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u64(effect_attempt_encoding& output, std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    output.push_back(
        static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

void append_identity(
    effect_attempt_encoding& output,
    const session_identity& identity)
{
  const auto value = identity.hex();
  for (std::size_t index = 0; index < value.size(); index += 2U)
  {
    const auto high = hexadecimal_digit(value[index]);
    const auto low = hexadecimal_digit(value[index + 1U]);
    output.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
}

session_identity head_checksum(
    const session_identity& attempt,
    std::uint64_t sequence,
    const session_identity& record)
{
  return make_session_identity(
      "pkgctl/effect-journal-head/1",
      {attempt.hex(), std::to_string(sequence), record.hex()});
}

effect_attempt_encoding encode_head(
    const session_identity& attempt,
    std::uint64_t sequence,
    const session_identity& record)
{
  effect_attempt_encoding output;
  output.reserve(head_encoding_size);
  output.insert(output.end(), head_magic.begin(), head_magic.end());
  append_u16(output, head_encoding_version);
  append_identity(output, attempt);
  append_u64(output, sequence);
  append_identity(output, record);
  append_identity(output, head_checksum(attempt, sequence, record));
  return output;
}

std::uint16_t read_u16(
    const effect_attempt_encoding& input,
    std::size_t& offset)
{
  const auto high = input[offset++];
  const auto low = input[offset++];
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(high) << 8U) | low);
}

std::uint64_t read_u64(
    const effect_attempt_encoding& input,
    std::size_t& offset)
{
  std::uint64_t result = 0U;
  for (unsigned int index = 0U; index < 8U; ++index)
    result = (result << 8U) | input[offset++];
  return result;
}

session_identity read_identity(
    const effect_attempt_encoding& input,
    std::size_t& offset)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string value(64U, '0');
  for (std::size_t index = 0U; index < 32U; ++index)
  {
    const auto byte = input[offset++];
    value[index * 2U] = digits[(byte >> 4U) & 0x0fU];
    value[index * 2U + 1U] = digits[byte & 0x0fU];
  }
  return session_identity::from_hex(std::move(value));
}

effect_journal_head decode_head(
    const effect_attempt_encoding& input,
    const session_identity& expected_attempt)
{
  if (input.size() != head_encoding_size)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal head has invalid size");

  std::size_t offset = 0U;
  for (const auto expected : head_magic)
  {
    if (input[offset++] != expected)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal head has invalid magic");
  }
  if (read_u16(input, offset) != head_encoding_version)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal head has unsupported version");

  const auto attempt = read_identity(input, offset);
  const auto sequence = read_u64(input, offset);
  const auto record = read_identity(input, offset);
  const auto checksum = read_identity(input, offset);
  if (attempt != expected_attempt ||
      checksum != head_checksum(attempt, sequence, record))
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal head authority is invalid");
  return effect_journal_head{sequence, record};
}

std::uint16_t record_encoding_version(
    const effect_attempt_encoding& encoding)
{
  if (encoding.size() < 10U)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal snapshot is too short to name its encoding");
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(encoding[8]) << 8U) | encoding[9]);
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
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot inspect controller journal lock");
  if (!S_ISREG(status.st_mode))
    throw effect_journal_error(
        effect_journal_error_code::store_open_failed,
        "controller journal lock is not a regular file");
  if (::flock(lock.get(), LOCK_EX) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot acquire controller journal lock");
  return lock;
}

std::optional<fd_guard> lock_store_read_only(int directory_fd)
{
  fd_guard lock(::openat(directory_fd, ".pkgctl-effect.lock",
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (lock.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    if (errno == ELOOP)
      throw effect_journal_error(
          effect_journal_error_code::store_open_failed,
          "effect-journal lock is a symbolic link");
    io_error(effect_journal_error_code::store_open_failed,
             "cannot open effect-journal lock read-only");
  }
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot inspect effect-journal lock");
  if (!S_ISREG(status.st_mode))
    throw effect_journal_error(
        effect_journal_error_code::store_open_failed,
        "effect-journal lock is not a regular file");
  if (::flock(lock.get(), LOCK_SH) != 0)
    io_error(effect_journal_error_code::store_open_failed,
             "cannot acquire shared effect-journal lock");
  return std::optional<fd_guard>(std::move(lock));
}

effect_attempt_encoding read_encoding(int directory_fd, const std::string& name)
{
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0)
  {
    if (errno == ELOOP)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal snapshot is a symbolic link");
    io_error(effect_journal_error_code::store_read_failed,
             "cannot open controller journal snapshot");
  }
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

std::optional<effect_journal_head> read_head(
    int directory_fd,
    const session_identity& attempt)
{
  const auto name = head_name(attempt);
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    if (errno == ELOOP)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal head is a symbolic link");
    io_error(effect_journal_error_code::store_read_failed,
             "cannot open effect-journal head");
  }

  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot inspect effect-journal head");
  if (!S_ISREG(status.st_mode) || (status.st_mode & 0222) != 0 ||
      status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) != head_encoding_size)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal head has invalid type or mode");

  effect_attempt_encoding encoding(head_encoding_size);
  std::size_t offset = 0U;
  while (offset < encoding.size())
  {
    const ssize_t count = ::read(
        file.get(), encoding.data() + offset, encoding.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(effect_journal_error_code::store_read_failed,
               "cannot read effect-journal head");
    }
    if (count == 0)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal head ended early");
    offset += static_cast<std::size_t>(count);
  }
  return decode_head(encoding, attempt);
}

std::optional<effect_attempt_record> load_latest_unlocked(
    int directory_fd,
    const session_identity& attempt,
    const effect_attempt_record* pending = nullptr)
{
  const auto head = read_head(directory_fd, attempt);
  if (head)
  {
    const std::string name =
        attempt.hex() + "-" + sequence_text(head->sequence) + "-" +
        head->record.hex() + ".pje";
    effect_attempt_encoding encoding;
    try
    {
      encoding = read_encoding(directory_fd, name);
    }
    catch (const effect_journal_error& problem)
    {
      if (problem.code() == effect_journal_error_code::store_read_failed &&
          problem.system_error() == ENOENT)
        throw effect_journal_error(
            effect_journal_error_code::store_corrupt,
            "effect-journal head names a missing snapshot");
      throw;
    }
    if (record_encoding_version(encoding) != effect_attempt_encoding_version)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal head names an unsupported snapshot");
    auto record = decode_effect_attempt_record(encoding);
    if (record.attempt() != attempt ||
        record.sequence() != head->sequence ||
        record.identity() != head->record)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal head and snapshot disagree");
    return record;
  }

  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(effect_journal_error_code::store_read_failed,
             "cannot duplicate controller journal directory");
  directory_guard directory(::fdopendir(duplicate));
  if (directory.get() == nullptr)
  {
    const int problem = errno;
    (void)::close(duplicate);
    throw_io_error(
        effect_journal_error_code::store_read_failed,
        "cannot enumerate controller journal directory", problem);
  }
  ::rewinddir(directory.get());

  while (true)
  {
    errno = 0;
    dirent* entry = ::readdir(directory.get());
    if (entry == nullptr)
    {
      if (errno != 0)
        io_error(effect_journal_error_code::store_read_failed,
                 "cannot enumerate controller journal directory");
      break;
    }

    const std::string_view name(entry->d_name);
    const auto parsed = parse_name(name, attempt);
    if (!parsed)
    {
      const std::string prefix = attempt.hex() + "-";
      if (name.substr(0U, prefix.size()) == prefix)
        throw effect_journal_error(
            effect_journal_error_code::store_corrupt,
            "controller journal contains a malformed record name");
      continue;
    }

    const auto encoding = read_encoding(directory_fd, entry->d_name);
    if (record_encoding_version(encoding) != effect_attempt_encoding_version)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "effect-journal snapshot has an unsupported encoding");
    auto record = decode_effect_attempt_record(encoding);
    if (record.attempt() != attempt ||
        record.sequence() != parsed->sequence ||
        record.identity().hex() != parsed->identity)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "controller journal filename and content disagree");

    if (pending != nullptr &&
        record.sequence() == pending->sequence() &&
        record.identity() == pending->identity())
      continue;
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "effect-journal snapshot exists without a durable head");
  }

  return std::nullopt;
}

void verify_existing_snapshot(
    int directory_fd,
    const std::string& name,
    const effect_attempt_record& expected)
{
  const auto existing_encoding = read_encoding(directory_fd, name);
  if (record_encoding_version(existing_encoding) !=
      effect_attempt_encoding_version)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "existing effect-journal snapshot has an unsupported encoding");

  const auto expected_encoding = encode_effect_attempt_record(expected);
  if (existing_encoding != expected_encoding)
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "existing effect-journal snapshot bytes are not exact");

  const auto existing = decode_effect_attempt_record(existing_encoding);
  if (existing.attempt() != expected.attempt() ||
      existing.sequence() != expected.sequence() ||
      existing.identity() != expected.identity())
    throw effect_journal_error(
        effect_journal_error_code::store_corrupt,
        "existing effect-journal snapshot authority is invalid");
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

void publish_head(
    int directory_fd,
    const effect_attempt_record& record)
{
  const auto encoding = encode_head(
      record.attempt(), record.sequence(), record.identity());
  const std::string final_name = head_name(record.attempt());
  const std::string temporary_name =
      ".tmp-effect-head-" +
      std::to_string(static_cast<unsigned long long>(::getpid())) +
      "-" + record.identity().hex();

  if (::unlinkat(directory_fd, temporary_name.c_str(), 0) != 0 &&
      errno != ENOENT)
    io_error(effect_journal_error_code::store_write_failed,
             "cannot remove stale temporary effect-journal head");

  fd_guard temporary(::openat(
      directory_fd, temporary_name.c_str(),
      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (temporary.get() < 0)
    io_error(effect_journal_error_code::store_write_failed,
             "cannot create temporary effect-journal head");

  try
  {
    write_all(temporary.get(), encoding);
    if (::fchmod(temporary.get(), 0444) != 0)
      io_error(effect_journal_error_code::store_write_failed,
               "cannot seal effect-journal head permissions");
    if (::fsync(temporary.get()) != 0)
      io_error(effect_journal_error_code::store_sync_failed,
               "cannot synchronize effect-journal head");
    if (::renameat(directory_fd, temporary_name.c_str(),
                   directory_fd, final_name.c_str()) != 0)
      io_error(effect_journal_error_code::store_write_failed,
               "cannot publish effect-journal head");
    temporary.reset();
    if (::fsync(directory_fd) != 0)
      io_error(effect_journal_error_code::store_sync_failed,
               "cannot synchronize effect-journal head directory");
  }
  catch (...)
  {
    (void)::unlinkat(directory_fd, temporary_name.c_str(), 0);
    throw;
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
  try
  {
    record.validate_successor_of(*latest);
  }
  catch (const effect_journal_error& problem)
  {
    if (problem.code() != effect_journal_error_code::invalid_transition)
      throw;
    throw effect_journal_error(
        effect_journal_error_code::store_conflict,
        std::string("controller journal snapshot is not the exact successor: ") +
            problem.what());
  }
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
  // Existing stores are observed under a shared, read-only lock. An empty
  // caller-created directory has no lock file yet; inspect it once, then retry
  // under the lock if a writer established the store concurrently. No reader
  // creates or opens any store object for writing.
  if (auto lock = lock_store_read_only(directory_fd_))
    return load_latest_unlocked(directory_fd_, attempt);

  try
  {
    auto record = load_latest_unlocked(directory_fd_, attempt);
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_latest_unlocked(directory_fd_, attempt);
    return record;
  }
  catch (...)
  {
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_latest_unlocked(directory_fd_, attempt);
    throw;
  }
}

effect_attempt_record posix_effect_journal_store::append(
    const effect_attempt_record& record)
{
  auto lock = lock_store(directory_fd_);
  const auto latest = load_latest_unlocked(
      directory_fd_, record.attempt(), &record);
  if (latest && latest->identity() == record.identity())
  {
    if (latest->sequence() != record.sequence() ||
        latest->attempt() != record.attempt())
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "committed effect-journal snapshot authority is invalid");
    const auto existing = read_encoding(directory_fd_, record_name(record));
    if (record_encoding_version(existing) != effect_attempt_encoding_version)
      throw effect_journal_error(
          effect_journal_error_code::store_corrupt,
          "committed effect-journal snapshot has unsupported encoding");
    verify_existing_snapshot(
        directory_fd_, record_name(record), record);
    publish_head(directory_fd_, record);
    return record;
  }
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
    const bool published =
        ::linkat(directory_fd_, temporary_name.c_str(), directory_fd_,
                 final_name.c_str(), 0) == 0;
    if (!published)
    {
      if (errno == EEXIST)
        verify_existing_snapshot(directory_fd_, final_name, record);
      else
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
    publish_head(directory_fd_, record);
  }
  catch (...)
  {
    (void)::unlinkat(directory_fd_, temporary_name.c_str(), 0);
    throw;
  }
  return record;
}

} // namespace pkgctl
