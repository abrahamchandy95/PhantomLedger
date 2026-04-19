#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/landlords/class.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::ids {

struct Layout {
  std::string_view prefix;
  std::uint8_t width = 0;
  bool allowZero = false;
};

[[nodiscard]] inline std::string format(const entities::identifier::Key &id);

namespace detail {

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
inline constexpr Layout kFamilyExternal{"XF", 8};
inline constexpr Layout kBusinessInternal{"BOP", 7};
inline constexpr Layout kBusinessExternal{"XO", 8};
inline constexpr Layout kBrokerageInternal{"BRK", 7};
inline constexpr Layout kBrokerageExternal{"XB", 8};
inline constexpr Layout kFraudDevice{"FD", 4, true};

inline constexpr std::size_t kRoleCount =
    static_cast<std::size_t>(entities::identifier::Role::brokerage) + 1;

inline constexpr std::size_t kBankCount =
    static_cast<std::size_t>(entities::identifier::Bank::external) + 1;

[[nodiscard]] constexpr std::size_t
toIndex(entities::identifier::Role role) noexcept {
  return static_cast<std::size_t>(role);
}

[[nodiscard]] constexpr std::size_t
toIndex(entities::identifier::Bank bank) noexcept {
  return static_cast<std::size_t>(bank);
}

inline constexpr std::array<std::array<Layout, kBankCount>, kRoleCount>
    kIdentityLayouts{{
        /* customer   */ {kCustomer, kInvalid},
        /* account    */ {kAccount, kInvalid},
        /* merchant   */ {kMerchant, kMerchantExternal},
        /* employer   */ {kInvalid, kEmployerExternal},
        /* landlord   */ {kInvalid, kLandlordExternal},
        /* client     */ {kInvalid, kClientExternal},
        /* platform   */ {kInvalid, kPlatformExternal},
        /* processor  */ {kInvalid, kProcessorExternal},
        /* family     */ {kInvalid, kFamilyExternal},
        /* business   */ {kBusinessInternal, kBusinessExternal},
        /* brokerage  */ {kBrokerageInternal, kBrokerageExternal},
    }};

[[nodiscard]] inline Layout checkedLayout(const entities::identifier::Key &id) {
  const auto spec = kIdentityLayouts[toIndex(id.role)][toIndex(id.bank)];
  if (spec.prefix.empty()) {
    throw std::invalid_argument("invalid role/bank combination");
  }
  return spec;
}

} // namespace detail

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

[[nodiscard]] constexpr Layout
layout(const entities::identifier::Key &id) noexcept {
  return detail::kIdentityLayouts[detail::toIndex(id.role)]
                                 [detail::toIndex(id.bank)];
}

[[nodiscard]] constexpr Layout
layout(entities::landlords::Class kind) noexcept {
  switch (kind) {
  case entities::landlords::Class::unspecified:
    return detail::kLandlordExternal;
  case entities::landlords::Class::individual:
    return detail::kLandlordIndividual;
  case entities::landlords::Class::llcSmall:
    return detail::kLandlordSmallLlc;
  case entities::landlords::Class::corporate:
    return detail::kLandlordCorporate;
  }
  return detail::kLandlordExternal;
}

[[nodiscard]] constexpr std::size_t renderedSize(Layout layout,
                                                 std::uint64_t number) {
  if (!layout.allowZero && number == 0) {
    throw std::invalid_argument("ID sequence must be >= 1");
  }

  const auto numberDigits = digits(number);
  const auto paddedDigits = numberDigits < layout.width
                                ? static_cast<std::size_t>(layout.width)
                                : numberDigits;

  return layout.prefix.size() + paddedDigits;
}

[[nodiscard]] inline std::size_t
renderedSize(const entities::identifier::Key &id) {
  const auto spec = detail::checkedLayout(id);
  return renderedSize(spec, id.number);
}

[[nodiscard]] constexpr std::size_t
renderedSize(entities::landlords::Class kind, std::uint64_t number) {
  return renderedSize(layout(kind), number);
}

inline std::size_t write(char *out, Layout layout, std::uint64_t number) {
  if (!layout.allowZero && number == 0) {
    throw std::invalid_argument("ID sequence must be >= 1");
  }

  std::memcpy(out, layout.prefix.data(), layout.prefix.size());

  const auto numberDigits = digits(number);
  const auto paddedDigits = numberDigits < layout.width
                                ? static_cast<std::size_t>(layout.width)
                                : numberDigits;

  char *begin = out + layout.prefix.size();
  char *pos = begin + paddedDigits;

  do {
    *--pos = static_cast<char>('0' + (number % 10));
    number /= 10;
  } while (number != 0);

  while (pos != begin) {
    *--pos = '0';
  }

  return layout.prefix.size() + paddedDigits;
}

inline std::size_t write(char *out, const entities::identifier::Key &id) {
  const auto spec = detail::checkedLayout(id);
  return write(out, spec, id.number);
}

inline std::size_t write(char *out, entities::landlords::Class kind,
                         std::uint64_t number) {
  return write(out, layout(kind), number);
}

[[nodiscard]] inline std::string render(Layout layout, std::uint64_t number) {
  std::string out;
  out.resize(renderedSize(layout, number));
  write(out.data(), layout, number);
  return out;
}

[[nodiscard]] inline std::string format(const entities::identifier::Key &id) {
  const auto spec = detail::checkedLayout(id);
  return render(spec, id.number);
}

[[nodiscard]] inline std::string customerId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::customer,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string accountId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::account,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string merchantId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::merchant,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string merchantExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::merchant,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string employerExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::employer,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string landlordId(std::uint64_t number,
                                            entities::landlords::Class kind) {
  return render(layout(kind), number);
}

[[nodiscard]] inline std::string landlordExternalId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::unspecified);
}

[[nodiscard]] inline std::string landlordIndividualId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::individual);
}

[[nodiscard]] inline std::string landlordSmallLlcId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::llcSmall);
}

[[nodiscard]] inline std::string landlordCorporateId(std::uint64_t number) {
  return landlordId(number, entities::landlords::Class::corporate);
}

[[nodiscard]] inline std::string clientExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::client,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string platformExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::platform,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string processorExternalId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::processor,
                                 entities::identifier::Bank::external, number));
}

[[nodiscard]] inline std::string familyExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::family,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string businessExternalId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::business,
                                           entities::identifier::Bank::external,
                                           number));
}

[[nodiscard]] inline std::string businessOperatingId(std::uint64_t number) {
  return format(entities::identifier::make(entities::identifier::Role::business,
                                           entities::identifier::Bank::internal,
                                           number));
}

[[nodiscard]] inline std::string brokerageExternalId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::brokerage,
                                 entities::identifier::Bank::external, number));
}

[[nodiscard]] inline std::string brokerageCustodyId(std::uint64_t number) {
  return format(
      entities::identifier::make(entities::identifier::Role::brokerage,
                                 entities::identifier::Bank::internal, number));
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
  out.reserve(customerId.size() + 1 + digits(index));
  out.push_back('D');
  out.append(customerId.substr(1));
  out.push_back('_');
  appendUnsigned(out, index);
  return out;
}

/// Overload that avoids constructing the customer ID at the call site.
[[nodiscard]] inline std::string
deviceId(const entities::identifier::Key &customer, std::uint32_t index) {
  if (customer.role != entities::identifier::Role::customer ||
      customer.bank != entities::identifier::Bank::internal) {
    throw std::invalid_argument(
        "deviceId expects an internal customer identity");
  }
  if (index == 0) {
    throw std::invalid_argument("Device index must be >= 1");
  }

  std::string out;
  out.resize(renderedSize(customer));
  write(out.data(), customer);
  out[0] = 'D';
  out.push_back('_');
  appendUnsigned(out, index);
  return out;
}

/// Shared fraud-ring device: "FD0000", "FD0001", ...
[[nodiscard]] inline std::string fraudDeviceId(std::uint32_t ringId) {
  return render(detail::kFraudDevice, ringId);
}

[[nodiscard]] constexpr bool
isExternal(const entities::identifier::Key &id) noexcept {
  return id.bank == entities::identifier::Bank::external;
}

[[nodiscard]] constexpr bool isExternal(std::string_view accountId) noexcept {
  return !accountId.empty() && accountId.front() == 'X';
}

} // namespace PhantomLedger::ids
