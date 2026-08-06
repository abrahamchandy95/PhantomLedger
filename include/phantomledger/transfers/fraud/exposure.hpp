#pragma once

#include "phantomledger/entities/parties/behaviors.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

/*
  Card-Exposure Weight
  Tilts unauthorized-fraud victim selection (docs/card_fraud_victimization.md
  D1). Real unauthorized card fraud is EXPOSURE-driven: victimization rises
  with cards held, transaction volume, e-commerce activity and income (BJS
  Identity Theft Supplement — existing-credit-card misuse is the largest
  identity-theft category and its rate rises with household income). A uniform
  draw is not merely under-realistic; for a GNN it makes every victim-side
  feature noise BY CONSTRUCTION, leaving nothing legitimate to learn on that
  side of the graph.

  THE WEIGHT IS DERIVED, NOT A PERSONA TABLE. The exposure gradient is
  mechanical — it is card activity — and `entity::behavior::Table` already
  parameterizes that per person: `cash.rateMultiplier` (transaction rate),
  `card.prob` and `card.share` (how much of it rides a card). Their product IS
  expected card exposure, so the persona ordering the research predicts
  (retiree and student LOW, salaried and highNetWorth HIGH) EMERGES from the
  spending model instead of being asserted by a hand-written multiplier table
  that would need anchors of its own.

  IT READS ENTITY-SYNTHESIS STATE, NOT REALIZED ROW COUNTS, and must keep
  doing so. Realized per-person card counts would be truer — they include the
  liquidity feedback this misses — but the monolithic reference engine receives
  the whole candidate span while the windowed production engine receives only a
  scalar count, so each would have to accumulate its own histogram while
  `test_arch_equivalence` / `test_production_windowed` compare the two
  byte-for-byte. Two accumulators is two things to drift. The behaviour table
  is built once at entity synthesis and is identical in both engines by
  construction, which makes this carrier parity-safe the way `homeAreas` is.
  REGISTERED refinement: realized-exposure weighting, if the liquidity feedback
  ever proves to matter.

  Every constant here is a DECLARED CHOICE with a named comparator, not a
  measured value — the research supports the DIRECTION and the functional FORM,
  not these magnitudes.
 */

namespace PhantomLedger::transfers::fraud {

/* Share of the hazard that responds to exposure; the remainder is flat
 * (reachable regardless of card activity). CHOICE. */
inline constexpr double kExposureTiltShare = 0.70;

/* Weight clamp, in units of the population mean. THE TILT IS BOUNDED ON
 * PURPOSE, for two load-bearing reasons: nobody becomes unreachable, because a
 * customer with no card activity can still be socially engineered and that is
 * the point of the exogenous-attacker model; and a tilt strong enough to make
 * persona or age PREDICT the label would recreate the merchant-identity
 * shortcut in a new place (docs/card_fraud_victimization.md D3).
 * tests/test_card_victim_baselines.cpp is the gate, and the clamp is what
 * keeps it honest by construction rather than by luck. CHOICE. */
inline constexpr double kMinExposureWeight = 0.15;
inline constexpr double kMaxExposureWeight = 4.00;

/* Per-person card-exposure weights, PersonId-1 indexed, mean ~1.0. Pure in
 * the behaviour table, so both engines produce the same span. An empty table
 * yields an empty vector, which leaves victim selection exactly uniform —
 * the carrier degrades to a plain draw rather than to something undefined. */
[[nodiscard]] inline std::vector<double>
cardExposureWeights(const entity::behavior::Table &table) {
  std::vector<double> out;
  const auto people = table.byPerson.size();
  if (people == 0) {
    return out;
  }
  out.reserve(people);

  double total = 0.0;
  for (const auto &persona : table.byPerson) {
    /* Expected card activity: rate x P(card) x card share of spend. */
    const double raw = std::max(0.0, persona.cash.rateMultiplier) *
                       std::max(0.0, persona.card.prob) *
                       std::max(0.0, persona.card.share);
    out.push_back(raw);
    total += raw;
  }

  const double mean = total / static_cast<double>(people);
  if (!(mean > 0.0)) {
    /* Degenerate table (no card activity anywhere): stay uniform. */
    std::fill(out.begin(), out.end(), 1.0);
    return out;
  }

  for (auto &w : out) {
    const double normalized = w / mean;
    w = std::clamp((1.0 - kExposureTiltShare) + kExposureTiltShare * normalized,
                   kMinExposureWeight, kMaxExposureWeight);
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud
