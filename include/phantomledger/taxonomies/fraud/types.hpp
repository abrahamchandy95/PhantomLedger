#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::fraud {

enum class Typology : std::uint8_t {
  classic = 0,
  layering = 1,
  funnel = 2,
  structuring = 3,
  invoice = 4,
  mule = 5,
  cycle = 6,
  scatterGather = 7,
  bipartite = 8,
};

inline constexpr auto kTypologies = std::to_array<Typology>({
    Typology::classic,
    Typology::layering,
    Typology::funnel,
    Typology::structuring,
    Typology::invoice,
    Typology::mule,
    Typology::cycle,
    Typology::scatterGather,
    Typology::bipartite,
});

inline constexpr std::size_t kTypologyCount = kTypologies.size();

enum class FraudType : std::uint8_t {
  none = 0,
  launderRing = 1,
  launderSolo = 2,
  txnFraudSolo = 3,
  txnFraudRing = 4,
};

[[nodiscard]] constexpr std::string_view fraudTypeName(FraudType t) noexcept {
  switch (t) {
  case FraudType::none:
    return {};
  case FraudType::launderRing:
    return "launder_ring";
  case FraudType::launderSolo:
    return "launder_solo";
  case FraudType::txnFraudSolo:
    return "txn_fraud_solo";
  case FraudType::txnFraudRing:
    return "txn_fraud_ring";
  }
  return {};
}

} // namespace PhantomLedger::fraud
