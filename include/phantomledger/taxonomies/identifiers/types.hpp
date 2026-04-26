#pragma once

#include <cstdint>

namespace PhantomLedger::identifiers {

enum class Role : std::uint16_t {
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
