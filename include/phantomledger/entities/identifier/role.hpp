#pragma once

#include <cstdint>

namespace PhantomLedger::entities::identifier {

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
};

} // namespace PhantomLedger::entities::identifier
