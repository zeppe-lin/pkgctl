// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/run_journal_codec.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pkgctl {

struct detail_run_journal_codec_access final {
  static transaction_dispatch_policy restore_policy(
      std::size_t construction_capacity,
      std::size_t check_capacity,
      transaction_failure_containment failure_containment,
      const session_identity& expected_identity)
  {
    return transaction_run_journal_record::restore_policy(
        construction_capacity, check_capacity, failure_containment,
        expected_identity);
  }

  static ready_transaction_unit restore_unit(
      const session_identity& transaction,
      transaction_unit_kind kind,
      pkgtransaction::transaction_node_identity primary_node,
      std::vector<pkgtransaction::transaction_node_identity> members,
      const session_identity& expected_identity)
  {
    return transaction_run_journal_record::restore_unit(
        transaction, kind, std::move(primary_node), std::move(members),
        expected_identity);
  }

  static transaction_dispatch_dependency restore_dependency(
      pkgtransaction::transaction_node_identity node,
      session_identity evidence,
      const session_identity& expected_identity)
  {
    return transaction_run_journal_record::restore_dependency(
        std::move(node), std::move(evidence), expected_identity);
  }

  static transaction_dispatch restore_dispatch(
      ready_transaction_unit unit,
      transaction_dispatch_nonce nonce,
      session_identity reserved_from_progress,
      pkgstate::installed_state_snapshot_identity reserved_state,
      std::vector<transaction_dispatch_dependency> dependencies,
      const session_identity& expected_identity)
  {
    return transaction_run_journal_record::restore_dispatch(
        std::move(unit), std::move(nonce),
        std::move(reserved_from_progress), std::move(reserved_state),
        std::move(dependencies), expected_identity);
  }

  static transaction_dispatch_record restore_dispatch_record(
      transaction_dispatch dispatch,
      transaction_dispatch_state state,
      std::optional<session_identity> attempt_session,
      std::optional<session_identity> effect_attempt,
      std::vector<session_identity> observations,
      std::optional<session_identity> terminal_evidence,
      const session_identity& expected_identity)
  {
    return transaction_run_journal_record::restore_dispatch_record(
        std::move(dispatch), state, std::move(attempt_session),
        std::move(effect_attempt), std::move(observations),
        std::move(terminal_evidence),
        expected_identity);
  }
};
namespace {

constexpr std::array<std::uint8_t, 8> encoding_magic{
    'P', 'K', 'G', 'R', 'U', 'N', 'J', '1'};
constexpr std::size_t maximum_state_identity_size = 256U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::corrupt_encoding, message);
}

class writer final {
public:
  void byte(std::uint8_t value)
  {
    output_.push_back(value);
    check_size();
  }

  void u16(std::uint16_t value)
  {
    byte(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    byte(static_cast<std::uint8_t>(value & 0xffU));
  }

  void u32(std::uint32_t value)
  {
    for (int shift = 24; shift >= 0; shift -= 8)
      byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void boolean(bool value)
  {
    byte(value ? 1U : 0U);
  }

  void raw(const std::uint8_t* data, std::size_t size)
  {
    if (size == 0U)
      return;
    if (size > maximum_transaction_run_encoding_size - output_.size())
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::invalid_record,
          "transaction-run encoding exceeds maximum size");
    output_.insert(output_.end(), data, data + size);
  }

  void text(std::string_view value)
  {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::invalid_record,
          "transaction-run text field is too large");
    u32(static_cast<std::uint32_t>(value.size()));
    raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void hex_identity(std::string_view value)
  {
    if (value.size() != 64U)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::invalid_record,
          "transaction-run identity is not SHA-256 hex");
    for (std::size_t index = 0; index < value.size(); index += 2U)
    {
      const auto high = digit(value[index]);
      const auto low = digit(value[index + 1U]);
      byte(static_cast<std::uint8_t>((high << 4U) | low));
    }
  }

  void optional_identity(const std::optional<session_identity>& value)
  {
    boolean(value.has_value());
    if (value)
      hex_identity(value->hex());
  }

  transaction_run_encoding finish()
  {
    return std::move(output_);
  }

private:
  static std::uint8_t digit(char value)
  {
    if (value >= '0' && value <= '9')
      return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f')
      return static_cast<std::uint8_t>(value - 'a' + 10);
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_record,
        "transaction-run identity contains invalid hex");
  }

  void check_size() const
  {
    if (output_.size() > maximum_transaction_run_encoding_size)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::invalid_record,
          "transaction-run encoding exceeds maximum size");
  }

  transaction_run_encoding output_;
};

class reader final {
public:
  explicit reader(const transaction_run_encoding& input) : input_(input) {}

  std::uint8_t byte()
  {
    require(1U);
    return input_[offset_++];
  }

  std::uint16_t u16()
  {
    std::uint16_t value = 0U;
    for (int index = 0; index < 2; ++index)
      value = static_cast<std::uint16_t>((value << 8U) | byte());
    return value;
  }

  std::uint32_t u32()
  {
    std::uint32_t value = 0U;
    for (int index = 0; index < 4; ++index)
      value = (value << 8U) | byte();
    return value;
  }

  std::uint64_t u64()
  {
    std::uint64_t value = 0U;
    for (int index = 0; index < 8; ++index)
      value = (value << 8U) | byte();
    return value;
  }

  bool boolean()
  {
    const auto value = byte();
    if (value > 1U)
      corrupt("transaction-run encoding contains invalid boolean");
    return value == 1U;
  }

  std::string text(std::size_t maximum)
  {
    const auto size = static_cast<std::size_t>(u32());
    if (size > maximum)
      corrupt("transaction-run text field exceeds its limit");
    require(size);
    if (size == 0U)
      return {};
    std::string value(
        reinterpret_cast<const char*>(input_.data() + offset_), size);
    offset_ += size;
    return value;
  }

  std::string hex_identity()
  {
    static constexpr char digits[] = "0123456789abcdef";
    require(32U);
    std::string value(64U, '0');
    for (std::size_t index = 0; index < 32U; ++index)
    {
      const auto byte_value = input_[offset_++];
      value[index * 2U] = digits[(byte_value >> 4U) & 0x0fU];
      value[index * 2U + 1U] = digits[byte_value & 0x0fU];
    }
    return value;
  }

  std::optional<session_identity> optional_identity()
  {
    if (!boolean())
      return std::nullopt;
    return session_identity::from_hex(hex_identity());
  }

  transaction_run_nonce::byte_array nonce()
  {
    transaction_run_nonce::byte_array bytes{};
    require(bytes.size());
    std::copy_n(input_.begin() + static_cast<std::ptrdiff_t>(offset_),
                bytes.size(), bytes.begin());
    offset_ += bytes.size();
    return bytes;
  }

  void expect_end() const
  {
    if (offset_ != input_.size())
      corrupt("transaction-run encoding contains trailing bytes");
  }

private:
  void require(std::size_t count) const
  {
    if (count > input_.size() - offset_)
      corrupt("transaction-run encoding ended early");
  }

  const transaction_run_encoding& input_;
  std::size_t offset_ = 0U;
};

std::uint32_t checked_count(std::size_t value, std::size_t maximum)
{
  if (value > maximum || value > std::numeric_limits<std::uint32_t>::max())
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::invalid_record,
        "transaction-run collection exceeds its limit");
  return static_cast<std::uint32_t>(value);
}

std::uint8_t encode_unit_kind(transaction_unit_kind value)
{
  switch (value)
  {
    case transaction_unit_kind::construction:
      return 0U;
    case transaction_unit_kind::check:
      return 1U;
    case transaction_unit_kind::operation:
      return 2U;
  }
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record,
      "transaction-run unit kind cannot be encoded");
}

std::uint8_t encode_dispatch_state(transaction_dispatch_state value)
{
  switch (value)
  {
    case transaction_dispatch_state::reserved:
      return 0U;
    case transaction_dispatch_state::started:
      return 1U;
    case transaction_dispatch_state::completed:
      return 2U;
    case transaction_dispatch_state::released_unstarted:
      return 3U;
  }
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record,
      "transaction-run dispatch state cannot be encoded");
}

std::uint8_t encode_failure_containment(
    transaction_failure_containment value)
{
  switch (value)
  {
    case transaction_failure_containment::stop_after_terminal_failure:
      return 0U;
  }
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_record,
      "transaction-run containment policy cannot be encoded");
}

transaction_unit_kind decode_unit_kind(std::uint8_t value)
{
  switch (value)
  {
    case 0U:
      return transaction_unit_kind::construction;
    case 1U:
      return transaction_unit_kind::check;
    case 2U:
      return transaction_unit_kind::operation;
  }
  corrupt("transaction-run encoding contains invalid unit kind");
}

transaction_dispatch_state decode_dispatch_state(std::uint8_t value)
{
  switch (value)
  {
    case 0U:
      return transaction_dispatch_state::reserved;
    case 1U:
      return transaction_dispatch_state::started;
    case 2U:
      return transaction_dispatch_state::completed;
    case 3U:
      return transaction_dispatch_state::released_unstarted;
  }
  corrupt("transaction-run encoding contains invalid dispatch state");
}

transaction_failure_containment decode_failure_containment(std::uint8_t value)
{
  if (value != 0U)
    corrupt("transaction-run encoding contains invalid containment policy");
  return transaction_failure_containment::stop_after_terminal_failure;
}

std::size_t decode_size(std::uint64_t value)
{
  if (value > std::numeric_limits<std::size_t>::max())
    corrupt("transaction-run size does not fit this platform");
  return static_cast<std::size_t>(value);
}

void encode_policy(writer& output, const transaction_dispatch_policy& policy)
{
  output.u64(policy.construction_capacity());
  output.u64(policy.check_capacity());
  output.byte(encode_failure_containment(policy.failure_containment()));
  output.hex_identity(policy.identity().hex());
}

transaction_dispatch_policy decode_policy(reader& input)
{
  const auto construction = decode_size(input.u64());
  const auto check = decode_size(input.u64());
  const auto containment = decode_failure_containment(input.byte());
  const auto identity = session_identity::from_hex(input.hex_identity());
  return detail_run_journal_codec_access::restore_policy(
      construction, check, containment, identity);
}

void encode_dispatch_record(
    writer& output,
    const transaction_dispatch_record& record)
{
  output.hex_identity(record.identity().hex());
  output.byte(encode_dispatch_state(record.state()));

  const auto& dispatch = record.dispatch();
  output.hex_identity(dispatch.identity().hex());
  output.byte(encode_unit_kind(dispatch.unit().kind()));
  output.hex_identity(dispatch.unit().primary_node().hex());
  output.u32(checked_count(
      dispatch.unit().members().size(), maximum_transaction_run_member_count));
  for (const auto& member : dispatch.unit().members())
    output.hex_identity(member.hex());
  output.hex_identity(dispatch.unit().identity().hex());
  output.raw(dispatch.nonce().bytes().data(), dispatch.nonce().bytes().size());
  output.hex_identity(dispatch.reserved_from_progress().hex());
  output.text(dispatch.reserved_state().string());

  output.u32(checked_count(
      dispatch.dependencies().size(),
      maximum_transaction_run_dependency_count));
  for (const auto& dependency : dispatch.dependencies())
  {
    output.hex_identity(dependency.node().hex());
    output.hex_identity(dependency.evidence().hex());
    output.hex_identity(dependency.identity().hex());
  }

  output.optional_identity(record.attempt_session());
  output.optional_identity(record.effect_attempt());
  output.u32(checked_count(
      record.observations().size(),
      maximum_transaction_run_observation_count));
  for (const auto& observation : record.observations())
    output.hex_identity(observation.hex());
  output.optional_identity(record.terminal_evidence());
}

transaction_dispatch_record decode_dispatch_record(
    reader& input,
    const session_identity& transaction)
{
  const auto record_identity = session_identity::from_hex(input.hex_identity());
  const auto state = decode_dispatch_state(input.byte());
  const auto dispatch_identity = session_identity::from_hex(input.hex_identity());
  const auto kind = decode_unit_kind(input.byte());
  auto primary = pkgtransaction::transaction_node_identity::from_sha256(
      input.hex_identity());

  const auto member_count = static_cast<std::size_t>(input.u32());
  if (member_count == 0U ||
      member_count > maximum_transaction_run_member_count)
    corrupt("transaction-run dispatch has invalid member count");
  std::vector<pkgtransaction::transaction_node_identity> members;
  members.reserve(member_count);
  for (std::size_t index = 0; index < member_count; ++index)
    members.push_back(pkgtransaction::transaction_node_identity::from_sha256(
        input.hex_identity()));
  const auto unit_identity = session_identity::from_hex(input.hex_identity());
  auto unit = detail_run_journal_codec_access::restore_unit(
      transaction, kind, std::move(primary), std::move(members), unit_identity);

  auto nonce = transaction_dispatch_nonce::from_bytes(input.nonce());
  auto reserved_progress = session_identity::from_hex(input.hex_identity());
  auto reserved_state = pkgstate::installed_state_snapshot_identity::parse(
      input.text(maximum_state_identity_size));

  const auto dependency_count = static_cast<std::size_t>(input.u32());
  if (dependency_count > maximum_transaction_run_dependency_count)
    corrupt("transaction-run dispatch has too many dependencies");
  std::vector<transaction_dispatch_dependency> dependencies;
  dependencies.reserve(dependency_count);
  for (std::size_t index = 0; index < dependency_count; ++index)
  {
    auto node = pkgtransaction::transaction_node_identity::from_sha256(
        input.hex_identity());
    auto evidence = session_identity::from_hex(input.hex_identity());
    auto identity = session_identity::from_hex(input.hex_identity());
    dependencies.push_back(
        detail_run_journal_codec_access::restore_dependency(
            std::move(node), std::move(evidence), identity));
  }

  auto dispatch = detail_run_journal_codec_access::restore_dispatch(
      std::move(unit), std::move(nonce), std::move(reserved_progress),
      std::move(reserved_state), std::move(dependencies), dispatch_identity);

  auto attempt = input.optional_identity();
  auto effect_attempt = input.optional_identity();
  const auto observation_count = static_cast<std::size_t>(input.u32());
  if (observation_count > maximum_transaction_run_observation_count)
    corrupt("transaction-run dispatch has too many observations");
  std::vector<session_identity> observations;
  observations.reserve(observation_count);
  for (std::size_t index = 0; index < observation_count; ++index)
    observations.push_back(session_identity::from_hex(input.hex_identity()));
  auto terminal = input.optional_identity();

  return detail_run_journal_codec_access::restore_dispatch_record(
      std::move(dispatch), state, std::move(attempt),
      std::move(effect_attempt), std::move(observations),
      std::move(terminal), record_identity);
}

} // namespace

transaction_run_encoding encode_transaction_run_record(
    const transaction_run_journal_record& record)
{
  writer output;
  output.raw(encoding_magic.data(), encoding_magic.size());
  output.u16(transaction_run_encoding_version);
  output.u16(record.schema_version());
  output.hex_identity(record.identity().hex());
  output.hex_identity(record.journal().hex());
  output.hex_identity(record.transaction().hex());
  output.raw(record.nonce().bytes().data(), record.nonce().bytes().size());
  output.u64(record.sequence());
  output.optional_identity(record.previous());
  output.hex_identity(record.run().hex());
  output.hex_identity(record.progress().hex());
  output.text(record.current_state().string());
  encode_policy(output, record.policy());
  output.boolean(record.complete());
  output.boolean(record.failed());
  output.boolean(record.stopped());
  output.u32(checked_count(
      record.dispatches().size(), maximum_transaction_run_dispatch_count));
  for (const auto& dispatch : record.dispatches())
    encode_dispatch_record(output, dispatch);
  return output.finish();
}

transaction_run_journal_record decode_transaction_run_record(
    const transaction_run_encoding& encoding)
{
  if (encoding.size() > maximum_transaction_run_encoding_size)
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::corrupt_encoding,
        "transaction-run encoding exceeds maximum size");

  try
  {
    reader input(encoding);
    for (const auto expected : encoding_magic)
    {
      if (input.byte() != expected)
        corrupt("transaction-run encoding has invalid magic");
    }
    const auto encoding_version = input.u16();
    if (encoding_version != transaction_run_encoding_version)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::unsupported_encoding,
          "unsupported transaction-run encoding version");
    const auto schema_version = input.u16();
    if (schema_version != transaction_run_record_schema_version)
      throw transaction_run_journal_error(
          transaction_run_journal_error_code::unsupported_encoding,
          "unsupported transaction-run record schema");

    auto identity = session_identity::from_hex(input.hex_identity());
    auto journal = session_identity::from_hex(input.hex_identity());
    auto transaction = session_identity::from_hex(input.hex_identity());
    auto nonce = transaction_run_nonce::from_bytes(input.nonce());
    const auto sequence = input.u64();
    auto previous = input.optional_identity();
    auto run = session_identity::from_hex(input.hex_identity());
    auto progress = session_identity::from_hex(input.hex_identity());
    auto current_state = pkgstate::installed_state_snapshot_identity::parse(
        input.text(maximum_state_identity_size));
    auto policy = decode_policy(input);
    const bool complete = input.boolean();
    const bool failed = input.boolean();
    const bool stopped = input.boolean();

    const auto dispatch_count = static_cast<std::size_t>(input.u32());
    if (dispatch_count > maximum_transaction_run_dispatch_count)
      corrupt("transaction-run encoding contains too many dispatches");
    std::vector<transaction_dispatch_record> dispatches;
    dispatches.reserve(dispatch_count);
    for (std::size_t index = 0; index < dispatch_count; ++index)
      dispatches.push_back(decode_dispatch_record(input, transaction));
    input.expect_end();

    return transaction_run_journal_record::restore(
        std::move(identity), std::move(journal), std::move(transaction),
        std::move(nonce), sequence, std::move(previous), std::move(run),
        std::move(progress), std::move(current_state), std::move(policy),
        std::move(dispatches), complete, failed, stopped);
  }
  catch (const transaction_run_journal_error& problem)
  {
    if (problem.code() ==
            transaction_run_journal_error_code::unsupported_encoding ||
        problem.code() == transaction_run_journal_error_code::corrupt_encoding)
      throw;
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::corrupt_encoding,
        std::string("invalid transaction-run encoding: ") + problem.what());
  }
  catch (const std::exception& problem)
  {
    throw transaction_run_journal_error(
        transaction_run_journal_error_code::corrupt_encoding,
        std::string("invalid transaction-run encoding: ") + problem.what());
  }
}

} // namespace pkgctl
