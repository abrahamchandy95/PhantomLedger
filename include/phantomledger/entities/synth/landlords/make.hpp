#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/landlords/scale.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::entities::synth::landlords {

using identifiers::Bank;
using identifiers::Role;

using namespace ::PhantomLedger::entity::landlord;
using namespace ::PhantomLedger::taxonomies::enums;

[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   const Config &cfg = {}) {
  const int total = scale(cfg.perTenK, population, cfg.floor);

  std::array<double, kTypeCount> weights{};

  for (std::size_t i = 0; i < cfg.mix.size(); ++i) {
    weights[i] = cfg.mix[i].weight;
  }

  const auto cdf = distributions::buildCdf(weights);

  Pack out;
  out.roster.records.reserve(static_cast<std::size_t>(total));

  // Single serial counter per bank so that Keys are unique.
  std::uint64_t internalSerial = 0;
  std::uint64_t externalSerial = 0;

  for (int i = 0; i < total; ++i) {
    const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
    const auto type = cfg.mix[idx].type;
    const auto typeIdx = toIndex(type);

    const double inBankP = cfg.inBankP.forType(type);
    const bool isInternal = rng.coin(inBankP);
    const auto bank = isInternal ? Bank::internal : Bank::external;

    const std::uint64_t serial =
        isInternal ? ++internalSerial : ++externalSerial;

    const auto id = entity::makeKey(Role::landlord, bank, serial);
    const auto recIx = static_cast<std::uint32_t>(out.roster.records.size());

    out.roster.records.push_back(Record{
        .accountId = id,
        .type = type,
    });

    out.index.byClass[typeIdx].push_back(recIx);

    if (isInternal) {
      out.internals.push_back(id);
    } else {
      out.externals.push_back(id);
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::landlords
