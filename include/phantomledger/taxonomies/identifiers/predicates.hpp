#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "types.hpp"

#include <array>
#include <optional>
#include <utility>

namespace PhantomLedger::identifiers {

using namespace ::PhantomLedger::taxonomies::enums;

namespace detail {

inline constexpr auto kBankModes = std::to_array<BankMode>({
    BankMode::internalOnly, // customer
    BankMode::internalOnly, // account
    BankMode::either,       // merchant
    BankMode::either,       // employer
    BankMode::either,       // landlord
    BankMode::either,       // client
    BankMode::externalOnly, // platform
    BankMode::externalOnly, // processor
    BankMode::externalOnly, // family
    BankMode::externalOnly, // business
    BankMode::externalOnly, // brokerage
    BankMode::internalOnly, // card
});

static_assert(isIndexable(kRoles));
static_assert(kBankModes.size() == kRoleCount);

} // namespace detail

[[nodiscard]] constexpr BankMode bankMode(Role role) noexcept {
  const auto index = toIndex(role);

  if (index < detail::kBankModes.size()) {
    return detail::kBankModes[index];
  }

  std::unreachable();
}

[[nodiscard]] constexpr bool allows(Role role, Bank bank) noexcept {
  switch (bankMode(role)) {
  case BankMode::internalOnly:
    return bank == Bank::internal;

  case BankMode::externalOnly:
    return bank == Bank::external;

  case BankMode::either:
    return true;
  }

  std::unreachable();
}

[[nodiscard]] constexpr bool requiresExplicitBank(Role role) noexcept {
  return bankMode(role) == BankMode::either;
}

[[nodiscard]] constexpr std::optional<Bank> defaultBank(Role role) noexcept {
  switch (bankMode(role)) {
  case BankMode::internalOnly:
    return Bank::internal;

  case BankMode::externalOnly:
    return Bank::external;

  case BankMode::either:
    return std::nullopt;
  }

  std::unreachable();
}

} // namespace PhantomLedger::identifiers
