#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/cards/policy.hpp"
#include "phantomledger/entities/synth/cards/seeds.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace PhantomLedger::entities::synth::cards {
namespace detail {

[[nodiscard]] inline entity::card::Autopay
sampleAutopay(random::Rng &rng, const Policy &policy) noexcept {
  const double u = rng.nextDouble();
  if (u < policy.autopayFullP) {
    return entity::card::Autopay::full;
  }
  if (u < policy.autopayFullP + policy.autopayMinP) {
    return entity::card::Autopay::minimum;
  }
  return entity::card::Autopay::manual;
}

[[nodiscard]] inline entity::Key
makeKey(std::uint32_t issuanceOrdinal) noexcept {
  // 1-indexed; matches Python L{i:09d} starting at i=1.
  return entity::makeKey(entity::Role::card, entity::Bank::internal,
                         issuanceOrdinal);
}

inline constexpr double kReservationCeiling = 0.90;

} // namespace detail

[[nodiscard]] inline entity::card::Registry
issue(std::uint64_t base, const entity::behavior::Table &personas,
      std::uint32_t personCount, const Policy &policy = kDefaultPolicy) {
  assert(policy.valid());
  assert(personas.byPerson.size() == personCount);

  entity::card::Registry out;
  out.byPerson.assign(personCount, entity::card::Registry::none);

  const auto reservation = static_cast<std::size_t>(
      static_cast<double>(personCount) * detail::kReservationCeiling);
  out.records.reserve(reservation);
  out.byKey.reserve(reservation);

  std::uint32_t issuedCount = 0;

  for (entity::PersonId p = 1; p <= personCount; ++p) {
    auto rng = random::Rng::fromSeed(issuanceSeed(base, p));
    const auto &persona = personas.byPerson[p - 1];

    if (!rng.coin(persona.card.prob)) {
      continue;
    }

    ++issuedCount;
    const auto cardIx = static_cast<std::uint32_t>(out.records.size());

    entity::card::Terms terms{};
    terms.key = detail::makeKey(issuedCount);
    terms.owner = p;
    terms.apr = std::clamp(probability::distributions::lognormalByMedian(
                               rng, policy.aprMedian, policy.aprSigma),
                           policy.aprMin, policy.aprMax);
    terms.creditLimit = std::max(
        policy.limitFloor, probability::distributions::lognormalByMedian(
                               rng, persona.card.limit, policy.limitSigma));
    // Inclusive [min, max] => uniformInt is half-open, so +1 on top.
    terms.cycleDay = static_cast<std::uint8_t>(
        rng.uniformInt(static_cast<std::int64_t>(policy.cycleDayMin),
                       static_cast<std::int64_t>(policy.cycleDayMax) + 1));
    terms.autopay = detail::sampleAutopay(rng, policy);

    out.byPerson[p - 1] = cardIx;
    out.byKey.emplace(terms.key, cardIx);
    out.records.push_back(terms);
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::cards
