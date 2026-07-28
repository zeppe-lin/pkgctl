// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/effect_journal_codec.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <openssl/evp.h>

namespace pkgctl {
namespace {

constexpr std::array<std::uint8_t, 8> magic{
    {'P', 'C', 'T', 'L', 'J', 'N', 'L', '1'}};
constexpr std::size_t checksum_size = 32U;

[[noreturn]] void corrupt(const std::string& message)
{
  throw effect_journal_error(
      effect_journal_error_code::corrupt_encoding, message);
}

class writer final {
public:
  void u8(std::uint8_t value) { bytes_.push_back(value); }
  void u16(std::uint16_t value)
  {
    bytes_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes_.push_back(static_cast<std::uint8_t>(value & 0xffU));
  }
  void u64(std::uint64_t value)
  {
    for (unsigned int shift = 56U;; shift -= 8U)
    {
      bytes_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
      if (shift == 0U)
        break;
    }
  }
  void boolean(bool value) { u8(value ? 1U : 0U); }
  void raw(const std::uint8_t* data, std::size_t size)
  { bytes_.insert(bytes_.end(), data, data + size); }
  void text(const std::string& value)
  {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
      throw effect_journal_error(
          effect_journal_error_code::invalid_record,
          "journal string exceeds encoding bounds");
    u64(value.size());
    raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }
  void optional_text(const std::optional<std::string>& value)
  {
    boolean(value.has_value());
    if (value)
      text(*value);
  }
  [[nodiscard]] effect_attempt_encoding finish() &&
  { return std::move(bytes_); }
private:
  effect_attempt_encoding bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  [[nodiscard]] std::uint8_t u8()
  {
    require(1U);
    return data_[offset_++];
  }
  [[nodiscard]] std::uint16_t u16()
  {
    const auto high = u8();
    const auto low = u8();
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high) << 8U) | low);
  }
  [[nodiscard]] std::uint64_t u64()
  {
    std::uint64_t value = 0U;
    for (unsigned int index = 0; index < 8U; ++index)
      value = (value << 8U) | u8();
    return value;
  }
  [[nodiscard]] bool boolean()
  {
    const auto value = u8();
    if (value > 1U)
      corrupt("journal Boolean is not canonical");
    return value == 1U;
  }
  [[nodiscard]] std::string text()
  {
    const auto length = u64();
    if (length > size_ - offset_)
      corrupt("journal string exceeds remaining encoding");
    const auto* begin = reinterpret_cast<const char*>(data_ + offset_);
    offset_ += static_cast<std::size_t>(length);
    return std::string(begin, begin + length);
  }
  [[nodiscard]] std::optional<std::string> optional_text()
  { return boolean() ? std::optional<std::string>(text()) : std::nullopt; }
  void expect(const std::uint8_t* expected, std::size_t size)
  {
    require(size);
    if (!std::equal(expected, expected + size, data_ + offset_))
      corrupt("journal magic does not match");
    offset_ += size;
  }
  [[nodiscard]] bool empty() const noexcept { return offset_ == size_; }
private:
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      corrupt("journal encoding is truncated");
  }
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t offset_ = 0U;
};

std::array<std::uint8_t, checksum_size> sha256(
    const std::uint8_t* data, std::size_t size)
{
  using context_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (size != 0U && EVP_DigestUpdate(context.get(), data, size) != 1))
    throw effect_journal_error(
        effect_journal_error_code::corrupt_encoding,
        "cannot compute journal checksum");
  std::array<std::uint8_t, checksum_size> result{};
  unsigned int length = 0U;
  if (EVP_DigestFinal_ex(context.get(), result.data(), &length) != 1 ||
      length != result.size())
    throw effect_journal_error(
        effect_journal_error_code::corrupt_encoding,
        "cannot finalize journal checksum");
  return result;
}

std::size_t count(reader& input)
{
  const auto value = input.u64();
  if (value > 65536U)
    corrupt("journal collection exceeds bounded size");
  return static_cast<std::size_t>(value);
}

effect_attempt_stage stage(std::uint8_t value)
{
  if (value < static_cast<std::uint8_t>(effect_attempt_stage::admitted) ||
      value > static_cast<std::uint8_t>(effect_attempt_stage::terminal))
    corrupt("journal stage is invalid");
  return static_cast<effect_attempt_stage>(value);
}

pkgapply::application_attempt_outcome application_outcome(std::uint8_t value)
{
  if (value < static_cast<std::uint8_t>(
                  pkgapply::application_attempt_outcome::precondition_refused) ||
      value > static_cast<std::uint8_t>(
                  pkgapply::application_attempt_outcome::indeterminate))
    corrupt("application outcome is invalid");
  return static_cast<pkgapply::application_attempt_outcome>(value);
}

pkgstate::state_publication_outcome publication_outcome(std::uint8_t value)
{
  if (value < static_cast<std::uint8_t>(
                  pkgstate::state_publication_outcome::published) ||
      value > static_cast<std::uint8_t>(
                  pkgstate::state_publication_outcome::indeterminate))
    corrupt("publication outcome is invalid");
  return static_cast<pkgstate::state_publication_outcome>(value);
}

effectful_operation_outcome terminal_outcome(std::uint8_t value)
{
  if (value > static_cast<std::uint8_t>(
                  effectful_operation_outcome::completed))
    corrupt("controller terminal outcome is invalid");
  return static_cast<effectful_operation_outcome>(value);
}

} // namespace

effect_attempt_encoding encode_effect_attempt_record(
    const effect_attempt_record& record)
{
  writer output;
  output.raw(magic.data(), magic.size());
  output.u16(effect_attempt_encoding_version);
  output.u16(record.schema_version());
  output.text(record.identity().hex());
  output.text(record.attempt().hex());
  output.text(record.session().hex());
  output.raw(record.nonce().bytes().data(), record.nonce().bytes().size());
  output.u64(record.sequence());
  output.boolean(record.previous().has_value());
  if (record.previous())
    output.text(record.previous()->hex());
  output.u64(record.before_total());
  output.u64(record.after_total());
  output.u8(static_cast<std::uint8_t>(record.stage()));
  output.boolean(record.active_index().has_value());
  if (record.active_index())
    output.u64(*record.active_index());
  output.u64(record.before().size());
  for (const auto& fact : record.before())
  {
    output.text(fact.result().hex());
    output.boolean(fact.succeeded());
  }
  output.boolean(record.application().has_value());
  if (record.application())
  {
    output.text(record.application()->receipt());
    output.u8(static_cast<std::uint8_t>(record.application()->outcome()));
    output.optional_text(record.application()->journal());
    output.optional_text(record.application()->completed_evidence());
  }
  output.u64(record.after().size());
  for (const auto& fact : record.after())
  {
    output.text(fact.result().hex());
    output.boolean(fact.succeeded());
  }
  output.optional_text(record.transaction_evidence());
  output.optional_text(record.publication_request());
  output.boolean(record.publication().has_value());
  if (record.publication())
  {
    output.text(record.publication()->receipt());
    output.u8(static_cast<std::uint8_t>(record.publication()->outcome()));
    output.optional_text(record.publication()->resulting_snapshot());
  }
  output.boolean(record.terminal_outcome().has_value());
  if (record.terminal_outcome())
    output.u8(static_cast<std::uint8_t>(*record.terminal_outcome()));
  output.optional_text(record.reconciled_state());

  auto encoding = std::move(output).finish();
  const auto checksum = sha256(encoding.data(), encoding.size());
  encoding.insert(encoding.end(), checksum.begin(), checksum.end());
  if (encoding.size() > maximum_effect_attempt_encoding_size)
    throw effect_journal_error(
        effect_journal_error_code::invalid_record,
        "journal encoding exceeds the maximum size");
  return encoding;
}

effect_attempt_record decode_effect_attempt_record(
    const effect_attempt_encoding& encoding)
{
  if (encoding.size() < magic.size() + 4U + checksum_size ||
      encoding.size() > maximum_effect_attempt_encoding_size)
    corrupt("journal encoding size is invalid");
  const std::size_t payload_size = encoding.size() - checksum_size;
  const auto checksum = sha256(encoding.data(), payload_size);
  if (!std::equal(checksum.begin(), checksum.end(),
                  encoding.begin() + static_cast<std::ptrdiff_t>(payload_size)))
    corrupt("journal checksum does not match");

  reader input(encoding.data(), payload_size);
  input.expect(magic.data(), magic.size());
  if (input.u16() != effect_attempt_encoding_version)
    throw effect_journal_error(
        effect_journal_error_code::unsupported_encoding,
        "journal encoding version is unsupported");
  if (input.u16() != effect_attempt_record_schema_version)
    throw effect_journal_error(
        effect_journal_error_code::unsupported_encoding,
        "journal record schema is unsupported");

  auto identity = session_identity::from_hex(input.text());
  auto attempt = session_identity::from_hex(input.text());
  auto session = session_identity::from_hex(input.text());
  effect_attempt_nonce::byte_array nonce_bytes{};
  for (auto& byte : nonce_bytes)
    byte = input.u8();
  auto nonce = effect_attempt_nonce::from_bytes(nonce_bytes);
  const auto sequence = input.u64();
  std::optional<session_identity> previous;
  if (input.boolean())
    previous = session_identity::from_hex(input.text());
  const auto before_total = count(input);
  const auto after_total = count(input);
  const auto current_stage = stage(input.u8());
  std::optional<std::size_t> active_index;
  if (input.boolean())
    active_index = count(input);

  std::vector<effect_lifecycle_fact> before;
  const auto before_count = count(input);
  before.reserve(before_count);
  for (std::size_t index = 0; index < before_count; ++index)
  {
    auto result = session_identity::from_hex(input.text());
    const bool succeeded = input.boolean();
    before.emplace_back(std::move(result), succeeded);
  }

  std::optional<effect_application_fact> application;
  if (input.boolean())
  {
    auto receipt = input.text();
    const auto outcome = application_outcome(input.u8());
    auto journal = input.optional_text();
    auto completed = input.optional_text();
    application.emplace(std::move(receipt), outcome, std::move(journal),
                        std::move(completed));
  }

  std::vector<effect_lifecycle_fact> after;
  const auto after_count = count(input);
  after.reserve(after_count);
  for (std::size_t index = 0; index < after_count; ++index)
  {
    auto result = session_identity::from_hex(input.text());
    const bool succeeded = input.boolean();
    after.emplace_back(std::move(result), succeeded);
  }

  auto transaction = input.optional_text();
  auto publication_request = input.optional_text();
  std::optional<effect_publication_fact> publication;
  if (input.boolean())
  {
    auto receipt = input.text();
    const auto outcome = publication_outcome(input.u8());
    auto resulting = input.optional_text();
    publication.emplace(std::move(receipt), outcome, std::move(resulting));
  }
  std::optional<effectful_operation_outcome> terminal;
  if (input.boolean())
    terminal = terminal_outcome(input.u8());
  auto reconciled = input.optional_text();
  if (!input.empty())
    corrupt("journal encoding has trailing data");

  return effect_attempt_record::restore(
      std::move(identity), std::move(attempt), std::move(session),
      std::move(nonce), sequence, std::move(previous), before_total,
      after_total, current_stage, active_index, std::move(before),
      std::move(application), std::move(after), std::move(transaction),
      std::move(publication_request), std::move(publication), terminal,
      std::move(reconciled));
}

} // namespace pkgctl
