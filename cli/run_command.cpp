// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "run_command.h"

#include <pkgctl/controller.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/preparation.h>
#include <pkgctl/run_runtime.h>
#include <pkgctl/run_store.h>
#include <pkgctl/target_observation.h>

#include <libpkgapply/application_receipt_codec.h>
#include <libpkgapply/request.h>
#include <libpkgapply-posix/backend.h>
#include <libpkgapply-posix/journal_store.h>
#include <libpkgapply-posix/target_observer.h>
#include <libpkgapply-exec/result_codec.h>
#include <libpkgcatalog-codec/codec.h>
#include <libpkgexec-linux/libpkgexec-linux.h>
#include <libpkgimage/libarchive_backend.h>
#include <libpkgresolve/resolver.h>
#include <libpkgstate/generation_codec.h>
#include <libpkgstate/publication_codec.h>
#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgtransaction/composer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace pkgctl::cli {
namespace {

constexpr std::size_t maximum_private_object_size = 1024U * 1024U * 1024U;
constexpr std::string_view command_evidence_magic =
    "PKGCTL-COMMAND-EVIDENCE";
constexpr std::string_view command_evidence_checksum_domain =
    "pkgctl/command-evidence";

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
    const int result = fd_;
    fd_ = -1;
    return result;
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

[[noreturn]] void fail_io(const std::string& message, int value = errno)
{
  throw std::runtime_error(message + ": " + std::strerror(value));
}

[[nodiscard]] fd_guard open_directory(const std::filesystem::path& path)
{
  if (!path.is_absolute() || path.empty() || path != path.lexically_normal())
    throw std::invalid_argument(
        "native run path must be absolute and normalized: " + path.string());
  fd_guard result(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC |
                                       O_NOFOLLOW));
  if (result.get() < 0)
    fail_io("cannot open native run directory " + path.string());
  struct stat status{};
  if (::fstat(result.get(), &status) != 0)
    fail_io("cannot inspect native run directory " + path.string());
  if (!S_ISDIR(status.st_mode))
    throw std::runtime_error(
        "native run authority is not a directory: " + path.string());
  return result;
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> read_optional(
    int directory_fd,
    const std::string& name)
{
  fd_guard file(::openat(directory_fd, name.c_str(),
                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (file.get() < 0)
  {
    if (errno == ENOENT)
      return std::nullopt;
    fail_io("cannot open private run object " + name);
  }
  struct stat status{};
  if (::fstat(file.get(), &status) != 0)
    fail_io("cannot inspect private run object " + name);
  if (!S_ISREG(status.st_mode) || status.st_nlink != 1 ||
      (status.st_mode & 0222) != 0 || status.st_size < 0)
    throw std::runtime_error(
        "private run object is not one immutable regular file: " + name);
  if (static_cast<std::uint64_t>(status.st_size) >
      maximum_private_object_size)
    throw std::runtime_error("private run object exceeds size limit: " + name);
  std::vector<std::uint8_t> bytes(
      static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0U;
  while (offset < bytes.size())
  {
    const auto count = ::read(
        file.get(), bytes.data() + offset, bytes.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      fail_io("cannot read private run object " + name);
    }
    if (count == 0)
      throw std::runtime_error(
          "private run object was truncated while reading: " + name);
    offset += static_cast<std::size_t>(count);
  }
  return bytes;
}

[[nodiscard]] std::vector<std::uint8_t> read_all(
    int directory_fd,
    const std::string& name)
{
  auto bytes = read_optional(directory_fd, name);
  if (!bytes)
    throw std::runtime_error("private run object is absent: " + name);
  return std::move(*bytes);
}

void write_all(int fd, const std::vector<std::uint8_t>& bytes)
{
  std::size_t offset = 0U;
  while (offset < bytes.size())
  {
    const auto count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0)
    {
      if (errno == EINTR)
        continue;
      fail_io("cannot write private run object");
    }
    if (count == 0)
      throw std::runtime_error("private run object write made no progress");
    offset += static_cast<std::size_t>(count);
  }
}

[[nodiscard]] std::string temporary_name(std::string_view role)
{
  static std::atomic<unsigned long> sequence{0UL};
  return ".pkgctl-" + std::string(role) + ".tmp." +
      std::to_string(static_cast<unsigned long>(::getpid())) + "." +
      std::to_string(sequence.fetch_add(1UL, std::memory_order_relaxed) + 1UL);
}

void retain_immutable(
    int directory_fd,
    const std::string& name,
    const std::vector<std::uint8_t>& bytes,
    std::string_view role)
{
  const auto existing = read_optional(directory_fd, name);
  if (existing)
  {
    if (*existing != bytes)
      throw std::runtime_error(
          "private run object identity is already bound to other bytes: " +
          name);
    return;
  }

  const auto temporary = temporary_name(role);
  fd_guard file(::openat(directory_fd, temporary.c_str(),
                         O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                         0600));
  if (file.get() < 0)
    fail_io("cannot create private run object temporary");
  try
  {
    write_all(file.get(), bytes);
    if (::fsync(file.get()) != 0)
      fail_io("cannot synchronize private run object");
    if (::fchmod(file.get(), 0400) != 0)
      fail_io("cannot seal private run object read-only");
    if (::fsync(file.get()) != 0)
      fail_io("cannot synchronize sealed private run object");
    if (::linkat(directory_fd, temporary.c_str(), directory_fd, name.c_str(), 0)
        != 0)
    {
      if (errno != EEXIST)
        fail_io("cannot publish private run object " + name);
      const auto winner = read_all(directory_fd, name);
      if (winner != bytes)
        throw std::runtime_error(
            "private run object publication conflicted: " + name);
    }
    if (::unlinkat(directory_fd, temporary.c_str(), 0) != 0)
      fail_io("cannot remove private run temporary");
    if (::fsync(directory_fd) != 0)
      fail_io("cannot synchronize private run directory");
  }
  catch (...)
  {
    (void)::unlinkat(directory_fd, temporary.c_str(), 0);
    throw;
  }
}

void append_u64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

[[nodiscard]] std::uint64_t read_u64(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  if (bytes.size() - offset < 8U)
    throw std::runtime_error("private run record is truncated");
  std::uint64_t value = 0U;
  for (unsigned int index = 0U; index < 8U; ++index)
    value = (value << 8U) | bytes[offset++];
  return value;
}

void append_text(std::vector<std::uint8_t>& output, std::string_view value)
{
  append_u64(output, value.size());
  output.insert(output.end(), value.begin(), value.end());
}

[[nodiscard]] std::string read_text(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  const auto size = read_u64(bytes, offset);
  if (size > bytes.size() - offset)
    throw std::runtime_error("private run record text is truncated");
  std::string value(
      reinterpret_cast<const char*>(bytes.data() + offset),
      static_cast<std::size_t>(size));
  offset += static_cast<std::size_t>(size);
  return value;
}

void append_bytes(
    std::vector<std::uint8_t>& output,
    const std::vector<std::uint8_t>& value)
{
  append_u64(output, value.size());
  output.insert(output.end(), value.begin(), value.end());
}

[[nodiscard]] std::vector<std::uint8_t> read_bytes(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  const auto size = read_u64(bytes, offset);
  if (size > bytes.size() - offset)
    throw std::runtime_error("private run record body is truncated");
  std::vector<std::uint8_t> result(
      bytes.begin() + static_cast<std::ptrdiff_t>(offset),
      bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
  offset += static_cast<std::size_t>(size);
  return result;
}

[[nodiscard]] std::string checksum(
    std::string_view domain,
    const std::vector<std::uint8_t>& bytes)
{
  std::string body(
      reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return make_session_identity(std::string(domain), {body}).hex();
}

[[nodiscard]] std::size_t read_count(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset,
    std::string_view description);

void append_command_optional_text(
    std::vector<std::uint8_t>& output,
    const std::optional<std::string>& value)
{
  append_u64(output, value ? 1U : 0U);
  if (value)
    append_text(output, *value);
}

[[nodiscard]] std::optional<std::string> read_command_optional_text(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  const auto present = read_u64(bytes, offset);
  if (present > 1U)
    throw std::runtime_error("private run optional-text tag is invalid");
  if (present == 0U)
    return std::nullopt;
  return read_text(bytes, offset);
}

[[nodiscard]] std::size_t read_count(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset,
    std::string_view description)
{
  const auto value = read_u64(bytes, offset);
  if (value > static_cast<std::uint64_t>(
                  std::numeric_limits<std::size_t>::max()) ||
      value > static_cast<std::uint64_t>(bytes.size() - offset))
  {
    throw std::runtime_error(
        "private run " + std::string(description) + " count is invalid");
  }
  return static_cast<std::size_t>(value);
}

[[nodiscard]] std::uint32_t read_u32_value(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset,
    std::string_view description)
{
  const auto value = read_u64(bytes, offset);
  if (value > std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error(
        "private run " + std::string(description) + " is too large");
  return static_cast<std::uint32_t>(value);
}

void append_scope(
    std::vector<std::uint8_t>& output,
    const pkgsource::requirement_scope& scope)
{
  switch (scope.kind())
  {
    case pkgsource::requirement_scope_kind::build:
      append_u64(output, 1U);
      return;
    case pkgsource::requirement_scope_kind::run:
      append_u64(output, 2U);
      return;
    case pkgsource::requirement_scope_kind::check:
      append_u64(output, 3U);
      return;
    case pkgsource::requirement_scope_kind::lifecycle:
      append_u64(output, 4U);
      break;
  }

  if (!scope.action())
    throw std::runtime_error("retained lifecycle goal lacks an action");
  switch (*scope.action())
  {
    case pkgsource::lifecycle_action::pre_install:
      append_u64(output, 1U);
      return;
    case pkgsource::lifecycle_action::post_install:
      append_u64(output, 2U);
      return;
    case pkgsource::lifecycle_action::pre_remove:
      append_u64(output, 3U);
      return;
    case pkgsource::lifecycle_action::post_remove:
      append_u64(output, 4U);
      return;
  }
  throw std::runtime_error("retained lifecycle goal has an unknown action");
}

[[nodiscard]] pkgsource::requirement_scope read_scope(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  switch (read_u64(bytes, offset))
  {
    case 1U: return pkgsource::requirement_scope::build();
    case 2U: return pkgsource::requirement_scope::run();
    case 3U: return pkgsource::requirement_scope::check();
    case 4U:
      break;
    default:
      throw std::runtime_error("private run goal scope tag is invalid");
  }

  switch (read_u64(bytes, offset))
  {
    case 1U:
      return pkgsource::requirement_scope::lifecycle(
          pkgsource::lifecycle_action::pre_install);
    case 2U:
      return pkgsource::requirement_scope::lifecycle(
          pkgsource::lifecycle_action::post_install);
    case 3U:
      return pkgsource::requirement_scope::lifecycle(
          pkgsource::lifecycle_action::pre_remove);
    case 4U:
      return pkgsource::requirement_scope::lifecycle(
          pkgsource::lifecycle_action::post_remove);
    default:
      throw std::runtime_error("private run lifecycle-action tag is invalid");
  }
}

void append_subject(
    std::vector<std::uint8_t>& output,
    const pkgsource::requirement_subject& subject)
{
  switch (subject.kind())
  {
    case pkgsource::requirement_subject_kind::package:
      append_u64(output, 1U);
      append_text(output, subject.package().name());
      return;
    case pkgsource::requirement_subject_kind::profile:
      append_u64(output, 2U);
      append_text(output, subject.profile().name());
      return;
  }
  throw std::runtime_error("retained goal has an unknown subject kind");
}

[[nodiscard]] pkgsource::requirement_subject read_subject(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset)
{
  const auto kind = read_u64(bytes, offset);
  auto text = read_text(bytes, offset);
  if (kind == 1U)
    return pkgsource::requirement_subject(
        pkgsource::package_reference(std::move(text)));
  if (kind == 2U)
    return pkgsource::requirement_subject(
        pkgsource::profile_reference(std::move(text)));
  throw std::runtime_error("private run goal subject tag is invalid");
}

void append_transaction_request_inputs(
    std::vector<std::uint8_t>& output,
    const transaction_request& request)
{
  const auto& resolution = request.resolution();
  const auto& catalog = resolution.catalog();
  append_u64(output, catalog.collections().size());
  for (const auto& collection : catalog.collections())
  {
    append_u64(output, collection.precedence());
    append_text(output, collection.name().name());
    append_text(output, collection.root().string());
    append_command_optional_text(output, collection.external_revision());
    const auto& declaration = collection.declaration();
    append_text(output, declaration.document());
    append_text(output, declaration.path());
    append_u64(output, declaration.line());
    append_u64(output, declaration.column());
  }
  append_u64(output, catalog.limits().max_document_bytes());

  append_text(output, resolution.architectures().build().name());
  append_text(output, resolution.architectures().target().name());
  append_u64(output, resolution.goals().size());
  for (const auto& goal : resolution.goals())
  {
    append_scope(output, goal.scope());
    append_subject(output, goal.subject());
    append_text(output, goal.origin());
  }

  switch (resolution.policy().preference())
  {
    case pkgresolve::installed_preference::retain_compatible:
      append_u64(output, 1U);
      break;
    case pkgresolve::installed_preference::prefer_catalog:
      append_u64(output, 2U);
      break;
  }

  switch (request.convergence().mode())
  {
    case pkgtransaction::convergence_mode::preserve_unselected:
      append_u64(output, 1U);
      break;
    case pkgtransaction::convergence_mode::remove_explicit:
      append_u64(output, 2U);
      break;
    case pkgtransaction::convergence_mode::converge_exact:
      append_u64(output, 3U);
      break;
  }
  append_u64(output, request.convergence().removals().size());
  for (const auto& package : request.convergence().removals())
    append_text(output, package.name());
}

[[nodiscard]] transaction_request read_transaction_request_inputs(
    const std::vector<std::uint8_t>& bytes,
    std::size_t& offset,
    const std::filesystem::path& canonical_store,
    const pkgstate::state_target_binding& binding)
{
  const auto collection_count = read_count(bytes, offset, "collection");
  std::vector<pkgcatalog::acquire::collection_specification> collections;
  collections.reserve(collection_count);
  for (std::size_t index = 0U; index < collection_count; ++index)
  {
    const auto precedence = read_u32_value(bytes, offset, "precedence");
    auto name = read_text(bytes, offset);
    auto root = read_text(bytes, offset);
    auto revision = read_command_optional_text(bytes, offset);
    auto document = read_text(bytes, offset);
    auto path = read_text(bytes, offset);
    const auto line = read_u32_value(bytes, offset, "declaration line");
    const auto column = read_u32_value(bytes, offset, "declaration column");
    collections.emplace_back(
        precedence, pkgcatalog::collection_reference(std::move(name)),
        std::filesystem::path(std::move(root)), std::move(revision),
        pkgsource::declaration_provenance(
            std::move(document), std::move(path), line, column));
  }
  const auto maximum_document_bytes = read_u64(bytes, offset);
  auto catalog = catalog_request::make(
      std::move(collections),
      pkgcatalog::acquire::limits(maximum_document_bytes));

  pkgresolve::architecture_context architectures(
      pkgsource::architecture_reference(read_text(bytes, offset)),
      pkgsource::architecture_reference(read_text(bytes, offset)));
  const auto goal_count = read_count(bytes, offset, "goal");
  std::vector<pkgresolve::resolution_goal> goals;
  goals.reserve(goal_count);
  for (std::size_t index = 0U; index < goal_count; ++index)
  {
    auto scope = read_scope(bytes, offset);
    auto subject = read_subject(bytes, offset);
    auto origin = read_text(bytes, offset);
    goals.emplace_back(
        std::move(scope), std::move(subject), std::move(origin));
  }

  pkgresolve::resolution_policy policy([&]() {
    switch (read_u64(bytes, offset))
    {
      case 1U: return pkgresolve::installed_preference::retain_compatible;
      case 2U: return pkgresolve::installed_preference::prefer_catalog;
      default:
        throw std::runtime_error(
            "private run resolution-policy tag is invalid");
    }
  }());

  const auto convergence_tag = read_u64(bytes, offset);
  const auto removal_count = read_count(bytes, offset, "explicit removal");
  std::vector<pkgsource::package_reference> removals;
  removals.reserve(removal_count);
  for (std::size_t index = 0U; index < removal_count; ++index)
    removals.emplace_back(read_text(bytes, offset));

  pkgtransaction::convergence_policy convergence = [&]() {
    switch (convergence_tag)
    {
      case 1U:
        if (!removals.empty())
          throw std::runtime_error(
              "private run preserve convergence carries removals");
        return pkgtransaction::convergence_policy::preserve_unselected();
      case 2U:
        return pkgtransaction::convergence_policy::remove_explicit(
            std::move(removals));
      case 3U:
        if (!removals.empty())
          throw std::runtime_error(
              "private run exact convergence carries removals");
        return pkgtransaction::convergence_policy::converge_exact();
      default:
        throw std::runtime_error(
            "private run convergence-policy tag is invalid");
    }
  }();

  auto resolution = resolution_request::make(
      std::move(catalog), state_location::make(canonical_store, binding),
      std::move(architectures), std::move(goals), std::move(policy));
  return transaction_request::make(
      std::move(resolution), std::move(convergence));
}

struct retained_native_execution_profiles final {
  pkgexec::backend_capability_profile construction;
  pkgexec::backend_capability_profile check;
  pkgexec::backend_capability_profile lifecycle;
};

struct retained_command_evidence final {
  session_identity transaction;
  transaction_request request;
  pkgcatalog::catalog_snapshot catalog;
  pkgstate::snapshot state;
  pkgexec::interpreter_identity interpreter;
  retained_native_execution_profiles execution_profiles;
};

class command_evidence_store final {
public:
  explicit command_evidence_store(const std::filesystem::path& directory)
      : directory_(open_directory(directory))
  {
  }

  void retain(
      const transaction_run_nonce& nonce,
      const transaction_session& transaction,
      const pkgexec::interpreter_identity& interpreter,
      const retained_native_execution_profiles& execution_profiles)
  {
    const auto catalog = pkgcatalog::encode_catalog_snapshot(
        transaction.resolution().catalog().catalog());
    const auto state = pkgstate::encode_generation_snapshot(
        transaction.resolution().installed());
    std::vector<std::uint8_t> bytes;
    append_text(bytes, command_evidence_magic);
    std::vector<std::uint8_t> request;
    append_transaction_request_inputs(request, transaction.request());
    append_text(bytes, transaction.identity().hex());
    append_bytes(bytes, catalog);
    append_bytes(bytes, state);
    append_bytes(bytes, request);
    append_text(bytes, interpreter.hex());
    append_bytes(bytes, pkgexec::encode_backend_capability_profile(
        execution_profiles.construction));
    append_bytes(bytes, pkgexec::encode_backend_capability_profile(
        execution_profiles.check));
    append_bytes(bytes, pkgexec::encode_backend_capability_profile(
        execution_profiles.lifecycle));
    append_text(bytes, checksum(command_evidence_checksum_domain, bytes));
    retain_immutable(
        directory_.get(), name(nonce), bytes, "command-evidence");
  }

  [[nodiscard]] retained_command_evidence load(
      const transaction_run_nonce& nonce,
      const std::filesystem::path& canonical_store) const
  {
    const auto bytes = read_all(directory_.get(), name(nonce));
    std::size_t offset = 0U;
    if (read_text(bytes, offset) != command_evidence_magic)
      throw std::runtime_error("command evidence has invalid magic");
    const auto transaction = session_identity::from_hex(read_text(bytes, offset));
    const auto catalog_encoding = read_bytes(bytes, offset);
    const auto state_encoding = read_bytes(bytes, offset);
    const auto request_encoding = read_bytes(bytes, offset);
    auto interpreter = pkgexec::interpreter_identity::from_sha256(
        read_text(bytes, offset));
    const auto construction_profile_encoding = read_bytes(bytes, offset);
    const auto check_profile_encoding = read_bytes(bytes, offset);
    const auto lifecycle_profile_encoding = read_bytes(bytes, offset);
    retained_native_execution_profiles execution_profiles{
        pkgexec::decode_backend_capability_profile(construction_profile_encoding),
        pkgexec::decode_backend_capability_profile(check_profile_encoding),
        pkgexec::decode_backend_capability_profile(lifecycle_profile_encoding)};
    const auto checksum_offset = offset;
    const auto expected_checksum = read_text(bytes, offset);
    if (offset != bytes.size())
      throw std::runtime_error("command evidence has trailing bytes");
    std::vector<std::uint8_t> prefix(
        bytes.begin(),
        bytes.begin() + static_cast<std::ptrdiff_t>(checksum_offset));
    if (expected_checksum != checksum(command_evidence_checksum_domain, prefix))
      throw std::runtime_error("command evidence checksum is invalid");

    auto catalog = pkgcatalog::decode_catalog_snapshot(catalog_encoding);
    auto state = pkgstate::decode_generation_snapshot(std::string_view(
        reinterpret_cast<const char*>(state_encoding.data()),
        state_encoding.size()));
    std::size_t request_offset = 0U;
    auto request = read_transaction_request_inputs(
        request_encoding, request_offset, canonical_store,
        state.target_binding());
    if (request_offset != request_encoding.size())
      throw std::runtime_error("command evidence request has trailing bytes");
    return {transaction, std::move(request), std::move(catalog),
            std::move(state), std::move(interpreter),
            std::move(execution_profiles)};
  }

private:
  [[nodiscard]] static std::string name(const transaction_run_nonce& nonce)
  {
    return "command-" + nonce.hex() + ".pce";
  }

  fd_guard directory_;
};

[[nodiscard]] transaction_session recompose_transaction(
    transaction_request request,
    pkgcatalog::catalog_snapshot catalog,
    pkgstate::snapshot installed)
{
  auto catalog_session_value = catalog_session::seal(
      request.resolution().catalog(), std::move(catalog));
  auto native_resolution_request = pkgresolve::resolution_request::seal(
      catalog_session_value.catalog(), installed,
      request.resolution().architectures(), request.resolution().goals(),
      request.resolution().policy());
  auto resolution = pkgresolve::resolve(std::move(native_resolution_request));
  auto resolution_session_value = resolution_session::seal(
      request.resolution(), std::move(catalog_session_value), installed,
      std::move(resolution));
  auto native_transaction_request = pkgtransaction::transaction_request::seal(
      resolution_session_value.resolution(), request.convergence());
  auto program = pkgtransaction::compose(std::move(native_transaction_request));
  return transaction_session::seal(
      std::move(request), std::move(resolution_session_value),
      std::move(program));
}


[[nodiscard]] std::string private_body_name(
    std::string_view role,
    std::string_view identity)
{
  const std::vector<std::uint8_t> bytes(identity.begin(), identity.end());
  return std::string(role) + "-" +
      checksum("pkgctl/private-effect-body-key/1", bytes) + ".bin";
}

[[nodiscard]] pkgapply::application_receipt decode_application_body(
    const std::vector<std::uint8_t>& bytes,
    const pkgapply::package_application_request& request)
{
  return std::visit(
      [&](const auto& body) {
        return pkgapply::decode_application_receipt(bytes, body);
      },
      request.body());
}

class private_effect_body_store final
    : public transaction_effect_body_sink,
      public transaction_effect_restart_body_source {
public:
  private_effect_body_store(
      const std::filesystem::path& directory,
      pkgapply::posix::application_journal_store& application_journals)
      : directory_(open_directory(directory)),
        application_journals_(application_journals)
  {
  }

  void retain_lifecycle(
      const pkgapply_exec::lifecycle_execution_result& result) override
  {
    const auto bytes = pkgapply_exec::encode_lifecycle_execution_result(result);
    retain_immutable(
        directory_.get(),
        private_body_name("lifecycle", result.identity().hex()), bytes,
        "lifecycle-body");
  }

  void retain_application(
      const pkgapply::package_application_request& request,
      const pkgapply::application_receipt& receipt) override
  {
    if (receipt.request() != request.identity())
      throw std::runtime_error(
          "application body belongs to another request");
    const auto bytes = pkgapply::encode_application_receipt(receipt);
    retain_immutable(
        directory_.get(),
        private_body_name("application", receipt.identity().string()), bytes,
        "application-body");
  }

  void retain_publication_request(
      const pkgstate::state_publication_request& request) override
  {
    const auto bytes = pkgstate::encode_state_publication_request(request);
    retain_immutable(
        directory_.get(),
        private_body_name("publication-request", request.identity().string()),
        bytes, "publication-request-body");
  }

  void retain_publication_receipt(
      const pkgstate::state_publication_request& request,
      const pkgstate::state_publication_receipt& receipt) override
  {
    if (receipt.request() != request.identity() ||
        receipt.actual_prior_snapshot() != request.expected_snapshot())
    {
      throw std::runtime_error(
          "publication receipt cannot be retained without exact prior-state authority");
    }
    const auto bytes = pkgstate::encode_state_publication_receipt(receipt);
    retain_immutable(
        directory_.get(),
        private_body_name("publication-receipt", receipt.identity().string()),
        bytes, "publication-receipt-body");
  }

  transaction_effect_restart_bodies load(
      const effectful_operation_session& session,
      const effect_attempt_record& record) override
  {
    transaction_effect_restart_bodies result;
    if (record.before().size() > session.before().size() ||
        record.after().size() > session.after().size())
      throw std::runtime_error(
          "effect journal lifecycle facts exceed admitted sessions");

    result.before.reserve(record.before().size());
    for (std::size_t index = 0U; index < record.before().size(); ++index)
    {
      const auto& fact = record.before()[index];
      auto body = pkgapply_exec::decode_lifecycle_execution_result(
          read_all(
              directory_.get(),
              private_body_name("lifecycle", fact.result().hex())),
          session.before()[index]);
      if (body.identity().hex() != fact.result().hex() ||
          body.succeeded() != fact.succeeded())
        throw std::runtime_error(
            "retained pre-application lifecycle body contradicts journal fact");
      result.before.push_back(std::move(body));
    }

    if (record.application())
    {
      const auto& fact = *record.application();
      auto body = decode_application_body(
          read_all(
              directory_.get(),
              private_body_name("application", fact.receipt())),
          session.request().application());
      if (body.identity().string() != fact.receipt() ||
          body.outcome() != fact.outcome())
        throw std::runtime_error(
            "retained application body contradicts journal fact");
      result.application = std::move(body);
    }

    result.after.reserve(record.after().size());
    for (std::size_t index = 0U; index < record.after().size(); ++index)
    {
      const auto& fact = record.after()[index];
      auto body = pkgapply_exec::decode_lifecycle_execution_result(
          read_all(
              directory_.get(),
              private_body_name("lifecycle", fact.result().hex())),
          session.after()[index]);
      if (body.identity().hex() != fact.result().hex() ||
          body.succeeded() != fact.succeeded())
        throw std::runtime_error(
            "retained post-application lifecycle body contradicts journal fact");
      result.after.push_back(std::move(body));
    }

    if (record.publication_request())
    {
      auto body = pkgstate::decode_state_publication_request(
          read_all(
              directory_.get(), private_body_name(
                  "publication-request", *record.publication_request())),
          session.request().expected_state());
      if (body.identity().string() != *record.publication_request())
        throw std::runtime_error(
            "retained publication request contradicts journal fact");
      result.publication_request = std::move(body);
    }

    if (record.publication())
    {
      if (!result.publication_request)
        throw std::runtime_error(
            "publication receipt fact lacks retained publication request");
      const auto& fact = *record.publication();
      auto body = pkgstate::decode_state_publication_receipt(
          read_all(
              directory_.get(),
              private_body_name("publication-receipt", fact.receipt())),
          *result.publication_request,
          session.request().expected_state());
      if (body.identity().string() != fact.receipt() ||
          body.outcome() != fact.outcome())
        throw std::runtime_error(
            "retained publication receipt contradicts journal fact");
      result.publication_receipt = std::move(body);
    }

    if (record.stage() == effect_attempt_stage::application_intent)
    {
      result.application_journal = application_journals_.load_active(
          session.request().application().identity());
      if (!result.application_journal)
        throw std::runtime_error(
            "interrupted application intent has no active application journal");

      const auto application_restart = pkgapply::assess_application_restart(
          *result.application_journal);
      if (application_restart.disposition() ==
              pkgapply::application_restart_disposition::terminal &&
          result.application_journal->receipt())
      {
        const auto retained = read_optional(
            directory_.get(),
            private_body_name(
                "application",
                result.application_journal->receipt()->string()));
        if (retained)
        {
          auto body = decode_application_body(
              *retained, session.request().application());
          if (body.identity() != *result.application_journal->receipt())
            throw std::runtime_error(
                "retained ahead application body contradicts terminal journal");
          result.application = std::move(body);
        }
      }
    }

    return result;
  }

private:
  fd_guard directory_;
  pkgapply::posix::application_journal_store& application_journals_;
};


template<typename Identity>
[[nodiscard]] Identity derived_digest_identity(
    std::string_view domain,
    const std::vector<std::string>& fields)
{
  return Identity::parse(
      "v1:sha256:" + make_session_identity(std::string(domain), fields).hex());
}

[[nodiscard]] std::vector<std::uint8_t> encode_operation_archive(
    const retained_transaction_effect_archive& archive)
{
  if (!archive.path.is_absolute() || archive.path.empty() ||
      archive.path != archive.path.lexically_normal())
    throw std::runtime_error(
        "retained operation archive path is not absolute and normalized");

  std::vector<std::uint8_t> bytes;
  append_text(bytes, "PKGCTL-OPERATION-ARCHIVE-1");
  append_text(bytes, archive.incoming.string());
  append_text(bytes, archive.path.string());
  append_text(bytes, checksum("pkgctl/operation-archive/1", bytes));
  return bytes;
}

[[nodiscard]] retained_transaction_effect_archive decode_operation_archive(
    const std::vector<std::uint8_t>& bytes,
    const pkgapply::incoming_package_authority_identity& expected)
{
  std::size_t offset = 0U;
  if (read_text(bytes, offset) != "PKGCTL-OPERATION-ARCHIVE-1")
    throw std::runtime_error("retained operation archive has invalid magic");
  auto incoming = pkgapply::incoming_package_authority_identity::parse(
      read_text(bytes, offset));
  if (incoming != expected)
    throw std::runtime_error(
        "retained operation archive belongs to another incoming authority");
  std::filesystem::path path(read_text(bytes, offset));
  if (!path.is_absolute() || path.empty() ||
      path != path.lexically_normal())
    throw std::runtime_error(
        "retained operation archive path is not absolute and normalized");

  const auto checksum_offset = offset;
  const auto retained_checksum = read_text(bytes, offset);
  if (offset != bytes.size())
    throw std::runtime_error(
        "retained operation archive has trailing bytes");
  std::vector<std::uint8_t> prefix(
      bytes.begin(),
      bytes.begin() + static_cast<std::ptrdiff_t>(checksum_offset));
  if (retained_checksum != checksum("pkgctl/operation-archive/1", prefix))
    throw std::runtime_error(
        "retained operation archive checksum is invalid");
  return {std::move(incoming), std::move(path)};
}

void add_path_closure(
    std::set<std::string>& paths,
    const std::string& path)
{
  if (path.empty())
    throw std::runtime_error("operation path authority is empty");
  paths.insert(path);
  std::size_t separator = path.rfind('/');
  while (separator != std::string::npos)
  {
    if (separator != 0U)
      paths.insert(path.substr(0U, separator));
    separator = separator == 0U
        ? std::string::npos
        : path.rfind('/', separator - 1U);
  }
}

[[nodiscard]] std::vector<pkgtransaction::transaction_node_identity>
lifecycle_side(
    const pkgtransaction::transaction_program& program,
    const pkgtransaction::transaction_node_identity& action,
    pkgtransaction::phase_order_kind phase)
{
  std::set<std::string> members;
  std::map<std::string, pkgtransaction::transaction_node_identity> identities;
  for (const auto& edge : program.edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::phase ||
        !edge.phase_order() || *edge.phase_order() != phase)
      continue;
    std::optional<pkgtransaction::transaction_node_identity> member;
    if (phase == pkgtransaction::phase_order_kind::pre_lifecycle_before_action &&
        edge.after() == action)
      member = edge.before();
    if (phase == pkgtransaction::phase_order_kind::action_before_post_lifecycle &&
        edge.before() == action)
      member = edge.after();
    if (!member)
      continue;
    const auto* node = program.find(*member);
    if (node == nullptr ||
        node->action() != pkgtransaction::transaction_action_kind::lifecycle)
      throw std::runtime_error(
          "transaction lifecycle phase edge names a non-lifecycle node");
    members.insert(member->hex());
    identities.emplace(member->hex(), *member);
  }

  std::map<std::string, std::set<std::string>> outgoing;
  std::map<std::string, std::size_t> indegree;
  for (const auto& member : members)
    indegree.emplace(member, 0U);
  for (const auto& edge : program.edges())
  {
    if (edge.kind() != pkgtransaction::transaction_edge_kind::requirement)
      continue;
    const auto before = edge.before().hex();
    const auto after = edge.after().hex();
    if (!members.count(before) || !members.count(after))
      continue;
    if (outgoing[before].insert(after).second)
      ++indegree[after];
  }

  std::set<std::string> ready;
  for (const auto& entry : indegree)
    if (entry.second == 0U)
      ready.insert(entry.first);
  std::vector<pkgtransaction::transaction_node_identity> result;
  while (!ready.empty())
  {
    const auto current = *ready.begin();
    ready.erase(ready.begin());
    result.push_back(identities.at(current));
    for (const auto& successor : outgoing[current])
      if (--indegree[successor] == 0U)
        ready.insert(successor);
  }
  if (result.size() != members.size())
    throw std::runtime_error("lifecycle ordering requirements contain a cycle");
  return result;
}

[[nodiscard]] pkgstate::installation_reason installation_reason_for(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node& action)
{
  for (const auto& reason : action.reasons())
    if (reason.kind() == pkgresolve::selection_reason_kind::direct_goal)
      return pkgstate::installation_reason::explicit_request();

  for (const auto& reason : action.reasons())
  {
    if (reason.kind() != pkgresolve::selection_reason_kind::profile_goal ||
        !reason.profile() || !reason.profile_identity())
      continue;
    return pkgstate::installation_reason::profile_membership(
        pkgstate::profile_reference(reason.profile()->name()),
        pkgstate::source_profile_identity::parse(
            "v1:sha256:" + reason.profile_identity()->hex()));
  }

  for (const auto& reason : action.reasons())
  {
    if (reason.kind() != pkgresolve::selection_reason_kind::runtime_requirement ||
        !reason.issuer())
      continue;
    for (const auto& selection :
         transaction.resolution().resolution().selections())
      if (selection.identity() == *reason.issuer())
        return pkgstate::installation_reason::runtime_dependency(
            pkgstate::package_reference(selection.package().name()));
    throw std::runtime_error(
        "runtime installation reason names an absent issuer selection");
  }
  throw std::runtime_error(
      "installation action has no durable installation reason");
}


[[nodiscard]] pkgplan::runtime_dependency_closure_identity runtime_closure_for(
    const transaction_session& transaction,
    const pkgtransaction::transaction_node& action)
{
  const auto* root = action.selection();
  if (root == nullptr)
    throw std::runtime_error(
        "incoming operation lacks selected package authority");

  std::set<std::string> members{root->identity().hex()};
  std::set<std::string> witnesses;
  std::vector<std::string> pending{root->identity().hex()};
  const auto& resolution = transaction.resolution().resolution();
  while (!pending.empty())
  {
    const auto issuer = std::move(pending.back());
    pending.pop_back();
    for (const auto& edge : resolution.edges())
    {
      if (edge.environment() != action.environment() ||
          edge.scope().kind() != pkgsource::requirement_scope_kind::run ||
          edge.issuer().hex() != issuer)
        continue;
      witnesses.insert(edge.identity().hex());
      if (members.insert(edge.required().hex()).second)
        pending.push_back(edge.required().hex());
    }
  }

  std::vector<std::string> fields{
      resolution.identity().hex(), root->identity().hex(),
      std::to_string(static_cast<unsigned int>(action.environment()))};
  for (const auto& member : members)
    fields.push_back("selection:" + member);
  for (const auto& witness : witnesses)
    fields.push_back("edge:" + witness);
  return derived_digest_identity<
      pkgplan::runtime_dependency_closure_identity>(
          "pkgctl/native-runtime-dependency-closure/1", fields);
}

class live_operation_authority final
    : public transaction_operation_specification_source,
      public transaction_effect_archive_source,
      public transaction_operation_session_store {
public:
  live_operation_authority(
      transaction_session transaction,
      pkgapply::posix::application_target_observer& observer,
      pkgimage::archive_backend& archives,
      pkgapply::application_target_context target,
      const std::filesystem::path& body_directory)
      : transaction_(std::move(transaction)), observer_(observer),
        archives_(archives), target_(std::move(target)),
        body_directory_(open_directory(body_directory)),
        control_(pkgapply::application_execution_control::make(
            pkgapply::application_recovery_requirement::exact_prior_state,
            pkgapply::application_durability_requirement::all_application_domains,
            pkgapply::application_cancellation_policy::recover_after_target_mutation)),
        policy_(
            derived_digest_identity<pkgplan::policy_snapshot_identity>(
                "pkgctl/native-command-policy/1",
                {transaction_.identity().hex(), target_.identity().string()}),
            pkgplan::normalized_path_policy(
                pkgplan::incoming_path_policy::activate(),
                pkgplan::obsolete_path_policy::remove(),
                pkgplan::shared_ownership_policy::forbid,
                pkgplan::directory_cleanup_policy::remove_if_empty),
            {})
  {
  }

  native_transaction_operation_specification operation(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch) override
  {
    if (progress.transaction().identity() != transaction_.identity() ||
        dispatch.unit().kind() != transaction_unit_kind::operation)
      throw std::runtime_error(
          "live operation authority received another transaction or unit kind");
    pending_archive_.reset();
    const auto* action = transaction_.program().find(
        dispatch.unit().primary_node());
    if (action == nullptr)
      throw std::runtime_error("operation action node is absent");

    std::optional<construction_result> construction;
    std::optional<pkgbuild::plan_adapter::artifact_projection> artifact;
    if (action->action() == pkgtransaction::transaction_action_kind::install ||
        action->action() == pkgtransaction::transaction_action_kind::upgrade)
    {
      const construction_result* found = nullptr;
      for (const auto& edge : transaction_.program().edges())
      {
        if (edge.kind() != pkgtransaction::transaction_edge_kind::phase ||
            !edge.phase_order() ||
            *edge.phase_order() !=
                pkgtransaction::phase_order_kind::build_before_target ||
            edge.after() != action->identity())
          continue;
        const auto* candidate = progress.construction(edge.before());
        if (candidate == nullptr || !candidate->succeeded())
          throw std::runtime_error(
              "incoming operation lacks successful predecessor construction");
        if (found != nullptr && found->identity() != candidate->identity())
          throw std::runtime_error(
              "incoming operation has ambiguous predecessor construction");
        found = candidate;
      }
      if (found == nullptr)
        throw std::runtime_error(
            "incoming operation lacks predecessor construction authority");
      construction = *found;
      native_operation_preparation_driver projector;
      artifact = projector.project_artifact(*construction);
      pending_archive_ = construction->session().paths().build.artifact_path;
    }

    std::set<std::string> paths;
    std::vector<pkgapply::posix::target_hardlink_expectation> hardlinks;
    if (artifact)
    {
      for (const auto& entry : artifact->authority().image().image().entries())
      {
        add_path_closure(paths, entry.path.string());
        if (entry.type == pkgimage::entry_type::hardlink && entry.hardlink_target)
          hardlinks.emplace_back(
              pkgplan::package_path::parse(entry.path.string()),
              pkgplan::package_path::parse(entry.hardlink_target->string()));
      }
    }

    const pkgstate::installed_package* installed = nullptr;
    if (action->action() == pkgtransaction::transaction_action_kind::upgrade ||
        action->action() == pkgtransaction::transaction_action_kind::remove)
    {
      installed = progress.current_state().find_package(action->package().name());
      if (installed == nullptr)
        throw std::runtime_error(
            "target operation lacks its exact installed package");
      for (const auto& entry : installed->manifest())
      {
        add_path_closure(paths, entry.path().string());
        if (entry.object().hardlink_anchor())
          hardlinks.emplace_back(
              pkgplan::package_path::parse(entry.path().string()),
              pkgplan::package_path::parse(
                  entry.object().hardlink_anchor()->string()));
      }
    }

    std::vector<pkgplan::package_path> requested;
    requested.reserve(paths.size());
    for (const auto& path : paths)
      requested.push_back(pkgplan::package_path::parse(path));
    auto observation_set = operation_observations(
        record, progress, dispatch, requested, std::move(hardlinks));

    const auto lifecycle = lifecycle_order::make(
        lifecycle_side(
            transaction_.program(), action->identity(),
            pkgtransaction::phase_order_kind::pre_lifecycle_before_action),
        lifecycle_side(
            transaction_.program(), action->identity(),
            pkgtransaction::phase_order_kind::action_before_post_lifecycle));

    if (action->action() == pkgtransaction::transaction_action_kind::remove)
      return native_transaction_operation_specification::remove(
          action->identity(), target_, control_, std::move(observation_set),
          policy_, lifecycle);

    const auto closure = runtime_closure_for(transaction_, *action);

    if (action->action() == pkgtransaction::transaction_action_kind::install)
      return native_transaction_operation_specification::install(
          action->identity(), target_, control_, std::move(observation_set),
          closure, policy_, lifecycle,
          installation_reason_for(transaction_, *action));
    if (action->action() == pkgtransaction::transaction_action_kind::upgrade)
      return native_transaction_operation_specification::upgrade(
          action->identity(), target_, control_, std::move(observation_set),
          closure, policy_, lifecycle);
    throw std::runtime_error(
        "live operation authority received a non-mutating action node");
  }

  void retain(
      const effectful_operation_session& session,
      operation_session_encoding encoding) override
  {
    const auto* incoming = session.request().application().incoming();
    if ((incoming != nullptr) != pending_archive_.has_value())
      throw std::runtime_error(
          "fresh operation session and retained archive path disagree");

    if (incoming != nullptr)
    {
      const retained_transaction_effect_archive archive{
          incoming->identity(), *pending_archive_};
      retain_immutable(
          body_directory_.get(),
          operation_archive_name(archive.incoming),
          encode_operation_archive(archive),
          "operation-archive");
    }
    retain_immutable(
        body_directory_.get(), operation_session_name(session.identity()),
        encoding, "operation-session");
    pending_archive_.reset();
  }

  std::optional<operation_session_encoding> load(
      const session_identity& session) override
  {
    return read_optional(
        body_directory_.get(), operation_session_name(session));
  }

  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority& incoming) override
  {
    const auto retained = decode_operation_archive(
        read_all(
            body_directory_.get(),
            operation_archive_name(incoming.identity())),
        incoming.identity());
    return archives_.open(pkgimage::archive_inspection_request{
        retained.path, incoming.image().receipt().archive_digest()});
  }

private:
  [[nodiscard]] static const transaction_dispatch_record& retained_dispatch(
      const transaction_run_journal_record& record,
      const transaction_dispatch& dispatch)
  {
    const auto found = std::find_if(
        record.dispatches().begin(), record.dispatches().end(),
        [&](const auto& candidate) {
          return candidate.dispatch().identity() == dispatch.identity();
        });
    if (found == record.dispatches().end())
      throw std::runtime_error(
          "operation authority lacks its durable dispatch record");
    return *found;
  }

  [[nodiscard]] static std::string operation_session_name(
      const session_identity& session)
  {
    return private_body_name("operation-session", session.hex());
  }

  [[nodiscard]] static std::string operation_archive_name(
      const pkgapply::incoming_package_authority_identity& incoming)
  {
    return private_body_name("operation-archive", incoming.string());
  }

  [[nodiscard]] pkgplan::target_observation_set operation_observations(
      const transaction_run_journal_record& record,
      const transaction_progress& progress,
      const transaction_dispatch& dispatch,
      const std::vector<pkgplan::package_path>& requested,
      std::vector<pkgapply::posix::target_hardlink_expectation> hardlinks)
  {
    const auto& retained = retained_dispatch(record, dispatch);
    if (retained.state() != transaction_dispatch_state::reserved)
      throw std::runtime_error(
          "live operation authority is fresh-dispatch authority only");

    return observe_native_target_paths(
        record, progress, dispatch, target_.target(), observer_,
        requested, std::move(hardlinks));
  }

  transaction_session transaction_;
  pkgapply::posix::application_target_observer& observer_;
  pkgimage::archive_backend& archives_;
  pkgapply::application_target_context target_;
  fd_guard body_directory_;
  pkgapply::application_execution_control control_;
  pkgplan::package_policy_snapshot policy_;
  std::optional<std::filesystem::path> pending_archive_;
};

class explicit_installed_package_source final
    : public retained_installed_package_tree_source {
public:
  explicit explicit_installed_package_source(
      std::vector<installed_tree_option> entries)
  {
    for (auto& entry : entries)
    {
      if (!entry.path.is_absolute() || entry.path.empty() ||
          entry.path != entry.path.lexically_normal())
        throw std::invalid_argument(
            "installed package tree path must be absolute and normalized");
      const auto key = entry.package.string();
      if (!entries_.emplace(
              key, retained_installed_package_tree{
                       std::move(entry.package), std::move(entry.resource),
                       std::move(entry.path)}).second)
        throw std::invalid_argument(
            "installed package tree supplied more than once: " + key);
    }
  }

  retained_installed_package_tree locate(
      const pkgstate::installed_package& package) override
  {
    const auto found = entries_.find(package.identity().string());
    if (found == entries_.end())
      throw std::runtime_error(
          "no retained package tree for installed package " +
          package.identity().string());
    return found->second;
  }

private:
  std::map<std::string, retained_installed_package_tree> entries_;
};

[[nodiscard]] std::filesystem::path runtime_path(
    const transaction_run_command& command,
    std::string_view name)
{
  if (!command.runtime_root.is_absolute() || command.runtime_root.empty() ||
      command.runtime_root != command.runtime_root.lexically_normal())
    throw std::invalid_argument(
        "native runtime root must be absolute and normalized");
  return command.runtime_root / std::string(name);
}

[[nodiscard]] std::vector<std::uint64_t> current_supplementary_groups()
{
  const int count = ::getgroups(0, nullptr);
  if (count < 0)
    fail_io("cannot inspect current supplementary groups");
  std::vector<gid_t> native(static_cast<std::size_t>(count));
  if (count > 0 && ::getgroups(count, native.data()) < 0)
    fail_io("cannot inspect current supplementary groups");
  std::vector<std::uint64_t> result;
  result.reserve(native.size());
  for (const auto group : native)
    result.push_back(static_cast<std::uint64_t>(group));
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  result.erase(std::remove(
      result.begin(), result.end(), static_cast<std::uint64_t>(::getgid())),
      result.end());
  return result;
}

[[nodiscard]] bool current_credentials(
    const pkgexec::credential_policy& credentials)
{
  return credentials.user_id() == static_cast<std::uint64_t>(::getuid()) &&
      credentials.group_id() == static_cast<std::uint64_t>(::getgid()) &&
      credentials.supplementary_groups() == current_supplementary_groups();
}

struct native_execution_scope final {
  bool construction = false;
  bool check = false;
  bool lifecycle = false;

  [[nodiscard]] bool any() const noexcept
  {
    return construction || check || lifecycle;
  }
};

void include_native_execution_scope(
    native_execution_scope& scopes,
    pkgtransaction::transaction_action_kind action)
{
  switch (action)
  {
    case pkgtransaction::transaction_action_kind::build:
      scopes.construction = true;
      return;
    case pkgtransaction::transaction_action_kind::check:
      scopes.check = true;
      return;
    case pkgtransaction::transaction_action_kind::lifecycle:
      scopes.lifecycle = true;
      return;
    case pkgtransaction::transaction_action_kind::install:
    case pkgtransaction::transaction_action_kind::upgrade:
    case pkgtransaction::transaction_action_kind::retain:
    case pkgtransaction::transaction_action_kind::remove:
      return;
  }
}

[[nodiscard]] native_execution_scope native_execution_scopes(
    const pkgtransaction::transaction_program& program)
{
  native_execution_scope result;
  for (const auto& node : program.nodes())
    include_native_execution_scope(result, node.action());
  return result;
}

[[nodiscard]] bool contains_node(
    const std::vector<pkgtransaction::transaction_node_identity>& nodes,
    const pkgtransaction::transaction_node_identity& needle)
{
  return std::find(nodes.begin(), nodes.end(), needle) != nodes.end();
}

void include_unit_nodes(
    std::vector<pkgtransaction::transaction_node_identity>& result,
    const ready_transaction_unit& unit)
{
  for (const auto& node : unit.members())
  {
    if (!contains_node(result, node))
      result.push_back(node);
  }
}

[[nodiscard]] bool effect_recovery_stops_run(
    const effect_attempt_record& record)
{
  const auto assessment = assess_effect_restart(record);
  if (!assessment.automatically_continuable())
    return true;

  if (assessment.disposition() == effect_restart_disposition::terminal)
  {
    return record.terminal_outcome() !=
        std::optional<effectful_operation_outcome>(
            effectful_operation_outcome::completed);
  }

  if (assessment.disposition() != effect_restart_disposition::seal_terminal)
    return false;

  switch (record.stage())
  {
    case effect_attempt_stage::before_lifecycle_terminal:
      return !record.before().back().succeeded();
    case effect_attempt_stage::application_terminal:
      return record.application()->outcome() !=
          pkgapply::application_attempt_outcome::completed;
    case effect_attempt_stage::after_lifecycle_terminal:
      return !record.after().back().succeeded();
    case effect_attempt_stage::publication_terminal:
      return record.publication()->outcome() !=
          pkgstate::state_publication_outcome::published;
    case effect_attempt_stage::admitted:
    case effect_attempt_stage::before_lifecycle_intent:
    case effect_attempt_stage::application_intent:
    case effect_attempt_stage::after_lifecycle_intent:
    case effect_attempt_stage::publication_intent:
    case effect_attempt_stage::terminal:
      break;
  }
  throw std::runtime_error(
      "seal-terminal effect assessment has an invalid journal stage");
}

[[nodiscard]] bool effect_recovery_may_execute_lifecycle(
    const effect_attempt_record& record)
{
  const auto disposition = assess_effect_restart(record).disposition();
  switch (disposition)
  {
    case effect_restart_disposition::continue_before_lifecycle:
    case effect_restart_disposition::start_application:
    case effect_restart_disposition::resume_application:
    case effect_restart_disposition::continue_after_application:
    case effect_restart_disposition::continue_after_lifecycle:
      return record.before().size() < record.before_total() ||
          record.after().size() < record.after_total();
    case effect_restart_disposition::start_publication:
    case effect_restart_disposition::reconcile_publication:
    case effect_restart_disposition::seal_terminal:
    case effect_restart_disposition::terminal:
    case effect_restart_disposition::external_resolution_required:
      return false;
  }
  return false;
}

[[nodiscard]] native_execution_scope resume_native_execution_scopes(
    const pkgtransaction::transaction_program& program,
    const transaction_run_journal_record& record,
    const effect_journal_store& effects)
{
  if (record.complete() || record.failed() || record.stopped())
    return {};

  native_execution_scope result;
  std::vector<pkgtransaction::transaction_node_identity> owned;
  for (const auto& retained : record.dispatches())
  {
    if (retained.state() != transaction_dispatch_state::completed &&
        retained.state() != transaction_dispatch_state::started)
    {
      continue;
    }

    include_unit_nodes(owned, retained.dispatch().unit());
    if (retained.state() != transaction_dispatch_state::started ||
        retained.dispatch().unit().kind() != transaction_unit_kind::operation)
    {
      continue;
    }

    if (!retained.observations().empty())
      return {};

    if (!retained.effect_attempt())
    {
      for (const auto& member : retained.dispatch().unit().members())
      {
        const auto* node = program.find(member);
        if (node == nullptr)
          throw std::runtime_error(
              "retained operation dispatch names an unknown transaction node");
        include_native_execution_scope(result, node->action());
      }
      continue;
    }

    const auto effect = effects.load_latest(*retained.effect_attempt());
    if (!effect)
      throw std::runtime_error(
          "started operation dispatch lacks retained effect evidence");
    if (effect_recovery_stops_run(*effect))
      return {};
    if (effect_recovery_may_execute_lifecycle(*effect))
      result.lifecycle = true;
  }

  for (const auto& node : program.nodes())
  {
    if (!contains_node(owned, node.identity()))
      include_native_execution_scope(result, node.action());
  }
  return result;
}

[[nodiscard]] std::vector<pkgexec::execution_guarantee>
native_execution_requirements(const native_execution_scope& scopes)
{
  if (!scopes.any())
    return {};

  std::vector<pkgexec::execution_guarantee> required{
      pkgexec::execution_guarantee::exact_interpreter,
      pkgexec::execution_guarantee::closed_environment,
      pkgexec::execution_guarantee::root_view,
      pkgexec::execution_guarantee::writable_resources,
      pkgexec::execution_guarantee::fixed_credentials,
      pkgexec::execution_guarantee::network_denied,
      pkgexec::execution_guarantee::complete_stdout_capture,
      pkgexec::execution_guarantee::complete_stderr_capture,
      pkgexec::execution_guarantee::cleanup_verified,
  };
  if (scopes.construction || scopes.check)
    required.push_back(pkgexec::execution_guarantee::read_only_resources);
  std::sort(required.begin(), required.end());
  return required;
}

[[nodiscard]] bool relevant_native_capability(
    pkgexec_linux::capability_kind capability) noexcept
{
  switch (capability)
  {
    case pkgexec_linux::capability_kind::closed_environment:
    case pkgexec_linux::capability_kind::current_root_view:
    case pkgexec_linux::capability_kind::current_credentials:
    case pkgexec_linux::capability_kind::writable_resources:
    case pkgexec_linux::capability_kind::complete_stream_capture:
    case pkgexec_linux::capability_kind::process_group_containment:
    case pkgexec_linux::capability_kind::descriptor_execution:
    case pkgexec_linux::capability_kind::mount_namespace:
    case pkgexec_linux::capability_kind::private_mount_propagation:
    case pkgexec_linux::capability_kind::openat2:
    case pkgexec_linux::capability_kind::open_tree:
    case pkgexec_linux::capability_kind::move_mount:
    case pkgexec_linux::capability_kind::mount_setattr:
    case pkgexec_linux::capability_kind::chroot:
    case pkgexec_linux::capability_kind::capability_drop:
    case pkgexec_linux::capability_kind::network_namespace:
      return true;
    case pkgexec_linux::capability_kind::process_supervision:
    case pkgexec_linux::capability_kind::no_new_privileges:
    case pkgexec_linux::capability_kind::close_range:
    case pkgexec_linux::capability_kind::pidfd:
    case pkgexec_linux::capability_kind::pidfd_cancellation:
    case pkgexec_linux::capability_kind::user_namespace:
    case pkgexec_linux::capability_kind::pid_namespace:
    case pkgexec_linux::capability_kind::landlock:
    case pkgexec_linux::capability_kind::cgroup_v2:
    case pkgexec_linux::capability_kind::loopback_configuration:
    case pkgexec_linux::capability_kind::address_space_limit:
    case pkgexec_linux::capability_kind::file_size_limit:
    case pkgexec_linux::capability_kind::open_files_limit:
      return false;
  }
  return false;
}

void require_native_execution_preflight(
    const native_execution_scope& scopes,
    const pkgexec_linux::capability_report& report,
    const retained_native_execution_profiles* retained_profiles = nullptr)
{
  const auto required = native_execution_requirements(scopes);
  if (required.empty())
    return;

  std::vector<pkgexec::execution_guarantee> missing;
  const auto& available = report.profile().guarantees();
  for (const auto guarantee : required)
  {
    if (!std::binary_search(available.begin(), available.end(), guarantee))
      missing.push_back(guarantee);
  }

  std::vector<std::string> missing_authorities;

  if (retained_profiles != nullptr)
  {
    const auto& current = report.profile();
    if (scopes.construction && current != retained_profiles->construction)
    {
      missing_authorities.push_back(
          "construction backend capability profile differs from admitted run authority");
    }
    if (scopes.check && current != retained_profiles->check)
    {
      missing_authorities.push_back(
          "check backend capability profile differs from admitted run authority");
    }
    if (scopes.lifecycle && current != retained_profiles->lifecycle)
    {
      missing_authorities.push_back(
          "lifecycle backend capability profile differs from admitted run authority");
    }
  }

  if (missing.empty() && missing_authorities.empty())
    return;

  std::string message =
      "native execution unavailable before transaction execution";
  if (!missing.empty())
  {
    message += "; missing guarantees:";
    for (const auto guarantee : missing)
      message += " " + std::string(pkgexec::to_string(guarantee));
  }
  for (const auto& authority : missing_authorities)
    message += "\n  " + authority;

  for (const auto& observation : report.observations())
  {
    if (observation.state() == pkgexec_linux::capability_state::available ||
        !relevant_native_capability(observation.capability()))
      continue;
    message += "\n  ";
    message += pkgexec_linux::to_string(observation.capability());
    message += "=";
    message += pkgexec_linux::to_string(observation.state());
    if (!observation.diagnostic().empty())
    {
      message += ": ";
      message += observation.diagnostic();
    }
  }
  throw std::runtime_error(message);
}

void require_native_execution_credentials(
    const native_execution_scope& scopes,
    const pkgexec::credential_policy& build_credentials,
    const pkgexec::credential_policy& lifecycle_credentials)
{
  std::vector<std::string> missing;
  if ((scopes.construction || scopes.check) &&
      !current_credentials(build_credentials))
  {
    missing.push_back(
        "construction/check credentials must match the native supervisor");
  }
  if (scopes.lifecycle && !current_credentials(lifecycle_credentials))
    missing.push_back("lifecycle credentials must match the native supervisor");

  if (missing.empty())
    return;

  std::string message =
      "native execution unavailable before transaction execution";
  for (const auto& authority : missing)
    message += "\n  " + authority;
  throw std::runtime_error(message);
}

template<typename Destination, typename Source>
[[nodiscard]] Destination translate_identity(const Source& source)
{
  return Destination::parse(source.string());
}

[[nodiscard]] pkgexec::root_view_identity execution_root_identity(
    const pkgstate::state_target_binding& binding)
{
  const auto text = binding.root_view().string();
  static constexpr std::string_view prefix = "v1:sha256:";
  if (text.compare(0U, prefix.size(), prefix) != 0)
    throw std::runtime_error("state root-view identity is not sha256-v1");
  return pkgexec::root_view_identity::from_sha256(text.substr(prefix.size()));
}

[[nodiscard]] pkgapply::application_target_context command_application_target(
    const transaction_run_command& command,
    const transaction_session& transaction,
    const pkgstate::state_target_binding& binding,
    const pkgexec::interpreter_identity& interpreter,
    const pkgexec::backend_capability_profile& execution_capabilities)
{
  const std::vector<std::string> common{
      transaction.identity().hex(), binding.identity().string(),
      command.target_root.string(), command.runtime_root.string(),
      "libpkgapply-posix/3.1"};
  auto fields = [&](std::string role, std::filesystem::path path = {}) {
    auto result = common;
    result.insert(result.begin(), std::move(role));
    if (!path.empty())
      result.push_back(path.string());
    return result;
  };

  return pkgapply::application_target_context::make(
      derived_digest_identity<pkgplan::target_system_context_identity>(
          "pkgctl/native-command-target-system/1", common),
      translate_identity<pkgapply::managed_target_identity>(
          binding.managed_target()),
      translate_identity<pkgapply::root_view_identity>(binding.root_view()),
      derived_digest_identity<pkgapply::observation_backend_identity>(
          "pkgctl/native-command-observation-backend/1",
          fields("observation", command.target_root)),
      [&]() {
        auto mutation_fields = fields("mutation", command.target_root);
        for (const auto* name : {
                 "application-journals", "application-checkpoints", "payload",
                 "capture", "rejected", "completed"})
          mutation_fields.push_back(runtime_path(command, name).string());
        return derived_digest_identity<pkgapply::mutation_backend_identity>(
            "pkgctl/native-command-mutation-backend/1", mutation_fields);
      }(),
      derived_digest_identity<pkgapply::mutation_exclusion_domain_identity>(
          "pkgctl/native-command-mutation-domain/1",
          fields("mutation-domain", runtime_path(command, "target-locks"))),
      derived_digest_identity<pkgapply::active_object_namespace_identity>(
          "pkgctl/native-command-active-namespace/1",
          fields("active", command.target_root)),
      derived_digest_identity<pkgapply::rejected_object_store_identity>(
          "pkgctl/native-command-rejected-store/1",
          fields("rejected", runtime_path(command, "rejected"))),
      derived_digest_identity<pkgapply::staging_namespace_identity>(
          "pkgctl/native-command-staging-namespace/1",
          fields("staging", runtime_path(command, "payload"))),
      derived_digest_identity<pkgapply::journal_namespace_identity>(
          "pkgctl/native-command-journal-namespace/1",
          fields("journal", runtime_path(command, "application-journals"))),
      derived_digest_identity<pkgapply::execution_capability_profile_identity>(
          "pkgctl/native-command-application-capabilities/1",
          fields("application-capabilities")),
      [&]() {
        std::vector<std::string> lifecycle_fields{
            transaction.identity().hex(), interpreter.hex(),
            std::to_string(command.lifecycle_credentials.user_id()),
            std::to_string(command.lifecycle_credentials.group_id()),
            command.lifecycle_root.string(), command.target_root.string(),
            execution_capabilities.identity().hex()};
        for (const auto group : command.lifecycle_credentials.supplementary_groups())
          lifecycle_fields.push_back("supplementary:" + std::to_string(group));
        return derived_digest_identity<pkgapply::lifecycle_executor_identity>(
            "pkgctl/native-command-lifecycle-executor/1", lifecycle_fields);
      }());
}

[[nodiscard]] native_transaction_session_configuration command_sessions(
    const transaction_run_command& command,
    const pkgstate::state_target_binding& binding,
    const pkgexec::interpreter_identity& interpreter)
{
  return native_transaction_session_configuration::make(
      {
          runtime_path(command, "content"),
          runtime_path(command, "construction-sessions"),
          runtime_path(command, "package-outputs"),
          runtime_path(command, "artifacts"),
          runtime_path(command, "check-temporary"),
          execution_root_identity(binding),
          command.build_root,
      },
      {
          pkgbuild::build_policy::make(
              pkgbuild::environment_policy::hermetic(
                  1U, 0022, command.source_date_epoch)),
          pkgfetch::acquisition_policy::defaults(),
          {interpreter, command.build_credentials.user_id(),
           command.build_credentials.group_id(),
           command.build_credentials.supplementary_groups()},
          {interpreter, command.build_credentials.user_id(),
           command.build_credentials.group_id(),
           command.build_credentials.supplementary_groups()},
          pkgexec::resource_limits::make(),
          pkgbuild::artifact_compression::none,
      });
}

[[nodiscard]] native_transaction_operation_configuration command_operations(
    const transaction_run_command& command,
    const transaction_session& transaction,
    const pkgstate::state_target_binding& binding,
    const pkgexec::interpreter_identity& interpreter)
{
  return native_transaction_operation_configuration::make(
      transaction,
      {
          execution_root_identity(binding),
          command.lifecycle_root,
          command.target_root,
          runtime_path(command, "lifecycle-sessions"),
          {interpreter, command.lifecycle_credentials.user_id(),
           command.lifecycle_credentials.group_id(),
           command.lifecycle_credentials.supplementary_groups()},
      });
}

[[nodiscard]] const char* drive_disposition_name(
    transaction_run_drive_disposition disposition) noexcept
{
  switch (disposition)
  {
    case transaction_run_drive_disposition::completed: return "completed";
    case transaction_run_drive_disposition::stopped_after_failure:
      return "stopped-after-failure";
    case transaction_run_drive_disposition::external_resolution_required:
      return "external-resolution-required";
    case transaction_run_drive_disposition::quiescent_incomplete:
      return "quiescent-incomplete";
    case transaction_run_drive_disposition::step_limit_reached:
      return "step-limit-reached";
    case transaction_run_drive_disposition::mutation_authority_unavailable:
      return "mutation-authority-unavailable";
  }
  return "unknown";
}

std::string_view application_outcome_name(
    pkgapply::application_attempt_outcome outcome) noexcept
{
  switch (outcome)
  {
    case pkgapply::application_attempt_outcome::precondition_refused:
      return "precondition-refused";
    case pkgapply::application_attempt_outcome::failed_before_target_mutation:
      return "failed-before-target-mutation";
    case pkgapply::application_attempt_outcome::completed:
      return "completed";
    case pkgapply::application_attempt_outcome::failed_fully_recovered:
      return "failed-fully-recovered";
    case pkgapply::application_attempt_outcome::failed_with_partial_effects:
      return "failed-with-partial-effects";
    case pkgapply::application_attempt_outcome::
        effects_visible_durability_unconfirmed:
      return "effects-visible-durability-unconfirmed";
    case pkgapply::application_attempt_outcome::indeterminate:
      return "indeterminate";
  }
  return "unknown";
}

std::string_view publication_outcome_name(
    pkgstate::state_publication_outcome outcome) noexcept
{
  switch (outcome)
  {
    case pkgstate::state_publication_outcome::published:
      return "published";
    case pkgstate::state_publication_outcome::stale_expected_state:
      return "stale-expected-state";
    case pkgstate::state_publication_outcome::request_rejected:
      return "request-rejected";
    case pkgstate::state_publication_outcome::failed_before_publication:
      return "failed-before-publication";
    case pkgstate::state_publication_outcome::
        published_durability_unconfirmed:
      return "published-durability-unconfirmed";
    case pkgstate::state_publication_outcome::indeterminate:
      return "indeterminate";
  }
  return "unknown";
}

std::string_view operation_outcome_name(
    effectful_operation_outcome outcome) noexcept
{
  switch (outcome)
  {
    case effectful_operation_outcome::lifecycle_failed_before_application:
      return "lifecycle-failed-before-application";
    case effectful_operation_outcome::application_not_completed:
      return "application-not-completed";
    case effectful_operation_outcome::lifecycle_failed_after_application:
      return "lifecycle-failed-after-application";
    case effectful_operation_outcome::outer_lease_lost:
      return "outer-lease-lost";
    case effectful_operation_outcome::state_publication_not_completed:
      return "state-publication-not-completed";
    case effectful_operation_outcome::state_publication_indeterminate:
      return "state-publication-indeterminate";
    case effectful_operation_outcome::completed:
      return "completed";
  }
  return "unknown";
}

void render_captured_stderr(
    std::string_view stage,
    const pkgexec::execution_result& execution)
{
  if (!execution.standard_error() ||
      !execution.standard_error()->material() ||
      execution.standard_error()->material()->empty())
    return;

  const auto& material = *execution.standard_error()->material();
  std::cerr << "pkgctl: " << stage << " stderr:\n" << material;
  if (material.back() != '\n')
    std::cerr << '\n';
}

void render_execution_failure(
    std::string_view stage,
    std::string_view diagnostic,
    const pkgexec::execution_result& execution)
{
  std::cerr << "pkgctl: " << stage << " failed";
  if (!diagnostic.empty())
    std::cerr << ": " << diagnostic;
  std::cerr << '\n';
  render_captured_stderr(stage, execution);
}

void render_operation_failure(const effectful_operation_result& operation)
{
  std::cerr << "pkgctl: operation failed: "
            << operation_outcome_name(operation.outcome()) << '\n';

  for (const auto& lifecycle : operation.before())
  {
    if (!lifecycle.succeeded())
    {
      render_execution_failure(
          "pre-application lifecycle",
          lifecycle.execution().diagnostic(), lifecycle.execution());
      return;
    }
  }

  if (operation.application() &&
      operation.application()->outcome() !=
          pkgapply::application_attempt_outcome::completed)
  {
    std::cerr << "pkgctl: application outcome: "
              << application_outcome_name(operation.application()->outcome())
              << '\n';
    return;
  }

  for (const auto& lifecycle : operation.after())
  {
    if (!lifecycle.succeeded())
    {
      render_execution_failure(
          "post-application lifecycle",
          lifecycle.execution().diagnostic(), lifecycle.execution());
      return;
    }
  }

  if (operation.publication_receipt() &&
      operation.publication_receipt()->outcome() !=
          pkgstate::state_publication_outcome::published)
  {
    std::cerr << "pkgctl: state publication outcome: "
              << publication_outcome_name(
                     operation.publication_receipt()->outcome())
              << '\n';
  }
}

void render_terminal_failure(const transaction_run_drive_result& result)
{
  if (result.disposition() !=
          transaction_run_drive_disposition::stopped_after_failure ||
      result.steps().empty())
    return;

  const auto& last = result.last();
  if (const auto* construction = last.construction())
  {
    render_execution_failure(
        "construction", construction->build().diagnostic(),
        construction->build().execution());
    return;
  }

  if (const auto* check = last.check())
  {
    const auto& execution = check->execution().execution();
    render_execution_failure("check", execution.diagnostic(), execution);
    return;
  }

  if (const auto* operation = last.operation())
  {
    if (operation->result)
      render_operation_failure(*operation->result);
    else
      std::cerr << "pkgctl: operation failed without terminal result body\n";
  }
}

void render_run_result(
    const transaction_session& transaction,
    const transaction_run_drive_result& result,
    bool admitted)
{
  std::cout << "transaction " << transaction.identity().hex() << '\n'
            << "journal " << result.record().journal().hex() << '\n'
            << "record " << result.record().identity().hex() << '\n'
            << "sequence " << result.record().sequence() << '\n'
            << "origin " << (admitted ? "admitted" : "resumed") << '\n'
            << "disposition " << drive_disposition_name(result.disposition())
            << '\n'
            << "steps " << result.steps().size() << '\n'
            << "durable-steps " << result.durable_step_count() << '\n'
            << "complete " << (result.run().progress().complete() ? "yes" : "no")
            << '\n'
            << "failed " << (result.run().progress().failed() ? "yes" : "no")
            << '\n';
}

[[nodiscard]] int drive_status(
    transaction_run_drive_disposition disposition) noexcept
{
  switch (disposition)
  {
    case transaction_run_drive_disposition::completed:
    case transaction_run_drive_disposition::step_limit_reached:
      return EXIT_SUCCESS;
    case transaction_run_drive_disposition::stopped_after_failure:
    case transaction_run_drive_disposition::external_resolution_required:
    case transaction_run_drive_disposition::quiescent_incomplete:
    case transaction_run_drive_disposition::mutation_authority_unavailable:
      return EXIT_FAILURE;
  }
  return EXIT_FAILURE;
}

} // namespace

int execute_transaction_run(transaction_run_command command)
{
  (void)open_directory(command.runtime_root);
  (void)open_directory(command.build_root);

  command_evidence_store command_evidence(
      runtime_path(command, "command-evidence"));
  std::optional<retained_command_evidence> retained_evidence;
  transaction_session transaction = [&]() {
    if (command.intent == transaction_run_command_intent::start)
    {
      if (!command.transaction)
        throw std::runtime_error(
            "fresh run lacks explicit transaction authority");
      return compose_transaction(std::move(*command.transaction));
    }
    if (command.transaction)
      throw std::runtime_error(
          "resumed run carries duplicate transaction authority");
    retained_evidence.emplace(
        command_evidence.load(command.nonce, command.canonical_store));
    auto composed = recompose_transaction(
        std::move(retained_evidence->request),
        std::move(retained_evidence->catalog),
        std::move(retained_evidence->state));
    if (composed.identity() != retained_evidence->transaction)
      throw std::runtime_error(
          "retained command evidence recomposes another transaction");
    return composed;
  }();

  const auto& binding = transaction.resolution().installed().target_binding();

  auto run_store = open_directory(runtime_path(command, "run"));
  auto evidence_store = open_directory(runtime_path(command, "evidence"));
  auto effect_store = open_directory(runtime_path(command, "effects"));
  const bool operation_runtime =
      native_transaction_requires_target_operation_authority(transaction);

  const auto dispatch_policy = transaction_dispatch_policy::make(1U, 1U);
  const auto expected_admission = transaction_run_journal_record::admit(
      transaction_run::begin(
          transaction_progress::begin(transaction), dispatch_policy),
      command.nonce);
  auto admission_store =
      posix_transaction_run_journal_store::from_directory_fd(run_store.get());
  const auto existing = admission_store.load_latest(expected_admission.journal());
  if (command.intent == transaction_run_command_intent::start && existing)
    throw std::runtime_error(
        "exact transaction run is already admitted; use --resume");
  if (command.intent == transaction_run_command_intent::resume && !existing)
    throw std::runtime_error(
        "exact transaction run is not admitted; use --start");

  auto effect_inspection_store =
      posix_effect_journal_store::from_directory_fd(effect_store.get());
  const auto execution_scopes = command.intent == transaction_run_command_intent::start
      ? native_execution_scopes(transaction.program())
      : resume_native_execution_scopes(
            transaction.program(), *existing, effect_inspection_store);

  require_native_execution_credentials(
      execution_scopes, command.build_credentials,
      command.lifecycle_credentials);

  std::optional<pkgexec_linux::interpreter_binding> current_interpreter;
  std::unique_ptr<pkgexec_linux::isolated_backend> current_execution_backend;
  if (command.intent == transaction_run_command_intent::start ||
      execution_scopes.any())
  {
    current_interpreter.emplace(
        pkgexec_linux::interpreter_binding::inspect(command.interpreter));
    current_execution_backend.reset(
        new pkgexec_linux::isolated_backend(
            pkgexec_linux::isolated_backend::make({*current_interpreter})));
  }

  retained_native_execution_profiles admitted_execution_profiles = [&]() {
    if (retained_evidence)
      return retained_evidence->execution_profiles;
    const auto profile = current_execution_backend->capabilities();
    return retained_native_execution_profiles{profile, profile, profile};
  }();
  const auto admitted_interpreter = retained_evidence
      ? retained_evidence->interpreter
      : current_interpreter->identity();

  if (current_execution_backend)
  {
    if (retained_evidence &&
        current_interpreter->identity() != admitted_interpreter)
    {
      throw std::runtime_error(
          "current interpreter differs from admitted run authority");
    }
    require_native_execution_preflight(
        execution_scopes, current_execution_backend->report(),
        command.intent == transaction_run_command_intent::resume
            ? &admitted_execution_profiles
            : nullptr);
  }

  pkgimage::libarchive_backend archive_backend;
  explicit_installed_package_source installed_packages(
      std::move(command.installed_trees));

  if (command.intent == transaction_run_command_intent::start)
  {
    command_evidence.retain(
        command.nonce, transaction, admitted_interpreter,
        admitted_execution_profiles);
  }

  const auto drive_policy =
      transaction_run_drive_policy::make(command.maximum_steps);
  const auto drive_runtime = [&](native_posix_transaction_run_runtime& runtime) {
    if (command.intent == transaction_run_command_intent::start)
    {
      const auto result = runtime.launch(
          dispatch_policy, command.nonce, drive_policy);
      render_run_result(transaction, result.drive(), true);
      render_terminal_failure(result.drive());
      return drive_status(result.drive().disposition());
    }

    const auto result = runtime.drive(
        expected_admission.journal(), drive_policy);
    render_run_result(transaction, result, false);
    render_terminal_failure(result);
    return drive_status(result.disposition());
  };

  if (!operation_runtime)
  {
    auto configuration = native_transaction_run_runtime_configuration::make(
        transaction, command_sessions(command, binding, admitted_interpreter));
    auto runtime = native_posix_transaction_run_runtime::from_directory_fds(
        run_store.get(), evidence_store.get(), effect_store.get(),
        std::move(configuration), {installed_packages},
        {current_execution_backend.get(), current_execution_backend.get(),
         archive_backend});
    return drive_runtime(*runtime);
  }
  else
  {
    (void)open_directory(command.lifecycle_root);
    auto target_root = open_directory(command.target_root);
    auto target_locks = open_directory(runtime_path(command, "target-locks"));
    auto application_journal_directory = open_directory(
        runtime_path(command, "application-journals"));
    auto application_checkpoint_directory = open_directory(
        runtime_path(command, "application-checkpoints"));
    auto payload_directory = open_directory(runtime_path(command, "payload"));
    auto capture_directory = open_directory(runtime_path(command, "capture"));
    auto rejected_directory = open_directory(runtime_path(command, "rejected"));
    auto completed_directory = open_directory(runtime_path(command, "completed"));

    auto application_target = command_application_target(
        command, transaction, binding, admitted_interpreter,
        admitted_execution_profiles.lifecycle);
    auto target_observer =
        pkgapply::posix::application_target_observer::from_directory_fd(
            target_root.get());
    auto application_journals =
        pkgapply::posix::application_journal_store::from_directory_fd(
            application_journal_directory.get());
    private_effect_body_store effect_bodies(
        runtime_path(command, "effect-bodies"), application_journals);
    auto application_backend =
        pkgapply::posix::application_posix_backend::from_directory_fds(
            application_target, target_root.get(),
            application_journal_directory.get(),
            application_checkpoint_directory.get(), payload_directory.get(),
            capture_directory.get(), rejected_directory.get(),
            completed_directory.get());
    auto state_store =
        pkgstate::posix::canonical_generation_store::open_existing(
            command.canonical_store, binding);
    live_operation_authority operation_authority(
        transaction, target_observer, archive_backend, application_target,
        runtime_path(command, "effect-bodies"));

    auto configuration = native_transaction_run_runtime_configuration::make(
        transaction, command_sessions(command, binding, admitted_interpreter),
        command_operations(command, transaction, binding, admitted_interpreter),
        {});
    auto runtime = native_posix_transaction_run_runtime::from_directory_fds(
        run_store.get(), evidence_store.get(), effect_store.get(),
        target_locks.get(), std::move(configuration),
        {installed_packages, operation_authority, effect_bodies,
         &operation_authority, &effect_bodies, &operation_authority},
        {current_execution_backend.get(), current_execution_backend.get(),
         *application_backend, current_execution_backend.get(), state_store,
         archive_backend});
    return drive_runtime(*runtime);
  }
}

} // namespace pkgctl::cli
