#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/landlords/scale.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::entities::synth::landlords {

namespace enumTax = ::PhantomLedger::taxonomies::enums;
namespace landlord = ::PhantomLedger::entity::landlord;

struct Share {
  landlord::Type type = landlord::Type::individual;
  double weight = 0.0;
};

struct Rate {
  landlord::Type type = landlord::Type::individual;
  double value = 0.0;
};

namespace detail {

[[nodiscard]] constexpr std::array<double, landlord::kTypeCount>
rates(std::array<Rate, landlord::kTypeCount> entries) noexcept {
  std::array<double, landlord::kTypeCount> out{};

  for (const auto &entry : entries) {
    out[enumTax::toIndex(entry.type)] = entry.value;
  }

  return out;
}

} // namespace detail

struct InBankProbability {
  std::array<double, landlord::kTypeCount> byType = detail::rates({{
      {landlord::Type::individual, 0.06},
      {landlord::Type::llcSmall, 0.04},
      {landlord::Type::corporate, 0.01},
  }});

  [[nodiscard]] constexpr double forType(landlord::Type type) const noexcept {
    return byType[enumTax::toIndex(type)];
  }
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    for (double p : byType) {
      r.check([&] {
        ::PhantomLedger::primitives::validate::unit("inBankP.byType", p);
      });
    }
  }
};

struct GenerationPlan {
  double perTenK = 12.0;
  int floor = 3;

  std::array<Share, landlord::kTypeCount> mix{{
      {landlord::Type::individual, 0.38},
      {landlord::Type::llcSmall, 0.15},
      {landlord::Type::corporate, 0.47},
  }};

  InBankProbability inBankP;
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;

    r.check([&] { v::nonNegative("perTenK", perTenK); });
    r.check([&] { v::nonNegative("floor", floor); });

    for (const auto &share : mix) {
      r.check([&] { v::nonNegative("mix.weight", share.weight); });
    }

    inBankP.validate(r);
  }
};

using identifiers::Bank;
using identifiers::Role;

[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   const GenerationPlan &plan = {}) {
  const int total = scale(plan.perTenK, population, plan.floor);

  std::array<double, landlord::kTypeCount> weights{};

  for (std::size_t i = 0; i < plan.mix.size(); ++i) {
    weights[i] = plan.mix[i].weight;
  }

  const auto cdf = probability::distributions::buildCdf(weights);

  Pack out;
  out.roster.records.reserve(static_cast<std::size_t>(total));

  // Single serial counter per bank so that Keys are unique.
  std::uint64_t internalSerial = 0;
  std::uint64_t externalSerial = 0;

  for (int i = 0; i < total; ++i) {
    const auto idx =
        probability::distributions::sampleIndex(cdf, rng.nextDouble());
    const auto type = plan.mix[idx].type;
    const auto typeIdx = enumTax::toIndex(type);

    const double inBankP = plan.inBankP.forType(type);
    const bool isInternal = rng.coin(inBankP);
    const auto bank = isInternal ? Bank::internal : Bank::external;

    const std::uint64_t serial =
        isInternal ? ++internalSerial : ++externalSerial;

    const auto id = entity::makeKey(Role::landlord, bank, serial);
    const auto recIx = static_cast<std::uint32_t>(out.roster.records.size());

    out.roster.records.push_back(landlord::Record{
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
