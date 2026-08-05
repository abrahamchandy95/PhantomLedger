#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/merchants/weights.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::merchants {

struct GenerationPlan {
  struct Scale {
    double corePerTenK = 120.0;
    int coreFloor = 250;
    double sizeSigma = 1.2;
  } core;

  struct Tail {
    double perTenK = 400.0;
    double share = 0.18;
    double sizeSigma = 1.8;
  } tail;

  struct Banking {
    double internalP = 0.02;
  } banking;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;

    r.check([&] { v::positive("core.corePerTenK", core.corePerTenK); });
    r.check([&] { v::nonNegative("core.coreFloor", core.coreFloor); });
    r.check([&] { v::positive("core.sizeSigma", core.sizeSigma); });

    r.check([&] { v::nonNegative("tail.perTenK", tail.perTenK); });
    r.check([&] { v::unit("tail.share", tail.share); });
    r.check([&] { v::positive("tail.sizeSigma", tail.sizeSigma); });

    r.check([&] { v::unit("banking.internalP", banking.internalP); });
  }
};

using identifiers::Bank;
using identifiers::Role;

namespace detail {

[[nodiscard]] inline entity::Key makeId(bool internal, std::uint64_t serial) {
  return entity::makeKey(Role::merchant,
                         internal ? Bank::internal : Bank::external, serial);
}

} // namespace detail

// SIZING IS POPULATION-ONLY AND ITS DRAW COUNT IS LOAD-BEARING.
//
// merchant-churn-2026-07 first tried to add churn headroom HERE, scaling
// the record count by the window length. That was wrong for a reason worth
// recording, because it looks harmless: **this function draws one
// `lognormal` per core record off the SHARED ENTITY STREAM**, so changing
// the record count changes the number of draws and shifts every downstream
// entity-stage value — personas, death dates, everything. The measured
// result was 51,079 account-closure violations in `test_membership` plus
// two unrelated behavioural gates moving, none of which had anything to do
// with merchants.
//
// Churn replacements are therefore appended by
// `synth::merchants::appendChurnReplacements` on an ISOLATED per-record
// lane, exactly as `placeGeography` and `assignLifecycle` are. Do not
// reintroduce a window or a headroom term in this signature: the fact that
// the base catalogue's draw count is invariant is what confines this round
// to the lifecycle mechanism instead of desynchronising the world.
[[nodiscard]] inline entity::merchant::Catalog
makeCatalog(random::Rng &rng, int population, const GenerationPlan &plan = {}) {
  const int coreCount =
      std::max(plan.core.coreFloor,
               static_cast<int>(
                   std::round(plan.core.corePerTenK *
                              (static_cast<double>(population) / 10000.0))));

  const int tailCount = std::max(
      0, static_cast<int>(std::round(
             plan.tail.perTenK * (static_cast<double>(population) / 10000.0))));

  const int total = coreCount + tailCount;

  std::vector<double> coreRaw(static_cast<std::size_t>(coreCount));
  for (double &value : coreRaw) {
    value =
        probability::distributions::lognormal(rng, 0.0, plan.core.sizeSigma);
  }
  const auto coreWeights = normalize(coreRaw);

  std::vector<double> tailWeights;
  if (tailCount > 0) {
    std::vector<double> tailRaw(static_cast<std::size_t>(tailCount));
    for (double &value : tailRaw) {
      value = probability::distributions::lognormal(rng, -0.75,
                                                    plan.tail.sizeSigma);
    }
    tailWeights = normalize(tailRaw);
  }

  const double coreShare = 1.0 - plan.tail.share;
  const double tailShare = plan.tail.share;

  entity::merchant::Catalog out;
  out.records.reserve(static_cast<std::size_t>(total));

  for (int i = 0; i < total; ++i) {
    const auto category =
        ::PhantomLedger::merchants::kCategories[rng.choiceIndex(
            ::PhantomLedger::merchants::kCategoryCount)];

    const auto serial = static_cast<std::uint64_t>(i + 1);
    const bool internal = i < coreCount && rng.coin(plan.banking.internalP);

    const double weight =
        i < coreCount
            ? coreWeights[static_cast<std::size_t>(i)] * coreShare
            : tailWeights[static_cast<std::size_t>(i - coreCount)] * tailShare;

    out.records.push_back(entity::merchant::Record{
        .label = entity::merchant::Label{serial},
        .counterpartyId = detail::makeId(internal, serial),
        .category = category,
        .weight = weight,
    });
  }

  return out;
}

} // namespace PhantomLedger::synth::merchants
