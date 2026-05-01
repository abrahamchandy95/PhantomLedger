#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "types.hpp"

#include <array>
#include <optional>
#include <utility>

namespace PhantomLedger::identifiers {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

struct RoleBankMode {
  Role role;
  BankMode mode;
};

inline constexpr auto kBankModeEntries = std::to_array<RoleBankMode>({
    {Role::customer, BankMode::internalOnly},
    {Role::account, BankMode::internalOnly},
    {Role::merchant, BankMode::either},
    {Role::employer, BankMode::either},
    {Role::landlord, BankMode::either},
    {Role::client, BankMode::either},
    {Role::platform, BankMode::externalOnly},
    {Role::processor, BankMode::externalOnly},
    {Role::family, BankMode::externalOnly},
    {Role::business, BankMode::either},
    {Role::brokerage, BankMode::either},
    {Role::card, BankMode::internalOnly},
});

static_assert(enumTax::isIndexable(kRoles));
static_assert(kBankModeEntries.size() == kRoleCount);

[[nodiscard]] consteval bool coversEveryRoleOnce() {
  std::array<std::uint8_t, kRoleCount> seen{};

  for (const auto entry : kBankModeEntries) {
    const auto index = enumTax::toIndex(entry.role);

    if (index >= kRoleCount) {
      return false;
    }

    ++seen[index];
  }

  for (const auto count : seen) {
    if (count != 1) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] consteval std::array<BankMode, kRoleCount> bankModeTable() {
  std::array<BankMode, kRoleCount> out{};

  for (const auto entry : kBankModeEntries) {
    out[enumTax::toIndex(entry.role)] = entry.mode;
  }

  return out;
}

static_assert(coversEveryRoleOnce());

inline constexpr auto kBankModes = bankModeTable();

} // namespace detail

[[nodiscard]] constexpr BankMode bankMode(Role role) noexcept {
  const auto index = enumTax::toIndex(role);

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
