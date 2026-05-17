#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/cards/seeds.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace PhantomLedger::synth::cards {

struct IssuanceRules {
  // ---- APR distribution ----

  double aprMedian = 0.22;
  double aprSigma = 0.25;
  double aprMin = 0.08;
  double aprMax = 0.36;

  // ---- Credit-limit distribution ----

  double limitSigma = 0.65;
  double limitFloor = 200.0;

  // ---- Cycle day ----

  std::uint8_t cycleDayMin = 1;
  std::uint8_t cycleDayMax = 28;

  // ---- Autopay categorical ----

  double autopayFullP = 0.40;
  double autopayMinP = 0.10;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::gt("aprMedian", aprMedian, 0.0); });
    r.check([&] { v::nonNegative("aprSigma", aprSigma); });
    r.check([&] { v::gt("aprMin", aprMin, 0.0); });
    r.check([&] { v::ge("aprMax", aprMax, aprMin); });

    r.check([&] { v::gt("limitSigma", limitSigma, 0.0); });
    r.check([&] { v::nonNegative("limitFloor", limitFloor); });

    r.check([&] { v::ge("cycleDayMin", static_cast<int>(cycleDayMin), 1); });
    r.check([&] {
      v::ge("cycleDayMax", static_cast<int>(cycleDayMax),
            static_cast<int>(cycleDayMin));
    });
    r.check([&] {
      v::between("cycleDayMax", static_cast<int>(cycleDayMax), 1, 28);
    });

    r.check([&] { v::unit("autopayFullP", autopayFullP); });
    r.check([&] { v::unit("autopayMinP", autopayMinP); });
    r.check([&] {
      v::between("autopayFullP + autopayMinP", autopayFullP + autopayMinP, 0.0,
                 1.0);
    });
  }
};

inline constexpr IssuanceRules kDefaultIssuanceRules{};

namespace detail {

[[nodiscard]] inline entity::card::Autopay
sampleAutopay(random::Rng &rng, const IssuanceRules &rules) noexcept {
  const double u = rng.nextDouble();

  if (u < rules.autopayFullP) {
    return entity::card::Autopay::full;
  }

  if (u < rules.autopayFullP + rules.autopayMinP) {
    return entity::card::Autopay::minimum;
  }

  return entity::card::Autopay::manual;
}

[[nodiscard]] inline entity::Key
makeKey(std::uint32_t issuanceOrdinal) noexcept {
  return entity::makeKey(entity::Role::card, entity::Bank::internal,
                         issuanceOrdinal);
}

inline constexpr double kReservationCeiling = 0.90;

} // namespace detail

[[nodiscard]] inline entity::card::Registry
issue(std::uint64_t base, const entity::behavior::Table &personas,
      std::uint32_t personCount,
      const IssuanceRules &rules = kDefaultIssuanceRules) {
  primitives::validate::Report report;
  rules.validate(report);
  report.throwIfFailed();

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
                               rng, rules.aprMedian, rules.aprSigma),
                           rules.aprMin, rules.aprMax);

    terms.creditLimit = std::max(
        rules.limitFloor, probability::distributions::lognormalByMedian(
                              rng, persona.card.limit, rules.limitSigma));

    // Inclusive [min, max] => uniformInt is half-open, so +1 on top.
    terms.cycleDay = static_cast<std::uint8_t>(
        rng.uniformInt(static_cast<std::int64_t>(rules.cycleDayMin),
                       static_cast<std::int64_t>(rules.cycleDayMax) + 1));

    terms.autopay = detail::sampleAutopay(rng, rules);

    out.byPerson[p - 1] = cardIx;
    out.byKey.emplace(terms.key, cardIx);
    out.records.push_back(terms);
  }

  return out;
}

} // namespace PhantomLedger::synth::cards
