#pragma once

#include "phantomledger/distributions/cdf.hpp"
#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/role.hpp"
#include "phantomledger/entities/landlords/record.hpp"
#include "phantomledger/entities/synth/landlords/config.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/landlords/scale.hpp"
#include "phantomledger/random/rng.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::entities::synth::landlords {
namespace detail {

[[nodiscard]] inline std::size_t classIndex(entities::landlords::Class kind) {
  switch (kind) {
  case entities::landlords::Class::individual:
    return 0;
  case entities::landlords::Class::llcSmall:
    return 1;
  case entities::landlords::Class::corporate:
    return 2;
  case entities::landlords::Class::unspecified:
    break;
  }

  throw std::invalid_argument("unsupported landlord class");
}

} // namespace detail

[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   const Config &cfg = {}) {
  const int total = scale(cfg.perTenK, population, cfg.floor);

  std::vector<double> weights;
  weights.reserve(cfg.mix.size());
  for (const auto &entry : cfg.mix) {
    weights.push_back(entry.weight);
  }

  const auto cdf = distributions::buildCdf(weights);

  Pack out;
  out.roster.records.reserve(static_cast<std::size_t>(total));

  std::uint64_t serial = 0;
  for (int i = 0; i < total; ++i) {
    const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
    const auto kind = cfg.mix[idx].kind;
    const auto id = identifier::make(identifier::Role::landlord,
                                     identifier::Bank::external, ++serial);

    const auto recIx = static_cast<std::uint32_t>(out.roster.records.size());
    out.roster.records.push_back(entities::landlords::Record{
        .accountId = id,
        .type = kind,
    });
    out.index.byClass[detail::classIndex(kind)].push_back(recIx);
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::landlords
