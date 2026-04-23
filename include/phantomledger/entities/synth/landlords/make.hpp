#pragma once

#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/landlords/record.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/landlords/scale.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::landlords {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;

namespace detail {

[[nodiscard]] constexpr std::size_t
classIndex(entities::landlords::Class kind) noexcept {
  return static_cast<std::size_t>(kind);
}

} // namespace detail

[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   const Config &cfg = {}) {
  const int total = scale(cfg.perTenK, population, cfg.floor);

  std::array<double, 3> weights{};
  for (std::size_t i = 0; i < cfg.mix.size(); ++i) {
    weights[i] = cfg.mix[i].weight;
  }

  const auto cdf = distributions::buildCdf(weights);

  Pack out;
  out.roster.records.reserve(static_cast<std::size_t>(total));

  // Single serial counter per bank so that Keys are unique.
  // Since Key{landlord, bank, N} doesn't encode the landlord Class,
  // separate per-type counters would produce duplicate Keys
  // (e.g. individual #1 and llcSmall #1 would both be {landlord, internal, 1}).
  // The landlord Class is preserved in the Record for typed formatting.
  std::uint64_t internalSerial = 0;
  std::uint64_t externalSerial = 0;

  for (int i = 0; i < total; ++i) {
    const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
    const auto kind = cfg.mix[idx].kind;
    const auto classIdx = detail::classIndex(kind);

    // Determine banking relationship.
    const double inBankP = cfg.inBankP.forClass(kind);
    const bool isInternal = rng.coin(inBankP);
    const auto bank = isInternal ? Bank::internal : Bank::external;

    const std::uint64_t serial =
        isInternal ? ++internalSerial : ++externalSerial;

    const auto id = identifier::make(Role::landlord, bank, serial);

    const auto recIx = static_cast<std::uint32_t>(out.roster.records.size());
    out.roster.records.push_back(entities::landlords::Record{
        .accountId = id,
        .type = kind,
    });
    out.index.byClass[classIdx].push_back(recIx);

    if (isInternal) {
      out.internals.push_back(id);
    } else {
      out.externals.push_back(id);
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::landlords
