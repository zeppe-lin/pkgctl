// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/error.h>
#include <pkgctl/identity.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

#include <openssl/evp.h>

namespace pkgctl {
namespace {

using context_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

void update(EVP_MD_CTX* context, const void* bytes, std::size_t size)
{
  if (EVP_DigestUpdate(context, bytes, size) != 1)
    throw error(error_code::identity_failure,
                "controller session identity update failed");
}

void add_field(EVP_MD_CTX* context, std::string_view value)
{
  std::array<unsigned char, 8> length{};
  std::uint64_t size = value.size();
  for (std::size_t index = 0; index < length.size(); ++index)
  {
    length[length.size() - 1 - index] = static_cast<unsigned char>(size & 0xffU);
    size >>= 8U;
  }
  update(context, length.data(), length.size());
  if (!value.empty())
    update(context, value.data(), value.size());
}

std::string hex(const unsigned char* bytes, std::size_t size)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(size * 2U);
  for (std::size_t index = 0; index < size; ++index)
  {
    result.push_back(digits[(bytes[index] >> 4U) & 0x0fU]);
    result.push_back(digits[bytes[index] & 0x0fU]);
  }
  return result;
}

} // namespace

session_identity session_identity::from_hex(std::string value)
{
  if (value.size() != 64U)
    throw error(error_code::identity_failure,
                "controller identity is not a SHA-256 hex value");
  for (const char digit : value)
  {
    if (!((digit >= '0' && digit <= '9') ||
          (digit >= 'a' && digit <= 'f')))
      throw error(error_code::identity_failure,
                  "controller identity contains invalid hex");
  }
  return session_identity(std::move(value));
}

session_identity::session_identity(std::string hex) : hex_(std::move(hex)) {}
const std::string& session_identity::hex() const noexcept { return hex_; }
bool operator==(const session_identity& lhs,
                const session_identity& rhs) noexcept
{ return lhs.hex_ == rhs.hex_; }
bool operator!=(const session_identity& lhs,
                const session_identity& rhs) noexcept
{ return !(lhs == rhs); }
bool operator<(const session_identity& lhs,
               const session_identity& rhs) noexcept
{ return lhs.hex_ < rhs.hex_; }

session_identity make_session_identity(
    std::string_view domain,
    const std::vector<std::string>& fields)
{
  context_ptr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    throw error(error_code::identity_failure,
                "controller session identity initialization failed");

  add_field(context.get(), domain);
  for (const std::string& field : fields)
    add_field(context.get(), field);

  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 ||
      size != 32U)
  {
    throw error(error_code::identity_failure,
                "controller session identity finalization failed");
  }
  return session_identity(hex(digest.data(), size));
}

} // namespace pkgctl
