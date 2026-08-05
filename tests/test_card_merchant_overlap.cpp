//
// tests/test_card_merchant_overlap.cpp
//
// card-fraud-realism-v2 step b: THE MERCHANT-OVERLAP INSTRUMENT
// (contract docs/card_fraud_v2_roadmap.md; gate 1 of
// docs/card_fraud_online_gnn.md).
//
// The online-GNN audit found the corpus's blocking defect statistically
// — every fraud row landed on a merchant with no legitimate card row,
// so merchant identity alone classifies the corpus. That was measured
// once, by hand, in SQL against a live PostgreSQL build. This is the
// same measurement as a SERVERLESS gate, so the defect has a number in
// the suite BEFORE the fix lands and a number after it (instrument
// first — the standing lesson).
//
// It reimplements the audit's own SQL:
//
//   per card-rail merchant: fraud_rows, legitimate_rows
//   fraud_only_merchants          = merchants with fraud > 0, legit == 0
//   fraud_rows_at_fraud_only      = fraud rows sitting on those
//
// WHAT THIS PINS TODAY: only that the measurement is WELL-DEFINED —
// the card rail carries both fraud and legitimate rows, every row
// resolves to a merchant endpoint, and the shares add up. The headline
// ratio is PRINTED, not bounded, because the current generator is
// expected to score ~1.0 on it (the defect) and the step b-2 selection
// round is what drives it to ~0. b-2 tightens the assertion; this file
// deliberately does not encode "the defect exists" as a requirement,
// so the fix never has to fight its own gate.
//
// THE AXIS: the endpoint POPULATION, not the endpoint identity. Two
// disjoint destination pools (fraud draws the legit-transfer biller
// hubs, legitimate card purchases draw the merchant catalogue) is a
// structural property of the generator, not a seed artifact, so a
// single small world measures it faithfully.
//
// WORLD SHAPE (join-cohort round): the leg now carries the PRODUCTION
// join cohort — 8 joiners at 300 people over 730 days from 1991, by the
// BEA sizing in synth/personas/join.hpp — instead of the joinerless
// harness world this gate was first written against
// (window_leg_support.hpp, WORLD SHAPE). NOTHING TO RE-CALIBRATE HERE:
// this file bounds no measured value. Its four assertions are
// well-definedness preconditions (both label populations present, every
// row resolved, shares consistent), satisfied by margins of three orders
// of magnitude — 124 fraud and 42,369 legitimate card rows over 254
// merchants on the pre-flip world — and the headline ratio is printed.
// The world shape is now PINNED below so the measurement can never again
// silently describe a population production does not generate.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;

using pltest::LegOptions;
using pltest::LegResult;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 1234567;
constexpr std::int32_t kPopulation = 300;

struct Counts {
  std::size_t fraud = 0;
  std::size_t legit = 0;
};

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// The card rail: the channel both legitimate card purchases and the
// unauthorized card / gift-card fraud rails ride. Overlap is only
// meaningful within ONE rail — comparing a card fraud row against a
// bill-pay legit row would prove nothing about merchant identity.
[[nodiscard]] bool isCardRail(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  pltest::announceLeg("card-rail overlap leg (1991, 730d)");
  LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  opt.window.days = 730;
  opt.population = kPopulation;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  const auto leg = pltest::runLeg(pools, opt);
  pltest::printLeg("card-overlap", leg);

  // WORLD SHAPE, asserted before anything is measured: this gate reports
  // on the population production generates, which has a join cohort. A
  // zero here means the leg rebuilt the pre-H3-3c-ii joinerless world and
  // every number below describes a world that never ships.
  check(leg.joiners > 0,
        "the leg carries the production join cohort (joiners " +
            std::to_string(leg.joiners) + ")");

  // Per-merchant fraud/legit tallies over the card rail. entity::Key
  // has a defaulted operator<=>, so it keys a std::map directly.
  std::map<pl::entity::Key, Counts> byMerchant;
  std::size_t cardFraudRows = 0;
  std::size_t cardLegitRows = 0;

  for (const auto &t : leg.rows) {
    if (!isCardRail(t)) {
      continue;
    }
    auto &cell = byMerchant[t.target];
    if (t.fraud.flag == 1) {
      ++cell.fraud;
      ++cardFraudRows;
    } else {
      ++cell.legit;
      ++cardLegitRows;
    }
  }

  std::size_t fraudMerchants = 0;
  std::size_t fraudOnlyMerchants = 0;
  std::size_t fraudRowsAtFraudOnly = 0;
  for (const auto &[key, cell] : byMerchant) {
    (void)key;
    if (cell.fraud == 0) {
      continue;
    }
    ++fraudMerchants;
    if (cell.legit == 0) {
      ++fraudOnlyMerchants;
      fraudRowsAtFraudOnly += cell.fraud;
    }
  }

  // The measurement must be well-defined before its value means
  // anything: both populations present, and every card row resolved to
  // some endpoint.
  check(cardFraudRows > 0,
        "card rail carries fraud rows (" + std::to_string(cardFraudRows) + ")");
  check(cardLegitRows > 0, "card rail carries legitimate rows (" +
                               std::to_string(cardLegitRows) + ")");
  check(!byMerchant.empty(), "card rail resolves to merchant endpoints (" +
                                 std::to_string(byMerchant.size()) + ")");
  check(fraudRowsAtFraudOnly <= cardFraudRows,
        "fraud rows at fraud-only merchants cannot exceed all fraud rows");

  const double fraudOnlyRowShare =
      cardFraudRows == 0
          ? 0.0
          : static_cast<double>(fraudRowsAtFraudOnly) /
                static_cast<double>(cardFraudRows);
  const double fraudOnlyMerchantShare =
      fraudMerchants == 0
          ? 0.0
          : static_cast<double>(fraudOnlyMerchants) /
                static_cast<double>(fraudMerchants);

  // THE HEADLINE NUMBER. ~1.0 means merchant identity alone separates
  // the labels (the audit's finding); step b-2 drives it toward 0 by
  // routing the card rails through the merchant acceptance population.
  std::printf("  world shape: join cohort %llu of %d people\n",
              static_cast<unsigned long long>(leg.joiners), kPopulation);
  std::printf("  card-rail merchants %zu (fraud-touched %zu, fraud-only "
              "%zu)\n",
              byMerchant.size(), fraudMerchants, fraudOnlyMerchants);
  std::printf("  card rows: fraud %zu, legit %zu\n", cardFraudRows,
              cardLegitRows);
  std::printf("  FRAUD-ONLY MERCHANT SHARE   %.4f\n", fraudOnlyMerchantShare);
  std::printf("  FRAUD ROWS AT FRAUD-ONLY    %.4f  <- b-2 drives this to ~0\n",
              fraudOnlyRowShare);

  // =====================================================================
  // venue-reuse-2026-08: CROSS-VICTIM CASH-OUT VENUE REUSE.
  //
  // The campaign is reconstructed FROM ROWS, using the fact that attacker
  // devices are campaign-owned (attacker-infra-2026-07): two victims whose
  // fraud rows carry the SAME attacker device are co-victims of one campaign.
  // That is exact for the ~75% of cases carrying an attacker device and
  // silently drops the victim-endpoint branch, which is the conservative
  // direction — a dropped co-victim pair can only lower the measured lift.
  //
  // WHY A LIFT AND NOT A RAW SHARE. Co-victims ALREADY share merchants
  // without any mechanism, because an i.i.d. draw from a finite CDF collides:
  // measured 75% (pop 300) / 33% (pop 900) before this round, against an
  // overlap lift over a time-matched control of only 1.98x / 1.79x. A gate
  // that banded the raw share, or that derived a floor of 1.0x analytically,
  // would be VACUOUS — CLAUDE.md rule 6, third instance. The CONTROL is the
  // measurement.
  // =====================================================================
  struct VictimFraud {
    std::set<pl::entity::Key> merchants;
    std::int64_t firstTs = std::numeric_limits<std::int64_t>::max();
    std::int64_t lastTs = std::numeric_limits<std::int64_t>::min();
  };
  // (attacker device owner id) -> victim -> their fraud merchants
  std::map<std::uint64_t, std::map<pl::entity::Key, VictimFraud>> byCampaign;
  std::map<pl::entity::Key, VictimFraud> allVictims;
  std::size_t casesWithDevice = 0;

  for (const auto &t : leg.rows) {
    if (!isCardRail(t) || t.fraud.flag != 1) {
      continue;
    }
    auto &v = allVictims[t.source];
    v.merchants.insert(t.target);
    v.firstTs = std::min(v.firstTs, t.timestamp);
    v.lastTs = std::max(v.lastTs, t.timestamp);

    const auto &dev = t.session.deviceId;
    if (dev.ownerType != pl::devices::OwnerType::ring) {
      continue; // victim-endpoint branch: no campaign attribution
    }
    ++casesWithDevice;
    auto &cell = byCampaign[dev.ownerId][t.source];
    cell.merchants.insert(t.target);
    cell.firstTs = std::min(cell.firstTs, t.timestamp);
    cell.lastTs = std::max(cell.lastTs, t.timestamp);
  }

  const auto overlaps = [](const VictimFraud &a, const VictimFraud &b) {
    for (const auto &m : a.merchants) {
      if (b.merchants.count(m) != 0) {
        return true;
      }
    }
    return false;
  };
  // TIME-MATCHED MEANS "SAW THE SAME LIVE CATALOGUE", NOT "OVERLAPPED".
  // The first version of this required the two victims' fraud windows to
  // intersect, and it discarded 170 of 171 available pairs at this leg — a
  // case is HOURS long inside a 730-day window, so windows essentially never
  // touch and the control collapsed to a single pair. The axis that actually
  // matters is merchant liveness, which moves on a monthly evolver, so the
  // match is a proximity band on the case dates.
  constexpr std::int64_t kMatchSeconds = 90LL * 24LL * 3600LL;
  const auto timeMatched = [](const VictimFraud &a, const VictimFraud &b) {
    const auto lo = std::max(a.firstTs, b.firstTs);
    const auto hi = std::min(a.lastTs, b.lastTs);
    return (lo - hi) <= kMatchSeconds;
  };

  std::size_t coPairs = 0;
  std::size_t coShared = 0;
  for (const auto &[camp, victims] : byCampaign) {
    (void)camp;
    for (auto i = victims.begin(); i != victims.end(); ++i) {
      for (auto j = std::next(i); j != victims.end(); ++j) {
        if (!timeMatched(i->second, j->second)) {
          continue;
        }
        ++coPairs;
        coShared += overlaps(i->second, j->second) ? 1U : 0U;
      }
    }
  }

  // Control: victim pairs NOT attributed to the same campaign.
  std::map<pl::entity::Key, std::uint64_t> campaignOf;
  for (const auto &[camp, victims] : byCampaign) {
    for (const auto &[victim, cell] : victims) {
      (void)cell;
      campaignOf[victim] = camp;
    }
  }
  std::size_t ctlPairs = 0;
  std::size_t ctlShared = 0;
  for (auto i = allVictims.begin(); i != allVictims.end(); ++i) {
    for (auto j = std::next(i); j != allVictims.end(); ++j) {
      const auto ci = campaignOf.find(i->first);
      const auto cj = campaignOf.find(j->first);
      if (ci != campaignOf.end() && cj != campaignOf.end() &&
          ci->second == cj->second) {
        continue; // co-victims belong to the armed arm
      }
      if (!timeMatched(i->second, j->second)) {
        continue;
      }
      ++ctlPairs;
      ctlShared += overlaps(i->second, j->second) ? 1U : 0U;
    }
  }

  double distinctPerVictim = 0.0;
  for (const auto &[victim, cell] : allVictims) {
    (void)victim;
    distinctPerVictim += static_cast<double>(cell.merchants.size());
  }
  if (!allVictims.empty()) {
    distinctPerVictim /= static_cast<double>(allVictims.size());
  }

  const double coShare =
      coPairs == 0 ? 0.0
                   : static_cast<double>(coShared) /
                         static_cast<double>(coPairs);
  const double ctlShare =
      ctlPairs == 0 ? 0.0
                    : static_cast<double>(ctlShared) /
                          static_cast<double>(ctlPairs);
  const double lift = ctlShare > 0.0 ? coShare / ctlShare : 0.0;

  std::printf("  --- venue reuse ---\n");
  std::printf("  fraud rows with attacker device %zu, campaigns %zu, "
              "victims %zu\n",
              casesWithDevice, byCampaign.size(), allVictims.size());
  std::printf("  distinct fraud merchants per victim   %.3f\n",
              distinctPerVictim);
  std::printf("  co-victim pairs %zu, share >=1 merchant  %.4f\n", coPairs,
              coShare);
  std::printf("  control pairs   %zu, share >=1 merchant  %.4f\n", ctlPairs,
              ctlShare);
  std::printf("  OVERLAP LIFT                          %.3fx\n", lift);

  // PRINTED, NOT BANDED, at this leg. Sizing a floor needs the disarm pair
  // measured across several seeds at both legs, and this leg's co-victim pair
  // count is small enough that a band here could pass vacuously. The
  // well-definedness checks below are what this file asserts today.
  check(allVictims.size() > 1, "leg carries more than one card-fraud victim");
  check(distinctPerVictim > 0.0, "victims resolve to fraud merchants");

  // ===================================================================
  // MEASURED, AND THE RESULT IS NEGATIVE. RECORDED HERE SO IT IS NOT
  // RE-DISCOVERED. venue-reuse-2026-08 was banded-in-intent at a floor near
  // 3.0x on the strength of a Monte-Carlo probe that predicted 4.47x armed
  // against 1.68x disarmed. Driving the REAL sampler at pop 900 x 1461d over
  // four seeds, armed against a disarm that switches off only the shared
  // slot:
  //
  //     seed        armed      disarmed
  //     1234567     1.376x     1.116x
  //     99887766    1.526x     1.592x   <- INVERTED
  //     424242      2.172x     1.895x
  //     7777777     1.386x     0.852x
  //     mean        1.615x     1.364x
  //
  // The two distributions OVERLAP and one seed inverts, so no floor separates
  // them: any threshold the armed build clears, some disarmed seed clears too.
  // Raising the shared slot from 1 of 3 rows to 2 of 3 moved the armed mean to
  // 1.812x and left the overlap intact. THE MECHANISM MOVES THE MEAN; THIS
  // STATISTIC CANNOT RESOLVE IT.
  //
  // WHY, and it is a real tension the design did not resolve: the shared venue
  // is restricted to the popularity HEAD precisely so its fraud rows stay
  // diluted by legitimate ones (that is what holds pure-fraud share at
  // 0.0000). But control victims draw from the same weighted pool and land on
  // the head constantly, so concentrating co-victims there makes them LESS
  // distinguishable, not more. The anti-leak measure and the signal pull in
  // opposite directions.
  //
  // Two things follow for whoever picks this up. (1) Pairwise "share >= 1 of
  // ~2.2 merchants" is the wrong statistic — it saturates. Prefer a
  // campaign-level concentration measure against SIZE-MATCHED random victim
  // groupings, which does not throw away group structure. (2) Do not simply
  // widen the shared slot's row share; that was tried above and buys mean, not
  // separation.
  // ===================================================================

  // THIS LEG CANNOT BAND THE LIFT AND THE GATE SAYS SO RATHER THAN PRETENDING.
  // 300 people over 730 days yields ~19 card-fraud victims across ~11
  // campaigns, so co-victim PAIRS land in the low single digits — one pair
  // flipping moves the ratio by tens of percent. Asserting a floor on that is
  // how `test_card_baselines` came to score 0.0000 on merchant-shortcut
  // recall: a pass earned by having no data. The power warning is itself the
  // check.
  std::printf("  POWER: %zu co-victim pairs — %s\n", coPairs,
              coPairs >= 25 ? "bandable"
                            : "UNDER-POWERED, lift is printed only");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_merchant_overlap: measurement well-defined "
              "(fraud-only row share %.4f, venue-reuse lift %.3fx)\n",
              fraudOnlyRowShare, lift);
  return 0;
}
