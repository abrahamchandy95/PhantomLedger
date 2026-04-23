#pragma once

#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <optional>
#include <utility>

namespace PhantomLedger::taxonomies::identifiers {

[[nodiscard]] constexpr BankMode bankMode(Role role) noexcept {
  switch (role) {
  case Role::customer:
  case Role::account:
    return BankMode::internalOnly;

  case Role::merchant:
  case Role::employer:
  case Role::landlord:
  case Role::client:
    return BankMode::either;

  case Role::platform:
  case Role::processor:
  case Role::business:
  case Role::brokerage:
  case Role::family:
    return BankMode::externalOnly;
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

} // namespace PhantomLedger::taxonomies::identifiers
