#pragma once

#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/identifier/role.hpp"
#include "phantomledger/entities/merchants/label.hpp"
#include "phantomledger/entities/merchants/record.hpp"
#include "phantomledger/entities/synth/merchants/config.hpp"
#include "phantomledger/entities/synth/merchants/pack.hpp"
#include "phantomledger/entities/synth/merchants/weights.hpp"
#include "phantomledger/math/sampling.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::merchants {
namespace detail {

[[nodiscard]] inline identifier::Key makeId(bool internal,
                                            std::uint64_t serial) {
  return identifier::make(identifier::Role::merchant,
                          internal ? identifier::Bank::internal
                                   : identifier::Bank::external,
                          serial);
}

} // namespace detail

[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   const Config &cfg = {}) {
  const int coreCount = std::max(
      cfg.core.coreFloor,
      static_cast<int>(std::round(
          cfg.core.corePerTenK * (static_cast<double>(population) / 10000.0))));

  const int tailCount = std::max(
      0, static_cast<int>(std::round(
             cfg.tail.perTenK * (static_cast<double>(population) / 10000.0))));

  const int total = coreCount + tailCount;

  std::vector<double> coreRaw(static_cast<std::size_t>(coreCount));
  for (double &value : coreRaw) {
    value = math::lognormal(rng, 0.0, cfg.core.sizeSigma);
  }
  const auto coreWeights = normalize(coreRaw);

  std::vector<double> tailWeights;
  if (tailCount > 0) {
    std::vector<double> tailRaw(static_cast<std::size_t>(tailCount));
    for (double &value : tailRaw) {
      value = math::lognormal(rng, -0.75, cfg.tail.sizeSigma);
    }
    tailWeights = normalize(tailRaw);
  }

  const double coreShare = 1.0 - cfg.tail.share;
  const double tailShare = cfg.tail.share;

  Pack out;
  out.catalog.records.reserve(static_cast<std::size_t>(total));

  constexpr std::size_t categoryCount =
      taxonomies::merchants::kDefaultCategories.size();

  for (int i = 0; i < total; ++i) {
    const auto categoryIndex =
        static_cast<std::uint16_t>(rng.choiceIndex(categoryCount));
    const auto serial = static_cast<std::uint64_t>(i + 1);
    const bool internal = i < coreCount && rng.coin(cfg.banking.internalP);

    const double weight =
        i < coreCount
            ? coreWeights[static_cast<std::size_t>(i)] * coreShare
            : tailWeights[static_cast<std::size_t>(i - coreCount)] * tailShare;

    out.catalog.records.push_back(entities::merchants::Record{
        .label = entities::merchants::Label{serial},
        .counterpartyId = detail::makeId(internal, serial),
        .categoryIndex = categoryIndex,
        .weight = weight,
    });
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::merchants
