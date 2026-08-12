// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_evidence_codec.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <openssl/evp.h>

namespace pkgctl {
namespace {

constexpr std::array<std::uint8_t, 8> construction_magic{
    'P', 'K', 'G', 'C', 'E', 'V', '0', '1'};
constexpr std::array<std::uint8_t, 8> check_magic{
    'P', 'K', 'G', 'K', 'E', 'V', '0', '1'};
constexpr std::size_t checksum_size = 32U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::corrupt_encoding, message);
}

[[noreturn]] void unsupported(const std::string& message)
{
  throw transaction_run_evidence_error(
      transaction_run_evidence_error_code::unsupported_encoding, message);
}

std::string sha256_hex(const std::uint8_t* bytes, std::size_t size)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (size != 0U && EVP_DigestUpdate(context.get(), bytes, size) != 1))
  {
    corrupt("cannot hash transaction-run evidence encoding");
  }
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_size = 0U;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1 ||
      digest_size != checksum_size)
  {
    corrupt("cannot finalize transaction-run evidence checksum");
  }
  static constexpr char digits[] = "0123456789abcdef";
  std::string result(checksum_size * 2U, '0');
  for (std::size_t index = 0U; index < checksum_size; ++index)
  {
    result[index * 2U] = digits[(digest[index] >> 4U) & 0x0fU];
    result[index * 2U + 1U] = digits[digest[index] & 0x0fU];
  }
  return result;
}

std::string encoding_digest(const std::vector<std::uint8_t>& encoding)
{
  return sha256_hex(encoding.data(), encoding.size());
}

std::uint8_t hexadecimal_digit(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  corrupt("transaction-run evidence identity contains invalid hex");
}

class writer final {
public:
  void raw(const std::uint8_t* bytes, std::size_t size)
  {
    if (size > maximum_transaction_run_evidence_encoding_size - output_.size())
      corrupt("transaction-run evidence encoding exceeds maximum size");
    if (size == 0U)
      return;
    output_.insert(output_.end(), bytes, bytes + size);
  }

  template<std::size_t Size>
  void raw(const std::array<std::uint8_t, Size>& bytes)
  {
    raw(bytes.data(), bytes.size());
  }

  void u16(std::uint16_t value)
  {
    output_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    output_.push_back(static_cast<std::uint8_t>(value & 0xffU));
  }

  void u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      output_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void identity(const std::string& hex)
  {
    if (hex.size() != 64U)
      corrupt("transaction-run evidence identity has invalid size");
    for (std::size_t index = 0U; index < hex.size(); index += 2U)
    {
      const auto high = hexadecimal_digit(hex[index]);
      const auto low = hexadecimal_digit(hex[index + 1U]);
      output_.push_back(static_cast<std::uint8_t>((high << 4U) | low));
    }
  }

  void bytes(const std::vector<std::uint8_t>& value)
  {
    u64(static_cast<std::uint64_t>(value.size()));
    raw(value.data(), value.size());
  }

  [[nodiscard]] const transaction_run_evidence_encoding& output() const noexcept
  {
    return output_;
  }

  [[nodiscard]] transaction_run_evidence_encoding finish()
  {
    const auto checksum = sha256_hex(output_.data(), output_.size());
    identity(checksum);
    if (output_.size() > maximum_transaction_run_evidence_encoding_size)
      corrupt("transaction-run evidence encoding exceeds maximum size");
    return std::move(output_);
  }

private:
  transaction_run_evidence_encoding output_;
};

class reader final {
public:
  reader(const transaction_run_evidence_encoding& input, std::size_t limit)
      : input_(input), limit_(limit)
  {
  }

  std::uint8_t byte()
  {
    require(1U);
    return input_[offset_++];
  }

  std::uint16_t u16()
  {
    const auto high = byte();
    const auto low = byte();
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high) << 8U) | low);
  }

  std::uint64_t u64()
  {
    std::uint64_t value = 0U;
    for (unsigned int index = 0U; index < 8U; ++index)
      value = (value << 8U) | byte();
    return value;
  }

  std::string identity()
  {
    static constexpr char digits[] = "0123456789abcdef";
    require(32U);
    std::string value(64U, '0');
    for (std::size_t index = 0U; index < 32U; ++index)
    {
      const auto current = input_[offset_++];
      value[index * 2U] = digits[(current >> 4U) & 0x0fU];
      value[index * 2U + 1U] = digits[current & 0x0fU];
    }
    return value;
  }

  std::vector<std::uint8_t> bytes(std::size_t maximum)
  {
    const auto size64 = u64();
    if (size64 > maximum || size64 > std::numeric_limits<std::size_t>::max())
      corrupt("transaction-run evidence subordinate encoding is too large");
    const auto size = static_cast<std::size_t>(size64);
    require(size);
    std::vector<std::uint8_t> value(
        input_.begin() + static_cast<std::ptrdiff_t>(offset_),
        input_.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
    offset_ += size;
    return value;
  }

  void finish() const
  {
    if (offset_ != limit_)
      corrupt("transaction-run evidence encoding has trailing data");
  }

private:
  void require(std::size_t size) const
  {
    if (size > limit_ - offset_)
      corrupt("transaction-run evidence encoding is truncated");
  }

  const transaction_run_evidence_encoding& input_;
  std::size_t limit_;
  std::size_t offset_ = 0U;
};

void validate_checksum(const transaction_run_evidence_encoding& encoding)
{
  if (encoding.size() < construction_magic.size() + 2U + checksum_size)
    corrupt("transaction-run evidence encoding is truncated");
  if (encoding.size() > maximum_transaction_run_evidence_encoding_size)
    corrupt("transaction-run evidence encoding exceeds maximum size");
  const auto payload_size = encoding.size() - checksum_size;
  const auto expected = sha256_hex(encoding.data(), payload_size);
  static constexpr char digits[] = "0123456789abcdef";
  std::string retained(64U, '0');
  for (std::size_t index = 0U; index < checksum_size; ++index)
  {
    const auto current = encoding[payload_size + index];
    retained[index * 2U] = digits[(current >> 4U) & 0x0fU];
    retained[index * 2U + 1U] = digits[current & 0x0fU];
  }
  if (retained != expected)
    corrupt("transaction-run evidence encoding checksum mismatch");
}

void read_magic(reader& input, const std::array<std::uint8_t, 8>& expected)
{
  for (const auto value : expected)
    if (input.byte() != value)
      corrupt("transaction-run evidence encoding has invalid magic");
  if (input.u16() != transaction_run_evidence_schema_version)
    unsupported("transaction-run evidence encoding version is unsupported");
}

session_identity construction_record_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& result,
    const session_identity& controller_request,
    const construction_session_encoding& session_encoding,
    const pkgfetch::materialization_identity& materialization,
    const pkgfetch::source_materialization_encoding& materialization_encoding,
    const pkgbuild::build_request_identity& build_request,
    const pkgexec::execution_request_identity& execution_request,
    const pkgexec::backend_capability_profile_identity& backend,
    const pkgexec::execution_evidence_identity& execution,
    const pkgbuild::build_result_identity& build,
    const pkgbuild_exec::build_execution_result_encoding& encoding)
{
  return make_session_identity(
      "pkgctl/construction-dispatch-evidence/3",
      {journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
       attempt_session.hex(), result.hex(), controller_request.hex(),
       encoding_digest(session_encoding), materialization.hex(),
       encoding_digest(materialization_encoding),
       build_request.hex(), execution_request.hex(), backend.hex(),
       execution.hex(), build.hex(), encoding_digest(encoding)});
}

session_identity check_record_identity(
    const session_identity& journal,
    const session_identity& transaction,
    const session_identity& dispatch,
    const pkgtransaction::transaction_node_identity& node,
    const session_identity& attempt_session,
    const session_identity& result,
    const session_identity& controller_request,
    const session_identity& construction,
    const pkgcheck::check_request_identity& check_request,
    const pkgexec::execution_request_identity& execution_request,
    const pkgexec::backend_capability_profile_identity& backend,
    const pkgexec::execution_evidence_identity& execution,
    const pkgcheck::check_result_identity& check,
    const pkgcheck_exec::check_execution_result_encoding& encoding)
{
  return make_session_identity(
      "pkgctl/check-dispatch-evidence/3",
      {journal.hex(), transaction.hex(), dispatch.hex(), node.hex(),
       attempt_session.hex(), result.hex(), controller_request.hex(),
       construction.hex(), check_request.hex(), execution_request.hex(),
       backend.hex(), execution.hex(), check.hex(), encoding_digest(encoding)});
}

} // namespace

struct detail_run_evidence_codec_access final {
  static construction_dispatch_evidence_record construction(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      session_identity dispatch,
      pkgtransaction::transaction_node_identity node,
      session_identity attempt_session,
      session_identity result,
      session_identity controller_request,
      construction_session_encoding session_encoding,
      pkgfetch::materialization_identity materialization,
      pkgfetch::source_materialization_encoding materialization_encoding,
      pkgbuild::build_request_identity build_request,
      pkgexec::execution_request_identity execution_request,
      pkgexec::backend_capability_profile_identity backend,
      pkgexec::execution_evidence_identity execution,
      pkgbuild::build_result_identity build,
      pkgbuild_exec::build_execution_result_encoding encoding)
  {
    return construction_dispatch_evidence_record(
        std::move(identity), std::move(journal), std::move(transaction),
        std::move(dispatch), std::move(node), std::move(attempt_session),
        std::move(result), std::move(controller_request),
        std::move(session_encoding), std::move(materialization),
        std::move(materialization_encoding),
        std::move(build_request), std::move(execution_request),
        std::move(backend),
        std::move(execution), std::move(build), std::move(encoding));
  }

  static check_dispatch_evidence_record check(
      session_identity identity,
      session_identity journal,
      session_identity transaction,
      session_identity dispatch,
      pkgtransaction::transaction_node_identity node,
      session_identity attempt_session,
      session_identity result,
      session_identity controller_request,
      session_identity construction,
      pkgcheck::check_request_identity check_request,
      pkgexec::execution_request_identity execution_request,
      pkgexec::backend_capability_profile_identity backend,
      pkgexec::execution_evidence_identity execution,
      pkgcheck::check_result_identity check,
      pkgcheck_exec::check_execution_result_encoding encoding)
  {
    return check_dispatch_evidence_record(
        std::move(identity), std::move(journal), std::move(transaction),
        std::move(dispatch), std::move(node), std::move(attempt_session),
        std::move(result), std::move(controller_request),
        std::move(construction), std::move(check_request),
        std::move(execution_request), std::move(backend),
        std::move(execution), std::move(check), std::move(encoding));
  }
};

transaction_run_evidence_encoding encode_construction_dispatch_evidence(
    const construction_dispatch_evidence_record& record)
{
  const auto expected = construction_record_identity(
      record.journal(), record.transaction(), record.dispatch(), record.node(),
      record.attempt_session(), record.result(), record.controller_request(),
      record.session_encoding(), record.materialization(),
      record.materialization_encoding(),
      record.build_request(), record.execution_request(), record.backend(),
      record.execution(), record.build(), record.encoding());
  if (record.schema_version() != transaction_run_evidence_schema_version ||
      record.identity() != expected)
  {
    throw transaction_run_evidence_error(
        transaction_run_evidence_error_code::invalid_record,
        "construction evidence record identity is not canonical");
  }

  writer output;
  output.raw(construction_magic);
  output.u16(record.schema_version());
  output.identity(record.identity().hex());
  output.identity(record.journal().hex());
  output.identity(record.transaction().hex());
  output.identity(record.dispatch().hex());
  output.identity(record.node().hex());
  output.identity(record.attempt_session().hex());
  output.identity(record.result().hex());
  output.identity(record.controller_request().hex());
  output.bytes(record.session_encoding());
  output.identity(record.materialization().hex());
  output.bytes(record.materialization_encoding());
  output.identity(record.build_request().hex());
  output.identity(record.execution_request().hex());
  output.identity(record.backend().hex());
  output.identity(record.execution().hex());
  output.identity(record.build().hex());
  output.bytes(record.encoding());
  return output.finish();
}

construction_dispatch_evidence_record decode_construction_dispatch_evidence(
    const transaction_run_evidence_encoding& encoding)
{
  validate_checksum(encoding);
  const auto payload_size = encoding.size() - checksum_size;
  reader input(encoding, payload_size);
  read_magic(input, construction_magic);
  auto retained_identity = session_identity::from_hex(input.identity());
  auto journal = session_identity::from_hex(input.identity());
  auto transaction = session_identity::from_hex(input.identity());
  auto dispatch = session_identity::from_hex(input.identity());
  auto node = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  auto attempt = session_identity::from_hex(input.identity());
  auto result = session_identity::from_hex(input.identity());
  auto request = session_identity::from_hex(input.identity());
  auto session_encoding = input.bytes(maximum_construction_session_encoding_size);
  auto materialization = pkgfetch::materialization_identity::from_sha256(
      input.identity());
  auto materialization_encoding = input.bytes(
      pkgfetch::maximum_source_materialization_encoding_size);
  auto build_request = pkgbuild::build_request_identity::from_sha256(
      input.identity());
  auto execution_request = pkgexec::execution_request_identity::from_sha256(
      input.identity());
  auto backend = pkgexec::backend_capability_profile_identity::from_sha256(
      input.identity());
  auto execution = pkgexec::execution_evidence_identity::from_sha256(
      input.identity());
  auto build = pkgbuild::build_result_identity::from_sha256(input.identity());
  auto nested = input.bytes(
      pkgbuild_exec::maximum_build_execution_result_encoding_size);
  input.finish();

  auto expected = construction_record_identity(
      journal, transaction, dispatch, node, attempt, result, request,
      session_encoding, materialization, materialization_encoding, build_request,
      execution_request, backend, execution, build, nested);
  if (retained_identity != expected)
    corrupt("construction evidence record identity mismatch");

  auto record = detail_run_evidence_codec_access::construction(
      std::move(retained_identity), std::move(journal),
      std::move(transaction), std::move(dispatch), std::move(node),
      std::move(attempt), std::move(result), std::move(request),
      std::move(session_encoding), std::move(materialization),
      std::move(materialization_encoding),
      std::move(build_request), std::move(execution_request),
      std::move(backend), std::move(execution), std::move(build),
      std::move(nested));
  if (encode_construction_dispatch_evidence(record) != encoding)
    corrupt("construction evidence encoding is not canonical");
  return record;
}

transaction_run_evidence_encoding encode_check_dispatch_evidence(
    const check_dispatch_evidence_record& record)
{
  const auto expected = check_record_identity(
      record.journal(), record.transaction(), record.dispatch(), record.node(),
      record.attempt_session(), record.result(), record.controller_request(),
      record.construction(), record.check_request(), record.execution_request(),
      record.backend(), record.execution(), record.check(), record.encoding());
  if (record.schema_version() != transaction_run_evidence_schema_version ||
      record.identity() != expected)
  {
    throw transaction_run_evidence_error(
        transaction_run_evidence_error_code::invalid_record,
        "check evidence record identity is not canonical");
  }

  writer output;
  output.raw(check_magic);
  output.u16(record.schema_version());
  output.identity(record.identity().hex());
  output.identity(record.journal().hex());
  output.identity(record.transaction().hex());
  output.identity(record.dispatch().hex());
  output.identity(record.node().hex());
  output.identity(record.attempt_session().hex());
  output.identity(record.result().hex());
  output.identity(record.controller_request().hex());
  output.identity(record.construction().hex());
  output.identity(record.check_request().hex());
  output.identity(record.execution_request().hex());
  output.identity(record.backend().hex());
  output.identity(record.execution().hex());
  output.identity(record.check().hex());
  output.bytes(record.encoding());
  return output.finish();
}

check_dispatch_evidence_record decode_check_dispatch_evidence(
    const transaction_run_evidence_encoding& encoding)
{
  validate_checksum(encoding);
  const auto payload_size = encoding.size() - checksum_size;
  reader input(encoding, payload_size);
  read_magic(input, check_magic);
  auto retained_identity = session_identity::from_hex(input.identity());
  auto journal = session_identity::from_hex(input.identity());
  auto transaction = session_identity::from_hex(input.identity());
  auto dispatch = session_identity::from_hex(input.identity());
  auto node = pkgtransaction::transaction_node_identity::from_sha256(
      input.identity());
  auto attempt = session_identity::from_hex(input.identity());
  auto result = session_identity::from_hex(input.identity());
  auto request = session_identity::from_hex(input.identity());
  auto construction = session_identity::from_hex(input.identity());
  auto check_request = pkgcheck::check_request_identity::from_sha256(
      input.identity());
  auto execution_request = pkgexec::execution_request_identity::from_sha256(
      input.identity());
  auto backend = pkgexec::backend_capability_profile_identity::from_sha256(
      input.identity());
  auto execution = pkgexec::execution_evidence_identity::from_sha256(
      input.identity());
  auto check = pkgcheck::check_result_identity::from_sha256(input.identity());
  auto nested = input.bytes(
      pkgcheck_exec::maximum_check_execution_result_encoding_size);
  input.finish();

  auto expected = check_record_identity(
      journal, transaction, dispatch, node, attempt, result, request,
      construction, check_request, execution_request, backend, execution,
      check, nested);
  if (retained_identity != expected)
    corrupt("check evidence record identity mismatch");

  auto record = detail_run_evidence_codec_access::check(
      std::move(retained_identity), std::move(journal),
      std::move(transaction), std::move(dispatch), std::move(node),
      std::move(attempt), std::move(result), std::move(request),
      std::move(construction), std::move(check_request),
      std::move(execution_request), std::move(backend),
      std::move(execution), std::move(check), std::move(nested));
  if (encode_check_dispatch_evidence(record) != encoding)
    corrupt("check evidence encoding is not canonical");
  return record;
}

} // namespace pkgctl
