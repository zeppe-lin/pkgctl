// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_journal_codec.h>
#include <pkgctl/run_store.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
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
    transaction_run_journal_error_code code,
    const std::string& message,
    int system_error)
{
  throw transaction_run_journal_error(
      code, message + ": " + std::strerror(system_error), system_error);
}

[[noreturn]] void io_error(
    transaction_run_journal_error_code code,
    const std::string& message)
{
  throw_io_error(code, message, errno);
}

std::string sequence_text(std::uint64_t sequence)
{
  std::array<char, 32> buffer{};
  const int length = std::snprintf(
      buffer.data(), buffer.size(), "%020llu",
      static_cast<unsigned long long>(sequence));
  if (length != 20)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_record,
        "cannot encode journal sequence");
  return std::string(buffer.data(), static_cast<std::size_t>(length));
}

std::string record_name(const transaction_run_journal_record& record)
{
  return record.journal().hex() + "-" + sequence_text(record.sequence()) +
         "-" + record.identity().hex() + ".pjr";
}

std::string head_name(const session_identity& journal)
{
  return journal.hex() + ".pjh";
}

struct journal_head final {
  std::uint64_t sequence;
  session_identity record;
};

constexpr std::array<std::uint8_t, 8> head_magic{
    'P', 'K', 'G', 'R', 'U', 'N', 'H', '1'};
constexpr std::uint16_t head_encoding_version = 1U;
constexpr std::size_t head_encoding_size =
    head_magic.size() + 2U + 32U + 8U + 32U + 32U;

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record,
      "transaction-run journal head identity contains invalid hex");
}

void append_u16(transaction_run_encoding& output, std::uint16_t value)
{
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u64(transaction_run_encoding& output, std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    output.push_back(
        static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

void append_identity(
    transaction_run_encoding& output,
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
    const session_identity& journal,
    std::uint64_t sequence,
    const session_identity& record)
{
  return make_session_identity(
      "pkgctl/transaction-run-journal-head/1",
      {journal.hex(), std::to_string(sequence), record.hex()});
}

transaction_run_encoding encode_head(
    const session_identity& journal,
    std::uint64_t sequence,
    const session_identity& record)
{
  transaction_run_encoding output;
  output.reserve(head_encoding_size);
  output.insert(output.end(), head_magic.begin(), head_magic.end());
  append_u16(output, head_encoding_version);
  append_identity(output, journal);
  append_u64(output, sequence);
  append_identity(output, record);
  append_identity(output, head_checksum(journal, sequence, record));
  return output;
}

std::uint16_t read_u16(
    const transaction_run_encoding& input,
    std::size_t& offset)
{
  const auto high = input[offset++];
  const auto low = input[offset++];
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(high) << 8U) | low);
}

std::uint64_t read_u64(
    const transaction_run_encoding& input,
    std::size_t& offset)
{
  std::uint64_t result = 0U;
  for (unsigned int index = 0U; index < 8U; ++index)
    result = (result << 8U) | input[offset++];
  return result;
}

session_identity read_identity(
    const transaction_run_encoding& input,
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

journal_head decode_head(
    const transaction_run_encoding& input,
    const session_identity& expected_journal)
{
  if (input.size() != head_encoding_size)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal head has invalid size");

  std::size_t offset = 0U;
  for (const auto expected : head_magic)
  {
    if (input[offset++] != expected)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal head has invalid magic");
  }
  if (read_u16(input, offset) != head_encoding_version)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal head has unsupported version");

  const auto journal = read_identity(input, offset);
  const auto sequence = read_u64(input, offset);
  const auto record = read_identity(input, offset);
  const auto checksum = read_identity(input, offset);
  if (journal != expected_journal ||
      checksum != head_checksum(journal, sequence, record))
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal head authority is invalid");
  return journal_head{sequence, record};
}

fd_guard lock_store(int directory_fd)
{
  fd_guard lock(::openat(directory_fd, ".pkgctl-run.lock",
                         O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (lock.get() < 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot open transaction-run journal lock");
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot inspect transaction-run journal lock");
  if (!S_ISREG(status.st_mode))
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_open_failed,
        "transaction-run journal lock is not a regular file");
  if (::flock(lock.get(), LOCK_EX) != 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot acquire transaction-run journal lock");
  return lock;
}

std::optional<fd_guard> lock_store_read_only(int directory_fd)
{
  fd_guard lock(::openat(directory_fd, ".pkgctl-run.lock",
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (lock.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    if (errno == ELOOP)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_open_failed,
          "transaction-run journal lock is a symbolic link");
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot open transaction-run journal lock read-only");
  }
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot inspect transaction-run journal lock");
  if (!S_ISREG(status.st_mode))
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_open_failed,
        "transaction-run journal lock is not a regular file");
  if (::flock(lock.get(), LOCK_SH) != 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot acquire shared transaction-run journal lock");
  return std::optional<fd_guard>(std::move(lock));
}

transaction_run_encoding read_encoding(int directory_fd, const std::string& name)
{
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0)
  {
    if (errno == ELOOP)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal snapshot is a symbolic link");
    io_error(transaction_run_journal_error_code::store_read_failed,
             "cannot open transaction-run journal snapshot");
  }
  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    io_error(transaction_run_journal_error_code::store_read_failed,
             "cannot inspect transaction-run journal snapshot");
  if (!S_ISREG(status.st_mode) || (status.st_mode & 0222) != 0 ||
      status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) >
          maximum_transaction_run_encoding_size)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal snapshot has invalid type or size");
  transaction_run_encoding encoding(static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0U;
  while (offset < encoding.size())
  {
    const ssize_t count = ::read(file.get(), encoding.data() + offset,
                                 encoding.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(transaction_run_journal_error_code::store_read_failed,
               "cannot read transaction-run journal snapshot");
    }
    if (count == 0)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal snapshot ended early");
    offset += static_cast<std::size_t>(count);
  }
  return encoding;
}

std::optional<journal_head> read_head(
    int directory_fd,
    const session_identity& journal)
{
  const auto name = head_name(journal);
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    if (errno == ELOOP)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal head is a symbolic link");
    io_error(transaction_run_journal_error_code::store_read_failed,
             "cannot open transaction-run journal head");
  }

  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    io_error(transaction_run_journal_error_code::store_read_failed,
             "cannot inspect transaction-run journal head");
  if (!S_ISREG(status.st_mode) || (status.st_mode & 0222) != 0 ||
      status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) != head_encoding_size)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal head has invalid type or mode");

  transaction_run_encoding encoding(head_encoding_size);
  std::size_t offset = 0U;
  while (offset < encoding.size())
  {
    const ssize_t count = ::read(
        file.get(), encoding.data() + offset, encoding.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(transaction_run_journal_error_code::store_read_failed,
               "cannot read transaction-run journal head");
    }
    if (count == 0)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal head ended early");
    offset += static_cast<std::size_t>(count);
  }
  return decode_head(encoding, journal);
}

bool has_unexpected_journal_record_entries(
    int directory_fd,
    const session_identity& journal,
    const std::optional<std::string>& allowed_name)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(transaction_run_journal_error_code::store_read_failed,
             "cannot duplicate transaction-run journal directory");
  directory_guard directory(::fdopendir(duplicate));
  if (directory.get() == nullptr)
  {
    const int problem = errno;
    (void)::close(duplicate);
    throw_io_error(
        transaction_run_journal_error_code::store_read_failed,
        "cannot enumerate transaction-run journal directory", problem);
  }
  ::rewinddir(directory.get());

  const std::string prefix = journal.hex() + "-";
  while (true)
  {
    errno = 0;
    dirent* entry = ::readdir(directory.get());
    if (entry == nullptr)
    {
      if (errno != 0)
        io_error(transaction_run_journal_error_code::store_read_failed,
                 "cannot enumerate transaction-run journal directory");
      return false;
    }
    const std::string_view name(entry->d_name);
    if (name.substr(0U, prefix.size()) == prefix &&
        (!allowed_name || name != *allowed_name))
      return true;
  }
}

std::optional<transaction_run_journal_record> load_latest_unlocked(
    int directory_fd,
    const session_identity& journal,
    const transaction_run_journal_record* pending = nullptr)
{
  const auto head = read_head(directory_fd, journal);
  if (!head)
  {
    const std::optional<std::string> allowed_name =
        pending ? std::optional<std::string>(record_name(*pending))
                : std::nullopt;
    if (has_unexpected_journal_record_entries(
            directory_fd, journal, allowed_name))
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal record exists without a durable head");

    if (pending)
    {
      const auto name = record_name(*pending);
      struct stat status{};
      if (::fstatat(directory_fd, name.c_str(), &status,
                    AT_SYMLINK_NOFOLLOW) == 0)
      {
        auto recovered = decode_transaction_run_record(
            read_encoding(directory_fd, name));
        if (recovered.journal() != pending->journal() ||
            recovered.sequence() != pending->sequence() ||
            recovered.identity() != pending->identity())
          throw transaction_run_journal_error(
              transaction_run_journal_error_code::store_corrupt,
              "uncommitted transaction-run snapshot authority is invalid");
      }
      else if (errno != ENOENT)
      {
        io_error(transaction_run_journal_error_code::store_read_failed,
                 "cannot inspect uncommitted transaction-run snapshot");
      }
    }
    return std::nullopt;
  }

  const std::string name =
      journal.hex() + "-" + sequence_text(head->sequence) + "-" +
      head->record.hex() + ".pjr";
  transaction_run_encoding encoding;
  try
  {
    encoding = read_encoding(directory_fd, name);
  }
  catch (const transaction_run_journal_error& problem)
  {
    if (problem.code() ==
            transaction_run_journal_error_code::store_read_failed &&
        problem.system_error() == ENOENT)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "transaction-run journal head names a missing snapshot");
    throw;
  }
  auto record = decode_transaction_run_record(encoding);
  if (record.journal() != journal ||
      record.sequence() != head->sequence ||
      record.identity() != head->record)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "transaction-run journal head and snapshot disagree");
  return record;
}

void verify_existing_snapshot(
    int directory_fd,
    const std::string& name,
    const transaction_run_journal_record& expected)
{
  const auto existing_encoding = read_encoding(directory_fd, name);
  const auto expected_encoding = encode_transaction_run_record(expected);
  if (existing_encoding != expected_encoding)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "existing transaction-run snapshot bytes are not exact");

  const auto existing = decode_transaction_run_record(existing_encoding);
  if (existing.journal() != expected.journal() ||
      existing.sequence() != expected.sequence() ||
      existing.identity() != expected.identity())
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_corrupt,
        "existing transaction-run snapshot authority is invalid");
}

void write_all(int fd, const transaction_run_encoding& encoding)
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
      io_error(transaction_run_journal_error_code::store_write_failed,
               "cannot write transaction-run journal snapshot");
    }
    if (count == 0)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_write_failed,
          "transaction-run journal snapshot write made no progress");
    offset += static_cast<std::size_t>(count);
  }
}

void publish_head(
    int directory_fd,
    const transaction_run_journal_record& record)
{
  const auto encoding = encode_head(
      record.journal(), record.sequence(), record.identity());
  const std::string final_name = head_name(record.journal());
  const std::string temporary_name =
      ".tmp-head-" +
      std::to_string(static_cast<unsigned long long>(::getpid())) +
      "-" + record.identity().hex();

  if (::unlinkat(directory_fd, temporary_name.c_str(), 0) != 0 &&
      errno != ENOENT)
    io_error(transaction_run_journal_error_code::store_write_failed,
             "cannot remove stale temporary transaction-run journal head");

  fd_guard temporary(::openat(
      directory_fd, temporary_name.c_str(),
      O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (temporary.get() < 0)
    io_error(transaction_run_journal_error_code::store_write_failed,
             "cannot create temporary transaction-run journal head");

  try
  {
    write_all(temporary.get(), encoding);
    if (::fchmod(temporary.get(), 0444) != 0)
      io_error(transaction_run_journal_error_code::store_write_failed,
               "cannot seal transaction-run journal head permissions");
    if (::fsync(temporary.get()) != 0)
      io_error(transaction_run_journal_error_code::store_sync_failed,
               "cannot synchronize transaction-run journal head");
    if (::renameat(directory_fd, temporary_name.c_str(),
                   directory_fd, final_name.c_str()) != 0)
      io_error(transaction_run_journal_error_code::store_write_failed,
               "cannot publish transaction-run journal head");
    temporary.reset();
    if (::fsync(directory_fd) != 0)
      io_error(transaction_run_journal_error_code::store_sync_failed,
               "cannot synchronize transaction-run journal head directory");
  }
  catch (...)
  {
    (void)::unlinkat(directory_fd, temporary_name.c_str(), 0);
    throw;
  }
}

void validate_successor(
    const std::optional<transaction_run_journal_record>& latest,
    const transaction_run_journal_record& record)
{
  if (!latest)
  {
    if (record.sequence() != 0U || record.previous())
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_conflict,
          "transaction-run journal must begin with sequence zero");
    return;
  }
  try
  {
    record.validate_successor_of(*latest);
  }
  catch (const transaction_run_journal_error& problem)
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_conflict,
        std::string("transaction-run journal snapshot is not the exact successor: ") +
            problem.what());
  }
}

} // namespace

posix_transaction_run_journal_store::posix_transaction_run_journal_store(int directory_fd) noexcept
    : directory_fd_(directory_fd)
{
}

posix_transaction_run_journal_store posix_transaction_run_journal_store::open(
    const std::string& directory)
{
  fd_guard fd(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                        O_NOFOLLOW));
  if (fd.get() < 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot open transaction-run journal directory");
  return posix_transaction_run_journal_store(fd.release());
}

posix_transaction_run_journal_store posix_transaction_run_journal_store::from_directory_fd(
    int directory_fd)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot duplicate transaction-run journal directory");
  fd_guard fd(duplicate);
  struct stat status{};
  if (::fstat(fd.get(), &status) != 0)
    io_error(transaction_run_journal_error_code::store_open_failed,
             "cannot inspect transaction-run journal directory");
  if (!S_ISDIR(status.st_mode))
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::store_open_failed,
        "transaction-run journal descriptor is not a directory");
  return posix_transaction_run_journal_store(fd.release());
}

posix_transaction_run_journal_store::posix_transaction_run_journal_store(
    posix_transaction_run_journal_store&& other) noexcept
    : directory_fd_(other.directory_fd_)
{
  other.directory_fd_ = -1;
}

posix_transaction_run_journal_store& posix_transaction_run_journal_store::operator=(
    posix_transaction_run_journal_store&& other) noexcept
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

posix_transaction_run_journal_store::~posix_transaction_run_journal_store()
{
  if (directory_fd_ >= 0)
    (void)::close(directory_fd_);
}

std::optional<transaction_run_journal_record>
posix_transaction_run_journal_store::load_latest(
    const session_identity& journal) const
{
  // Existing stores are observed under a shared, read-only lock. An empty
  // caller-created directory has no lock file yet; inspect it once, then retry
  // under the lock if a writer established the store concurrently. No reader
  // creates or opens any store object for writing.
  if (auto lock = lock_store_read_only(directory_fd_))
    return load_latest_unlocked(directory_fd_, journal);

  try
  {
    auto record = load_latest_unlocked(directory_fd_, journal);
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_latest_unlocked(directory_fd_, journal);
    return record;
  }
  catch (...)
  {
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_latest_unlocked(directory_fd_, journal);
    throw;
  }
}

transaction_run_journal_record posix_transaction_run_journal_store::append(
    const transaction_run_journal_record& record)
{
  auto lock = lock_store(directory_fd_);
  const auto latest = load_latest_unlocked(
      directory_fd_, record.journal(), &record);
  if (latest && latest->identity() == record.identity())
  {
    if (latest->sequence() != record.sequence() ||
        latest->journal() != record.journal())
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::store_corrupt,
          "committed transaction-run snapshot authority is invalid");
    publish_head(directory_fd_, record);
    return record;
  }
  validate_successor(latest, record);

  const auto encoding = encode_transaction_run_record(record);
  const std::string final_name = record_name(record);
  const std::string temporary_name =
      ".tmp-" + std::to_string(static_cast<unsigned long long>(::getpid())) +
      "-" + record.identity().hex();
  if (::unlinkat(directory_fd_, temporary_name.c_str(), 0) != 0 &&
      errno != ENOENT)
    io_error(transaction_run_journal_error_code::store_write_failed,
             "cannot remove stale temporary transaction-run journal snapshot");
  fd_guard temporary(::openat(directory_fd_, temporary_name.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600));
  if (temporary.get() < 0)
    io_error(transaction_run_journal_error_code::store_write_failed,
             "cannot create temporary transaction-run journal snapshot");
  try
  {
    write_all(temporary.get(), encoding);
    if (::fchmod(temporary.get(), 0444) != 0)
      io_error(transaction_run_journal_error_code::store_write_failed,
               "cannot seal transaction-run journal snapshot permissions");
    if (::fsync(temporary.get()) != 0)
      io_error(transaction_run_journal_error_code::store_sync_failed,
               "cannot synchronize transaction-run journal snapshot");
    const bool published =
        ::linkat(directory_fd_, temporary_name.c_str(), directory_fd_,
                 final_name.c_str(), 0) == 0;
    if (!published)
    {
      if (errno == EEXIST)
        verify_existing_snapshot(directory_fd_, final_name, record);
      else
        io_error(transaction_run_journal_error_code::store_write_failed,
                 "cannot publish transaction-run journal snapshot");
    }
    if (::unlinkat(directory_fd_, temporary_name.c_str(), 0) != 0)
      io_error(transaction_run_journal_error_code::store_write_failed,
               "cannot remove temporary transaction-run journal snapshot");
    temporary.reset();
    if (::fsync(directory_fd_) != 0)
      io_error(transaction_run_journal_error_code::store_sync_failed,
               "cannot synchronize transaction-run journal directory");
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
