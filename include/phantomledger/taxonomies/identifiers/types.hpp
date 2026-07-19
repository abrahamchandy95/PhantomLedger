#pragma once

#include <array>
#include <cstdint>

namespace PhantomLedger::identifiers {

enum class Role : std::uint8_t {
  customer,
  account,
  merchant,
  employer,
  landlord,
  client,
  platform,
  processor,
  family,
  business,
  brokerage,
  card,
};

inline constexpr auto kRoles = std::to_array<Role>({
    Role::customer,
    Role::account,
    Role::merchant,
    Role::employer,
    Role::landlord,
    Role::client,
    Role::platform,
    Role::processor,
    Role::family,
    Role::business,
    Role::brokerage,
    Role::card,
});

inline constexpr std::size_t kRoleCount = kRoles.size();

enum class Bank : std::uint8_t {
  internal,
  external,
};

enum class BankMode : std::uint8_t {
  internalOnly,
  externalOnly,
  either,
};

} // namespace PhantomLedger::identifiers
