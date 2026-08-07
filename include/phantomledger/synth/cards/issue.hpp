#pragma once

#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/cards/seeds.hpp"
#include "phantomledger/synth/econ/nominal.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
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

/* Reservation only. The mean of `kHeldCardShares` — a sizing hint for
 * `records.reserve`, never a modelling constant. */
inline constexpr double kMeanHeldCards = 3.17;

/* Number of credit cards HELD, conditional on holding at least one.
 *
 * CITED: Federal Reserve Banks of Atlanta and San Francisco, 2024 Survey and
 * Diary of Consumer Payment Choice, Tables volume, Table 4 "Use of Credit
 * Card Debt", row block "Number of credit cards, conditional on having a
 * credit card". Accessed 2026-08-07. Published rows: 1 = 23.3%, 2 = 23.5%,
 * 3 = 19.4%, 4 = 11.3%, 5 = 7.5%, 6 or more = 15.1%. Median 3; mean 3.17
 * with the open top bucket imputed at 7.
 *
 * THE TOP BUCKET'S INTERNAL SPLIT IS A DECLARED CHOICE, not a published
 * figure. The last three entries below carry 15.1% of mass at a bucket mean
 * of exactly 7.000, which is what reproduces the citation's own 3.17
 * imputation; nothing in the source distinguishes 6 from 8. Do not quote the
 * split as measured.
 *
 * The published rows sum to 1.001 because each is rounded to a tenth of a
 * point. `sampleHeldCount` normalises by the table's own total rather than
 * editing a cited number to make it close.
 *
 * ADOPTION IS NOT TOUCHED HERE. Whether a person holds a card at all stays
 * `persona.card.prob`, whose realized 0.817-0.833 already matches the same
 * survey's 0.823 adoption row. This table governs only how many. */
inline constexpr std::array<double, 8> kHeldCardShares{
    0.233, 0.235, 0.194, 0.113, 0.075, 0.050, 0.051, 0.050,
};

static_assert(kHeldCardShares.size() <= entity::card::kMaxCardsPerPerson,
              "the held-count law must fit the registry's per-person row");

/* ONE uniform, on the caller's lane. */
[[nodiscard]] inline std::uint32_t sampleHeldCount(random::Rng &rng) noexcept {
  double total = 0.0;
  for (const auto share : kHeldCardShares) {
    total += share;
  }

  double target = rng.nextDouble() * total;
  for (std::size_t i = 0; i + 1 < kHeldCardShares.size(); ++i) {
    target -= kHeldCardShares[i];
    if (target < 0.0) {
      return static_cast<std::uint32_t>(i + 1);
    }
  }
  return static_cast<std::uint32_t>(kHeldCardShares.size());
}

/* The per-card term draws, identical for the first card and the extras so
 * the two passes cannot drift apart. Four draws in a fixed order: APR,
 * credit limit, cycle day, autopay. */
[[nodiscard]] inline entity::card::Terms
sampleTerms(random::Rng &rng, const IssuanceRules &rules,
            const entity::behavior::Persona &persona, double stockScale,
            entity::PersonId owner, std::uint32_t issuanceOrdinal) {
  entity::card::Terms terms{};
  terms.key = makeKey(issuanceOrdinal);
  terms.owner = owner;

  terms.apr = std::clamp(probability::distributions::lognormalByMedian(
                             rng, rules.aprMedian, rules.aprSigma),
                         rules.aprMin, rules.aprMax);

  terms.creditLimit = std::max(rules.limitFloor,
                               probability::distributions::lognormalByMedian(
                                   rng, persona.card.limit, rules.limitSigma)) *
                      stockScale;

  // Inclusive [min, max] => uniformInt is half-open, so +1 on top.
  terms.cycleDay = static_cast<std::uint8_t>(
      rng.uniformInt(static_cast<std::int64_t>(rules.cycleDayMin),
                     static_cast<std::int64_t>(rules.cycleDayMax) + 1));

  terms.autopay = sampleAutopay(rng, rules);

  return terms;
}

/* The extras pass's per-person lane. DISTINCT from `issuanceSeed` so the
 * first card's four term draws stay byte-identical: extending the person's
 * existing lane would be safe for that person but this keeps the two passes
 * independently auditable, and it is what lets the extras pass be deleted or
 * re-tuned without re-rolling a single pre-round card. */
inline constexpr std::uint64_t kExtraCardDomain = 0x4558'5452'4143'4453ULL;

[[nodiscard]] inline std::uint64_t extraIssuanceSeed(std::uint64_t base,
                                                     entity::PersonId person) {
  return issuanceSeed(base ^ kExtraCardDomain, person);
}

} // namespace detail

/// Issue the credit-card registry.
///
/// H1 step 2b (class P stock): `startYear` is the WINDOW-START year;
/// credit limits are calibration-year draws scaled ONCE by
/// priceScale(startYear) so exported limits and ledger overdraft room
/// report the same era-correct world value (authority U-6). The 0
/// sentinel default means "calibration-flat" (scale 1.0) for legacy
/// call sites; production passes the real window year. Scaling
/// multiplies AFTER the draw — RNG order untouched.
[[nodiscard]] inline entity::card::Registry
issue(std::uint64_t base, const entity::behavior::Table &personas,
      std::uint32_t personCount,
      const IssuanceRules &rules = kDefaultIssuanceRules, int startYear = 0) {
  primitives::validate::Report report;
  rules.validate(report);
  report.throwIfFailed();

  assert(personas.byPerson.size() == personCount);

  const double stockScale =
      startYear == 0 ? 1.0 : synth::econ::priceScale(startYear);

  entity::card::Registry out;
  out.byPerson = entity::card::makeCardsByPerson(personCount);

  const auto reservation = static_cast<std::size_t>(
      static_cast<double>(personCount) * detail::kReservationCeiling *
      detail::kMeanHeldCards);

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

    const auto terms =
        detail::sampleTerms(rng, rules, persona, stockScale, p, issuedCount);

    /* Row capacity is `kMaxCardsPerPerson` and this pass mints at most one
     * card per person, so the append cannot fail here. The append pass that
     * mints the extras must check the return. */
    (void)out.byPerson.pushBack(static_cast<std::uint32_t>(p - 1), cardIx);
    out.byKey.emplace(terms.key, cardIx);
    out.records.push_back(terms);
  }

  /* THE EXTRAS PASS, AND IT MUST BE A SECOND PASS.
   *
   * `terms.key` is `makeKey(issuedCount)` — a running issuance ordinal. Minting
   * a person's second card inside the loop above would renumber every card
   * issued after them and move every downstream account key with it. Appending
   * after the first pass has finished leaves every pre-round ordinal
   * byte-identical and gives the extras the block above the high-water mark;
   * `appendChurnReplacements` solved the same problem for merchants the same
   * way.
   *
   * DRAW ACCOUNTING: `issue` consumes no shared stream at all — every person
   * gets `Rng::fromSeed`, so this pass adds draws to a NEW isolated per-person
   * lane and moves no other entity's values. The eligibility predicate is
   * untouched: extras are minted only for people the first pass already gave a
   * card to, so the SET of cardholders is byte-identical and adoption still
   * comes from `persona.card.prob` alone.
   *
   * Held count follows the cited Fed law; how many of them are wired into the
   * spending hot loop is a separate and smaller cap, applied in
   * `prepareSpenders`. */
  const auto holdersBefore = issuedCount;

  for (entity::PersonId p = 1; p <= personCount; ++p) {
    const auto row = static_cast<std::uint32_t>(p - 1);
    if (out.byPerson.rowLength(row) == 0) {
      continue;
    }

    auto rng = random::Rng::fromSeed(detail::extraIssuanceSeed(base, p));
    const auto &persona = personas.byPerson[p - 1];

    const auto held = std::min(detail::sampleHeldCount(rng),
                               entity::card::kMaxCardsPerPerson);

    for (std::uint32_t extra = 1; extra < held; ++extra) {
      ++issuedCount;
      const auto cardIx = static_cast<std::uint32_t>(out.records.size());

      const auto terms =
          detail::sampleTerms(rng, rules, persona, stockScale, p, issuedCount);

      if (!out.byPerson.pushBack(row, cardIx)) {
        --issuedCount;
        break;
      }
      out.byKey.emplace(terms.key, cardIx);
      out.records.push_back(terms);
    }
  }

  /* THE CARDHOLDER SET IS UNCHANGED, AND IT IS CHECKABLE. The extras pass
   * appends to rows that were already non-empty and never creates one, so the
   * number of people holding at least one card cannot move — which is what
   * keeps adoption a property of `persona.card.prob` alone and keeps
   * `hasCard`'s partition of the population byte-identical to the pre-round
   * tree. Counting holders directly rather than trusting the loop shape. */
  {
    std::uint32_t holdersAfter = 0;
    for (std::uint32_t row = 0; row < personCount; ++row) {
      holdersAfter += out.byPerson.rowLength(row) > 0 ? 1U : 0U;
    }
    assert(holdersAfter == holdersBefore);
    (void)holdersAfter;
  }
  (void)holdersBefore;

  return out;
}

} // namespace PhantomLedger::synth::cards
