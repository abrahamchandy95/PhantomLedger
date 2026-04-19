#pragma once

#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/role.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace PhantomLedger::entities::identifier {

enum class BankMode : std::uint8_t {
  internalOnly,
  externalOnly,
  either,
};

[[nodiscard]] constexpr BankMode bankMode(Role role) noexcept {
  switch (role) {
  case Role::customer:
  case Role::account:
    return BankMode::internalOnly;

  case Role::merchant:
    return BankMode::either;

  case Role::employer:
  case Role::landlord:
  case Role::client:
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

} // namespace PhantomLedger::entities::identifier
