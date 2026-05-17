#include "phantomledger/app/setup.hpp"

#include "phantomledger/app/options.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::app::setup {

namespace {

namespace pii = ::PhantomLedger::synth::pii;
namespace pl = ::PhantomLedger;

[[nodiscard]] std::size_t
poolSizeForFraction(double fraction, std::int32_t population) noexcept {
  constexpr std::size_t kPoolFloor = 1'000;
  constexpr std::size_t kPoolCap = 50'000;

  const auto expected = static_cast<std::size_t>(
      std::ceil(static_cast<double>(population) * fraction));
  const std::size_t targetN =
      (expected > kPoolCap / 4) ? kPoolCap : expected * 4;
  return std::max(kPoolFloor, targetN);
}

[[nodiscard]] std::uint32_t derivePoolSeed(std::uint64_t topLevelSeed,
                                           std::size_t countryIdx) noexcept {
  constexpr std::uint64_t kPoolSeedDomain = 0xA5A5A5A5ULL;
  constexpr std::uint64_t kCountryMixer = 0x9E3779B9ULL;
  return static_cast<std::uint32_t>(
      topLevelSeed ^ kPoolSeedDomain ^
      (static_cast<std::uint64_t>(countryIdx) * kCountryMixer));
}

[[nodiscard]] pii::LocalePool generateLocalePool(pl::locale::Country country,
                                                 double fraction,
                                                 std::int32_t population,
                                                 std::uint64_t seed) {
  assert(fraction > 0.0 && "Caller must filter zero-weight countries.");

  const auto idx = pl::taxonomies::enums::toIndex(country);
  const auto poolN = poolSizeForFraction(fraction, population);

  const pii::PoolSizes sizes{
      .firstNames = poolN,
      .middleNames = poolN,
      .lastNames = poolN,
      .streets = poolN,
  };

  return pii::buildLocalePool(country, sizes, derivePoolSeed(seed, idx));
}

} // namespace

pii::PoolSet buildPoolSet(const RunOptions &opts, const pii::LocaleMix &mix) {
  double totalWeight = 0.0;
  for (const auto w : mix.weights) {
    totalWeight += w;
  }

  pii::PoolSet poolSet;
  if (totalWeight <= 0.0) {
    return poolSet;
  }

  for (const auto country : pl::locale::kCountries) {
    const auto idx = pl::taxonomies::enums::toIndex(country);
    const auto weight = mix.weights[idx];
    if (weight <= 0.0) {
      continue;
    }
    poolSet.byCountry[idx] = generateLocalePool(country, weight / totalWeight,
                                                opts.population, opts.seed);
  }

  return poolSet;
}

pl::pipeline::stages::entities::EntitySynthesis
buildEntitySynthesis(const RunOptions &opts, const pii::PoolSet &pools,
                     const pii::LocaleMix &mix, pl::time::TimePoint simStart) {
  return pl::pipeline::stages::entities::EntitySynthesis{
      .population = opts.population,
      .identity =
          pii::IdentityContext{
              .pools = &pools,
              .simStart = simStart,
              .localeMix = mix,
          },
  };
}

} // namespace PhantomLedger::app::setup
