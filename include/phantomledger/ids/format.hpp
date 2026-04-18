#pragma once
/*
 * format.hpp — deterministic entity ID formatting.
 *
 * Hot-path code should keep entities::Identity and only materialize strings
 * at IO boundaries. Formatting rules live in compact compile-time layouts,
 * and every formatter here builds the final string in a single allocation.
 */

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/random/rng.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::ids {

enum class LandlordKind : std::uint8_t {
  external,
  individual,
  smallLlc,
  corporate,
};

[[nodiscard]] inline std::string format(const entities::Identity &id);

namespace detail {

struct Layout {
  std::string_view prefix;
  std::uint8_t width = 0;
  bool allowZero = false;
};

inline constexpr Layout kInvalid{};

inline constexpr Layout kCustomer{"C", 10};
inline constexpr Layout kAccount{"A", 10};
inline constexpr Layout kMerchant{"M", 8};
inline constexpr Layout kMerchantExternal{"XM", 8};
inline constexpr Layout kEmployerExternal{"XE", 8};
inline constexpr Layout kLandlordExternal{"XL", 8};
inline constexpr Layout kLandlordIndividual{"XLI", 7};
inline constexpr Layout kLandlordSmallLlc{"XLS", 7};
inline constexpr Layout kLandlordCorporate{"XLC", 7};
inline constexpr Layout kClientExternal{"XC", 8};
inline constexpr Layout kPlatformExternal{"XP", 8};
inline constexpr Layout kProcessorExternal{"XS", 8};
inline constexpr Layout kBusinessExternal{"XO", 8};
inline constexpr Layout kBrokerageExternal{"XB", 8};
inline constexpr Layout kFraudDevice{"FD", 4, true};

inline constexpr std::size_t kTypeCount =
    static_cast<std::size_t>(entities::Type::brokerage) + 1;

inline constexpr std::size_t kBankCount =
    static_cast<std::size_t>(entities::BankAccount::external) + 1;

[[nodiscard]] constexpr std::size_t toIndex(entities::Type type) noexcept {
  return static_cast<std::size_t>(type);
}

[[nodiscard]] constexpr std::size_t
toIndex(entities::BankAccount bank) noexcept {
  return static_cast<std::size_t>(bank);
}

inline constexpr std::array<std::array<Layout, kBankCount>, kTypeCount>
    kIdentityLayouts{{
        /* customer  */ {kCustomer, kInvalid},
        /* account   */ {kAccount, kInvalid},
        /* merchant  */ {kMerchant, kMerchantExternal},
        /* employer  */ {kInvalid, kEmployerExternal},
        /* landlord  */ {kInvalid, kLandlordExternal},
        /* client    */ {kInvalid, kClientExternal},
        /* platform  */ {kInvalid, kPlatformExternal},
        /* processor */ {kInvalid, kProcessorExternal},
        /* business  */ {kInvalid, kBusinessExternal},
        /* brokerage */ {kInvalid, kBrokerageExternal},
    }};

[[nodiscard]] constexpr std::size_t digits(std::uint64_t value) noexcept {
  std::size_t count = 1;
  while (value >= 10) {
    value /= 10;
    ++count;
  }
  return count;
}

inline void appendUnsigned(std::string &out, std::uint64_t value) {
  const auto count = digits(value);
  const auto start = out.size();

  out.resize(start + count);
  auto *pos = out.data() + out.size();

  do {
    *--pos = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0);
}

[[nodiscard]] inline std::string render(Layout layout, std::uint64_t number) {
  if (!layout.allowZero && number == 0) {
    throw std::invalid_argument("ID sequence must be >= 1");
  }

  const auto numberDigits = digits(number);
  const auto paddedDigits = numberDigits < layout.width
                                ? static_cast<std::size_t>(layout.width)
                                : numberDigits;

  std::string out;
  out.reserve(layout.prefix.size() + paddedDigits);
  out.append(layout.prefix);
  out.append(paddedDigits, '0');

  auto *pos = out.data() + out.size();
  do {
    *--pos = static_cast<char>('0' + (number % 10));
    number /= 10;
  } while (number != 0);

  return out;
}

[[nodiscard]] inline Layout layout(const entities::Identity &id) {
  const auto spec = kIdentityLayouts[toIndex(id.type)][toIndex(id.bank)];
  if (spec.prefix.empty()) {
    throw std::invalid_argument("invalid identity type/bank combination");
  }
  return spec;
}

[[nodiscard]] constexpr Layout layout(LandlordKind kind) noexcept {
  switch (kind) {
  case LandlordKind::external:
    return kLandlordExternal;
  case LandlordKind::individual:
    return kLandlordIndividual;
  case LandlordKind::smallLlc:
    return kLandlordSmallLlc;
  case LandlordKind::corporate:
    return kLandlordCorporate;
  }

  return kLandlordExternal;
}

} // namespace detail

[[nodiscard]] inline std::string format(const entities::Identity &id) {
  return detail::render(detail::layout(id), id.number);
}

[[nodiscard]] inline std::string customerId(std::uint64_t number) {
  return format(entities::people::customer(number));
}

[[nodiscard]] inline std::string accountId(std::uint64_t number) {
  return format(entities::accounts::account(number));
}

[[nodiscard]] inline std::string merchantId(std::uint64_t number) {
  return format(entities::accounts::merchant(number));
}

[[nodiscard]] inline std::string merchantExternalId(std::uint64_t number) {
  return format(
      entities::accounts::merchant(number, entities::BankAccount::external));
}

[[nodiscard]] inline std::string employerExternalId(std::uint64_t number) {
  return format(entities::accounts::employer(number));
}

[[nodiscard]] inline std::string landlordId(std::uint64_t number,
                                            LandlordKind kind) {
  return detail::render(detail::layout(kind), number);
}

[[nodiscard]] inline std::string landlordExternalId(std::uint64_t number) {
  return landlordId(number, LandlordKind::external);
}

[[nodiscard]] inline std::string landlordIndividualId(std::uint64_t number) {
  return landlordId(number, LandlordKind::individual);
}

[[nodiscard]] inline std::string landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, LandlordKind::smallLlc);
}

[[nodiscard]] inline std::string landlordCorporateId(std::uint64_t number) {
  return landlordId(number, LandlordKind::corporate);
}

[[nodiscard]] inline std::string clientExternalId(std::uint64_t number) {
  return format(entities::accounts::client(number));
}

[[nodiscard]] inline std::string platformExternalId(std::uint64_t number) {
  return format(entities::accounts::platform(number));
}

[[nodiscard]] inline std::string processorExternalId(std::uint64_t number) {
  return format(entities::accounts::processor(number));
}

[[nodiscard]] inline std::string businessExternalId(std::uint64_t number) {
  return format(entities::accounts::business(number));
}

[[nodiscard]] inline std::string brokerageExternalId(std::uint64_t number) {
  return format(entities::accounts::brokerage(number));
}

/// Deterministic device ID: "C0000000123", index 1 -> "D0000000123_1"
[[nodiscard]] inline std::string deviceId(std::string_view customerId,
                                          std::uint32_t index) {
  if (index == 0) {
    throw std::invalid_argument("Device index must be >= 1");
  }
  if (customerId.empty() || customerId.front() != 'C') {
    throw std::invalid_argument("deviceId expects a C-prefixed customer ID");
  }

  std::string out;
  out.reserve(customerId.size() + 1 + detail::digits(index));
  out.push_back('D');
  out.append(customerId.substr(1));
  out.push_back('_');
  detail::appendUnsigned(out, index);
  return out;
}

/// Shared fraud-ring device: "FD0000", "FD0001", ...
[[nodiscard]] inline std::string fraudDeviceId(std::uint32_t ringId) {
  return detail::render(detail::kFraudDevice, ringId);
}

/// Generate a random-looking IPv4 string.
[[nodiscard]] inline std::string randomIp(random::Rng &rng) {
  const auto o1 = static_cast<std::uint32_t>(rng.uniformInt(11, 223));
  const auto o2 = static_cast<std::uint32_t>(rng.uniformInt(0, 256));
  const auto o3 = static_cast<std::uint32_t>(rng.uniformInt(0, 256));
  const auto o4 = static_cast<std::uint32_t>(rng.uniformInt(1, 255));

  std::string out;
  out.reserve(15);

  detail::appendUnsigned(out, o1);
  out.push_back('.');
  detail::appendUnsigned(out, o2);
  out.push_back('.');
  detail::appendUnsigned(out, o3);
  out.push_back('.');
  detail::appendUnsigned(out, o4);

  return out;
}

[[nodiscard]] constexpr bool isExternal(const entities::Identity &id) noexcept {
  return id.bank == entities::BankAccount::external;
}

[[nodiscard]] constexpr bool isExternal(std::string_view accountId) noexcept {
  return !accountId.empty() && accountId.front() == 'X';
}

} // namespace PhantomLedger::ids
