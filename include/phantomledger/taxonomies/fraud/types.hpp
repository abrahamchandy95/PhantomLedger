#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

} // namespace PhantomLedger::fraud
