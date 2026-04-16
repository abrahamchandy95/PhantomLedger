#pragma once

#include <cstdint>

namespace PhantomLedger::entities {

enum class Type : std::uint16_t {
  customer,
  account,
  merchant,
  employer,
  landlord,
  client,
  platform,
  processor,
  business,
  brokerage,
};

enum class BankAccount : std::uint8_t {
  internal,
  external,
};

} // namespace PhantomLedger::entities
