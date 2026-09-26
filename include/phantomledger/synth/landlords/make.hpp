#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/counterparties/landlords.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/counterparties/size_law.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/landlords/scale.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace PhantomLedger::synth::landlords {

namespace enumTax = ::PhantomLedger::taxonomies::enums;
namespace landlord = ::PhantomLedger::entity::landlord;

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

/// Landlord roster sizing (counterparty-sizes-2026-09). The roster is the
/// RHFS 2021 property-size law (plus the NMHC Top-50 owners) thinned to this
/// population; the renter share is the payer share that thinning divides,
/// and a test ties it to rent::Rules{}.paidFraction. Each landlord's type
/// comes from its class's RHFS unit mix, not from a population-wide mix.
struct GenerationPlan {
  double renterShare = 0.35;

  InBankProbability inBankP;
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;

    r.check([&] { v::unit("renterShare", renterShare); });
    inBankP.validate(r);
  }
};

using identifiers::Bank;
using identifiers::Role;

namespace detail {

/*
  Entropy compatibility for the retired landlord roster. It drew
  sampleIndex(mix cdf, nextDouble) and then coin(inBankP) per landlord on the
  SHARED entity stream, with max(3, round(12 * pop / 1e4)) landlords. Every
  legacy inBankP sat strictly inside (0, 1), so each coin drew, and the loop
  spent exactly two u64 per landlord. The constants are frozen here, not
  config.
*/
inline constexpr double kLegacyLandlordsPerTenK = 12.0;
inline constexpr int kLegacyLandlordFloor = 3;
inline constexpr int kLegacyDrawsPerLandlord = 2;

inline void burnLegacyDraws(random::Rng &rng, int population) {
  const int legacy =
      scale(kLegacyLandlordsPerTenK, population, kLegacyLandlordFloor);
  for (int i = 0; i < legacy * kLegacyDrawsPerLandlord; ++i) {
    (void)rng.nextU64();
  }
}

// The internal landlord layouts (LI/LS/LC) render seven digits.
inline constexpr std::size_t kMaxRoster = 10'000'000;

} // namespace detail

/// `rng` is the shared entity stream: it only pays the legacy burn. The
/// roster is built from the size law, and each record's type and bank ride
/// its own {"landlord_type", serial} lane off `seed`: one uniform picks the
/// type from the class's unit mix, and the in-bank coin, whose draw count
/// depends on the type, is LAST on the lane.
[[nodiscard]] inline Pack makePack(random::Rng &rng, int population,
                                   std::uint64_t seed,
                                   const GenerationPlan &plan = {}) {
  namespace sizes = ::PhantomLedger::synth::counterparties::sizes;

  detail::burnLegacyDraws(rng, population);

  const auto law = sizes::landlordLaw(population, plan.renterShare);
  const auto total = law.total();
  if (total >= detail::kMaxRoster) {
    throw std::length_error(
        "landlord roster exceeds the seven-digit internal layout");
  }

  const auto typeShares = sizes::landlordTypeShares();
  const random::RngFactory factory{seed};

  Pack out;
  out.roster.records.reserve(total);

  std::uint64_t serial = 0;
  for (std::size_t c = 0; c < law.classes.size(); ++c) {
    const auto cdf = probability::distributions::buildCdf(typeShares[c]);

    for (std::uint32_t m = 0; m < law.classes[c].members; ++m) {
      ++serial;
      auto lane = factory.rng({"landlord_type", std::to_string(serial)});

      const auto typeIx =
          probability::distributions::sampleIndex(cdf, lane.nextDouble());
      const auto type = ::PhantomLedger::landlords::kTypes[typeIx];
      const bool isInternal = lane.coin(plan.inBankP.forType(type));
      const auto bank = isInternal ? Bank::internal : Bank::external;

      const auto id = entity::makeKey(Role::landlord, bank, serial);
      const auto recIx = static_cast<std::uint32_t>(out.roster.records.size());

      out.roster.records.push_back(landlord::Record{
          .accountId = id,
          .type = type,
      });

      out.index.byClass[enumTax::toIndex(type)].push_back(recIx);

      if (isInternal) {
        out.internals.push_back(id);
      } else {
        out.externals.push_back(id);
      }
    }
  }

  out.roster.pool = law.pool();
  return out;
}

} // namespace PhantomLedger::synth::landlords
