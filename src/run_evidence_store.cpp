// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_evidence_store.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cerrno>
#include <cstring>
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

enum class evidence_kind : std::uint8_t {
  construction = 1,
  check = 2,
};

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

[[noreturn]] void throw_io_error(
    transaction_run_evidence_error_code code,
    const std::string& message,
    int system_error)
{
  throw transaction_run_evidence_error(
      code, message + ": " + std::strerror(system_error), system_error);
}

[[noreturn]] void io_error(
    transaction_run_evidence_error_code code,
    const std::string& message)
{
  throw_io_error(code, message, errno);
}

[[noreturn]] void corrupt(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::store_corrupt, message);
}

[[noreturn]] void conflict(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::store_conflict, message);
}

void require_directory(int fd, const std::string& message)
{
  struct stat status{};
  if (::fstat(fd, &status) != 0)
    io_error(transaction_run_evidence_error_code::store_open_failed, message);
  if (!S_ISDIR(status.st_mode))
    throw transaction_run_evidence_error(
        transaction_run_evidence_error_code::store_contract_violation,
        message + ": descriptor is not a directory");
}

fd_guard lock_store(int directory_fd)
{
  fd_guard lock(::openat(
      directory_fd, ".pkgctl-run-evidence.lock",
      O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (lock.get() < 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot open transaction-run evidence lock");
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot inspect transaction-run evidence lock");
  if (!S_ISREG(status.st_mode) || status.st_nlink != 1)
    throw transaction_run_evidence_error(
        transaction_run_evidence_error_code::store_contract_violation,
        "transaction-run evidence lock is not one regular file");
  if (::flock(lock.get(), LOCK_EX) != 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot lock transaction-run evidence store");
  return lock;
}

std::optional<fd_guard> lock_store_read_only(int directory_fd)
{
  fd_guard lock(::openat(
      directory_fd, ".pkgctl-run-evidence.lock",
      O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (lock.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    if (errno == ELOOP)
      throw transaction_run_evidence_error(
          transaction_run_evidence_error_code::store_open_failed,
          "transaction-run evidence lock is a symbolic link");
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot open transaction-run evidence lock read-only");
  }
  struct stat status{};
  if (::fstat(lock.get(), &status) != 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot inspect transaction-run evidence lock");
  if (!S_ISREG(status.st_mode) || status.st_nlink != 1)
    throw transaction_run_evidence_error(
        transaction_run_evidence_error_code::store_contract_violation,
        "transaction-run evidence lock is not one regular file");
  if (::flock(lock.get(), LOCK_SH) != 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot acquire shared transaction-run evidence lock");
  return std::optional<fd_guard>(std::move(lock));
}

void sync_directory(int directory_fd)
{
  if (::fsync(directory_fd) != 0)
    io_error(transaction_run_evidence_error_code::store_sync_failed,
             "cannot synchronize transaction-run evidence directory");
}

std::string kind_name(evidence_kind kind)
{
  switch (kind)
  {
    case evidence_kind::construction:
      return "construction";
    case evidence_kind::check:
      return "check";
  }
  corrupt("unknown transaction-run evidence kind");
}

std::string object_name(evidence_kind kind, const session_identity& identity)
{
  return kind_name(kind) + "-" + identity.hex() + ".pce";
}

std::string index_name(
    evidence_kind kind,
    const session_identity& journal,
    const session_identity& dispatch,
    const session_identity& attempt)
{
  return kind_name(kind) + "-" + journal.hex() + "-" + dispatch.hex() +
         "-" + attempt.hex() + ".pci";
}

std::string temporary_name(std::string_view role)
{
  static std::atomic<unsigned long> counter{0UL};
  const auto sequence = counter.fetch_add(1UL, std::memory_order_relaxed) + 1UL;
  return ".pkgctl-evidence-" + std::string(role) + ".tmp." +
         std::to_string(static_cast<unsigned long>(::getpid())) + "." +
         std::to_string(sequence);
}

struct temporary_file final {
  fd_guard descriptor;
  std::string name;
};

temporary_file create_temporary_file(
    int directory_fd,
    std::string_view role)
{
  for (unsigned int attempt = 0U; attempt < 1024U; ++attempt)
  {
    auto name = temporary_name(role);
    fd_guard descriptor(::openat(
        directory_fd, name.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (descriptor.get() >= 0)
      return temporary_file{std::move(descriptor), std::move(name)};
    if (errno != EEXIST)
      io_error(transaction_run_evidence_error_code::store_write_failed,
               "cannot create transaction-run evidence temporary file");
  }
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::store_write_failed,
      "cannot allocate a unique transaction-run evidence temporary file");
}

void write_all(int fd, const std::vector<std::uint8_t>& bytes)
{
  std::size_t offset = 0U;
  while (offset < bytes.size())
  {
    const auto written = ::write(
        fd, bytes.data() + offset, bytes.size() - offset);
    if (written < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(transaction_run_evidence_error_code::store_write_failed,
               "cannot write transaction-run evidence file");
    }
    if (written == 0)
      throw transaction_run_evidence_error(
          transaction_run_evidence_error_code::store_write_failed,
          "transaction-run evidence write made no progress");
    offset += static_cast<std::size_t>(written);
  }
}

std::optional<std::vector<std::uint8_t>> read_file(
    int directory_fd,
    const std::string& name,
    std::size_t maximum,
    bool absent_allowed)
{
  fd_guard file(::openat(
      directory_fd, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (file.get() < 0)
  {
    if (absent_allowed && errno == ENOENT)
      return std::nullopt;
    io_error(transaction_run_evidence_error_code::store_read_failed,
             "cannot open transaction-run evidence file " + name);
  }

  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    io_error(transaction_run_evidence_error_code::store_read_failed,
             "cannot inspect transaction-run evidence file " + name);
  if (!S_ISREG(status.st_mode) || status.st_nlink != 1 ||
      (status.st_mode & 0222) != 0 || status.st_size < 0)
  {
    corrupt(
        "transaction-run evidence path is not one immutable regular file: " +
        name);
  }
  const auto size = static_cast<std::uint64_t>(status.st_size);
  if (size > maximum)
    corrupt("transaction-run evidence file exceeds its size limit: " + name);

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::size_t offset = 0U;
  while (offset < bytes.size())
  {
    const auto count = ::read(file.get(), bytes.data() + offset,
                              bytes.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      io_error(transaction_run_evidence_error_code::store_read_failed,
               "cannot read transaction-run evidence file " + name);
    }
    if (count == 0)
      corrupt("transaction-run evidence file was truncated while reading: " +
              name);
    offset += static_cast<std::size_t>(count);
  }
  return bytes;
}

void persist_immutable(
    int directory_fd,
    const std::string& final_name,
    const std::vector<std::uint8_t>& bytes,
    const std::string& role)
{
  if (const auto existing = read_file(
          directory_fd, final_name,
          maximum_transaction_run_evidence_encoding_size, true))
  {
    if (*existing != bytes)
      conflict("immutable transaction-run evidence object differs: " +
               final_name);
    return;
  }

  auto temporary = create_temporary_file(directory_fd, role);
  try
  {
    write_all(temporary.descriptor.get(), bytes);
    if (::fchmod(temporary.descriptor.get(), 0400) != 0)
      io_error(transaction_run_evidence_error_code::store_write_failed,
               "cannot seal transaction-run evidence file permissions");
    if (::fsync(temporary.descriptor.get()) != 0)
      io_error(transaction_run_evidence_error_code::store_sync_failed,
               "cannot synchronize transaction-run evidence temporary file");
    temporary.descriptor.reset();

    if (::linkat(directory_fd, temporary.name.c_str(),
                 directory_fd, final_name.c_str(), 0) != 0)
    {
      if (errno != EEXIST)
        io_error(transaction_run_evidence_error_code::store_write_failed,
                 "cannot publish immutable transaction-run evidence file");
      const auto existing = read_file(
          directory_fd, final_name,
          maximum_transaction_run_evidence_encoding_size, false);
      if (!existing || *existing != bytes)
        conflict("immutable transaction-run evidence publication conflicts");
    }
    if (::unlinkat(directory_fd, temporary.name.c_str(), 0) != 0 &&
        errno != ENOENT)
      io_error(transaction_run_evidence_error_code::store_write_failed,
               "cannot remove transaction-run evidence temporary file");
    sync_directory(directory_fd);
  }
  catch (...)
  {
    temporary.descriptor.reset();
    (void)::unlinkat(directory_fd, temporary.name.c_str(), 0);
    throw;
  }
}

constexpr std::array<std::uint8_t, 8> index_magic{
    'P', 'K', 'G', 'E', 'V', 'I', 'X', '1'};
constexpr std::uint16_t index_version = 1U;
constexpr std::size_t index_identity_count = 7U;
constexpr std::size_t index_size =
    index_magic.size() + 2U + 1U + index_identity_count * 32U + 32U;

std::uint8_t hex_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  corrupt("transaction-run evidence index identity contains invalid hex");
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_identity(
    std::vector<std::uint8_t>& output,
    const std::string& identity)
{
  if (identity.size() != 64U)
    corrupt("transaction-run evidence index identity has invalid size");
  for (std::size_t index = 0U; index < identity.size(); index += 2U)
  {
    const auto high = hex_digit(identity[index]);
    const auto low = hex_digit(identity[index + 1U]);
    output.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
}

std::string read_identity(
    const std::vector<std::uint8_t>& input,
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
  return value;
}

struct evidence_index final {
  evidence_kind kind;
  session_identity journal;
  session_identity transaction;
  session_identity dispatch;
  pkgtransaction::transaction_node_identity node;
  session_identity attempt;
  session_identity record;
  session_identity result;
};

session_identity index_checksum(const evidence_index& index)
{
  return make_session_identity(
      "pkgctl/transaction-run-evidence-index/1",
      {
          kind_name(index.kind), index.journal.hex(), index.transaction.hex(),
          index.dispatch.hex(), index.node.hex(), index.attempt.hex(),
          index.record.hex(), index.result.hex(),
      });
}

std::vector<std::uint8_t> encode_index(const evidence_index& index)
{
  std::vector<std::uint8_t> output;
  output.reserve(index_size);
  output.insert(output.end(), index_magic.begin(), index_magic.end());
  append_u16(output, index_version);
  output.push_back(static_cast<std::uint8_t>(index.kind));
  append_identity(output, index.journal.hex());
  append_identity(output, index.transaction.hex());
  append_identity(output, index.dispatch.hex());
  append_identity(output, index.node.hex());
  append_identity(output, index.attempt.hex());
  append_identity(output, index.record.hex());
  append_identity(output, index.result.hex());
  append_identity(output, index_checksum(index).hex());
  if (output.size() != index_size)
    corrupt("transaction-run evidence index has invalid encoded size");
  return output;
}

evidence_index decode_index(
    const std::vector<std::uint8_t>& input,
    evidence_kind expected_kind,
    const session_identity& expected_journal,
    const session_identity& expected_dispatch,
    const session_identity& expected_attempt)
{
  if (input.size() != index_size)
    corrupt("transaction-run evidence index has invalid size");
  std::size_t offset = 0U;
  for (const auto expected : index_magic)
    if (input[offset++] != expected)
      corrupt("transaction-run evidence index has invalid magic");
  const auto version = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(input[offset]) << 8U) |
      input[offset + 1U]);
  offset += 2U;
  if (version != index_version)
    corrupt("transaction-run evidence index has unsupported version");
  const auto kind = static_cast<evidence_kind>(input[offset++]);
  if (kind != expected_kind)
    corrupt("transaction-run evidence index has the wrong kind");

  evidence_index index{
      kind,
      session_identity::from_hex(read_identity(input, offset)),
      session_identity::from_hex(read_identity(input, offset)),
      session_identity::from_hex(read_identity(input, offset)),
      pkgtransaction::transaction_node_identity::from_sha256(
          read_identity(input, offset)),
      session_identity::from_hex(read_identity(input, offset)),
      session_identity::from_hex(read_identity(input, offset)),
      session_identity::from_hex(read_identity(input, offset)),
  };
  const auto checksum = session_identity::from_hex(read_identity(input, offset));
  if (offset != input.size() || index.journal != expected_journal ||
      index.dispatch != expected_dispatch || index.attempt != expected_attempt ||
      checksum != index_checksum(index))
  {
    corrupt("transaction-run evidence index authority is invalid");
  }
  return index;
}

template<typename Record>
evidence_index make_index(evidence_kind kind, const Record& record)
{
  return evidence_index{
      kind, record.journal(), record.transaction(), record.dispatch(),
      record.node(), record.attempt_session(), record.identity(),
      record.result()};
}

template<typename Record>
void validate_loaded_record(
    const evidence_index& index,
    const Record& record)
{
  if (record.journal() != index.journal ||
      record.transaction() != index.transaction ||
      record.dispatch() != index.dispatch || record.node() != index.node ||
      record.attempt_session() != index.attempt ||
      record.identity() != index.record || record.result() != index.result)
  {
    corrupt("transaction-run evidence object contradicts its index");
  }
}

std::optional<construction_dispatch_evidence_record>
load_construction_unlocked(
    int directory_fd,
    const session_identity& journal,
    const session_identity& dispatch,
    const session_identity& attempt_session)
{
  const auto index_path = index_name(
      evidence_kind::construction, journal, dispatch, attempt_session);
  const auto index_bytes = read_file(
      directory_fd, index_path, index_size, true);
  if (!index_bytes)
    return std::nullopt;
  const auto index = decode_index(
      *index_bytes, evidence_kind::construction, journal, dispatch,
      attempt_session);
  const auto object_bytes = read_file(
      directory_fd, object_name(evidence_kind::construction, index.record),
      maximum_transaction_run_evidence_encoding_size, true);
  if (!object_bytes)
    corrupt("construction evidence index names an absent object");
  auto record = decode_construction_dispatch_evidence(*object_bytes);
  validate_loaded_record(index, record);
  return record;
}

std::optional<check_dispatch_evidence_record> load_check_unlocked(
    int directory_fd,
    const session_identity& journal,
    const session_identity& dispatch,
    const session_identity& attempt_session)
{
  const auto index_path = index_name(
      evidence_kind::check, journal, dispatch, attempt_session);
  const auto index_bytes = read_file(
      directory_fd, index_path, index_size, true);
  if (!index_bytes)
    return std::nullopt;
  const auto index = decode_index(
      *index_bytes, evidence_kind::check, journal, dispatch, attempt_session);
  const auto object_bytes = read_file(
      directory_fd, object_name(evidence_kind::check, index.record),
      maximum_transaction_run_evidence_encoding_size, true);
  if (!object_bytes)
    corrupt("check evidence index names an absent object");
  auto record = decode_check_dispatch_evidence(*object_bytes);
  validate_loaded_record(index, record);
  return record;
}

} // namespace

posix_transaction_run_evidence_store::posix_transaction_run_evidence_store(
    int directory_fd) noexcept
    : directory_fd_(directory_fd)
{
}

posix_transaction_run_evidence_store
posix_transaction_run_evidence_store::open(const std::string& directory)
{
  fd_guard fd(::open(
      directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (fd.get() < 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot open transaction-run evidence directory");
  require_directory(fd.get(), "cannot inspect transaction-run evidence directory");
  return posix_transaction_run_evidence_store(fd.release());
}

posix_transaction_run_evidence_store
posix_transaction_run_evidence_store::from_directory_fd(int directory_fd)
{
  const int duplicate = ::fcntl(directory_fd, F_DUPFD_CLOEXEC, 3);
  if (duplicate < 0)
    io_error(transaction_run_evidence_error_code::store_open_failed,
             "cannot duplicate transaction-run evidence directory");
  fd_guard fd(duplicate);
  require_directory(fd.get(), "cannot inspect transaction-run evidence directory");
  return posix_transaction_run_evidence_store(fd.release());
}

posix_transaction_run_evidence_store::posix_transaction_run_evidence_store(
    posix_transaction_run_evidence_store&& other) noexcept
    : directory_fd_(other.directory_fd_)
{
  other.directory_fd_ = -1;
}

posix_transaction_run_evidence_store&
posix_transaction_run_evidence_store::operator=(
    posix_transaction_run_evidence_store&& other) noexcept
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

posix_transaction_run_evidence_store::~posix_transaction_run_evidence_store()
{
  if (directory_fd_ >= 0)
    (void)::close(directory_fd_);
}

construction_dispatch_evidence_record
posix_transaction_run_evidence_store::publish(
    const construction_dispatch_evidence_record& record)
{
  auto lock = lock_store(directory_fd_);
  const auto index_path = index_name(
      evidence_kind::construction, record.journal(), record.dispatch(),
      record.attempt_session());
  if (const auto existing = load_construction_unlocked(
          directory_fd_, record.journal(), record.dispatch(),
          record.attempt_session()))
  {
    if (existing->identity() != record.identity())
      conflict("construction attempt already indexes different evidence");
    return *existing;
  }

  const auto object = encode_construction_dispatch_evidence(record);
  persist_immutable(
      directory_fd_, object_name(evidence_kind::construction, record.identity()),
      object, "construction-object");
  const auto index = encode_index(make_index(evidence_kind::construction, record));
  persist_immutable(directory_fd_, index_path, index, "construction-index");
  return record;
}

check_dispatch_evidence_record posix_transaction_run_evidence_store::publish(
    const check_dispatch_evidence_record& record)
{
  auto lock = lock_store(directory_fd_);
  const auto index_path = index_name(
      evidence_kind::check, record.journal(), record.dispatch(),
      record.attempt_session());
  if (const auto existing = load_check_unlocked(
          directory_fd_, record.journal(), record.dispatch(),
          record.attempt_session()))
  {
    if (existing->identity() != record.identity())
      conflict("check attempt already indexes different evidence");
    return *existing;
  }

  const auto object = encode_check_dispatch_evidence(record);
  persist_immutable(
      directory_fd_, object_name(evidence_kind::check, record.identity()),
      object, "check-object");
  const auto index = encode_index(make_index(evidence_kind::check, record));
  persist_immutable(directory_fd_, index_path, index, "check-index");
  return record;
}

std::optional<construction_dispatch_evidence_record>
posix_transaction_run_evidence_store::load_construction(
    const session_identity& journal,
    const session_identity& dispatch,
    const session_identity& attempt_session) const
{
  if (auto lock = lock_store_read_only(directory_fd_))
  {
    return load_construction_unlocked(
        directory_fd_, journal, dispatch, attempt_session);
  }

  try
  {
    auto record = load_construction_unlocked(
        directory_fd_, journal, dispatch, attempt_session);
    if (auto lock = lock_store_read_only(directory_fd_))
    {
      return load_construction_unlocked(
          directory_fd_, journal, dispatch, attempt_session);
    }
    return record;
  }
  catch (...)
  {
    if (auto lock = lock_store_read_only(directory_fd_))
    {
      return load_construction_unlocked(
          directory_fd_, journal, dispatch, attempt_session);
    }
    throw;
  }
}

std::optional<check_dispatch_evidence_record>
posix_transaction_run_evidence_store::load_check(
    const session_identity& journal,
    const session_identity& dispatch,
    const session_identity& attempt_session) const
{
  if (auto lock = lock_store_read_only(directory_fd_))
    return load_check_unlocked(
        directory_fd_, journal, dispatch, attempt_session);

  try
  {
    auto record = load_check_unlocked(
        directory_fd_, journal, dispatch, attempt_session);
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_check_unlocked(
          directory_fd_, journal, dispatch, attempt_session);
    return record;
  }
  catch (...)
  {
    if (auto lock = lock_store_read_only(directory_fd_))
      return load_check_unlocked(
          directory_fd_, journal, dispatch, attempt_session);
    throw;
  }
}

} // namespace pkgctl
