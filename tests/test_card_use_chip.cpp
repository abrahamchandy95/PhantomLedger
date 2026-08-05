//
// tests/test_card_use_chip.cpp
//
// card-fraud-realism-v2 ROUND 8 (use-chip-causal-2026-07): THE CAUSAL
// ENTRY-MODE GATE — the entry-mode half of gate 4 in
// docs/card_fraud_online_gnn.md, contract row in
// docs/card_fraud_feature_contract.md.
//
// Before this round `use_chip` was an FNV content hash (Swipe .63 /
// Chip .26 / Online .11) with no mechanism behind it: a physical
// grocery outlet could render "Online Transaction" and a 1994 row could
// render "Chip Transaction". Now the exporter derives entry mode from
// the destination's ACCEPTANCE ENVIRONMENT — the same Footprint axis
// both legitimate selection (payments.cpp) and the fraud rails
// (unauthorized.cpp pickMerchantDestination) partition on — plus the
// dated US EMV terminal mix for the chip/swipe split on physical rows.
//
// Two legs: 1991/730d (pre-EMV) and 2019/365d. The 2019 leg is ONE
// calendar year on purpose: the staged corpus of a [2019-01-01,
// 2020-01-01) window carries only 2019 timestamps, so the table value
// (.65) is the EXACT per-row expectation. A 730d leg would blend the
// 2019 (.65) and 2020 (.73) table years and park the realized share at
// the band edge — a false red waiting for a seed.
//
// WHAT IS GATED, and why each check is not vacuous:
//
//   COHERENCE PIN   for every card-view row, Online <=> the destination
//                   is a geography-free acceptance endpoint (catalog
//                   `Footprint::online`, or a non-catalog remote
//                   biller). This restates the derivation's contract,
//                   deliberately — like the point-in-time gate it is a
//                   REGRESSION BARRIER: it fails the moment anyone
//                   reintroduces a hash or an acausal mapping, and it
//                   would have FAILED on the pre-round derivation.
//   ERA PIN (1991)  zero "Chip Transaction" rows in a pre-EMV leg —
//                   sharp and sampling-free, the same class of signal
//                   as the class-F clamp ceiling.
//   ERA PIN (2019)  Chip AND Swipe both present, and the realized chip
//                   share of card-present rows inside [0.60, 0.70]
//                   (table value .65; the per-row draw is a content
//                   hash ~ Uniform, so at the >= 1,000-row precondition
//                   binomial 3 sigma is +-0.045). UNDER-POWERED legs
//                   FAIL rather than pass vacuously (ROUND 5's law);
//                   the repair is a larger leg, never a wider band.
//   POPULATED       both modalities and both labels present per leg, so
//                   no pin above can pass against an empty class.
//
// PRINTED, NOT GATED (the overlap gate's "instrument first" law): the
// CNP share of fraud rows vs legitimate rows. The generator encodes
// CNP-majority fraud (kCardNotPresentShare = 0.70, per-CASE) against a
// CNP-minority legitimate mix (~0.11 mode roll plus online favorites),
// but the fraud share realizes over CASES (clustered), the gift-card
// rail is card-present by construction, and the mix is seed-dependent
// at N=300 — so the direction is recorded here and a band can be
// tightened once the first runs put numbers next to it.
//
// WORLD SHAPE: production join cohort, pinned via leg.joiners like
// every v2 gate. Footprints resolve against LegResult::merchants — the
// SAME catalogue records the generator selected from.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/exporter/card_fraud/derive.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <cstdio>
#include <map>
#include <optional>
#include <string>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace derive = pl::exporter::card_fraud::derive;

using Footprint = pl::entity::merchant::Footprint;
using pltest::LegOptions;
using pltest::LegResult;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 24681357;
constexpr std::int32_t kPopulation = 300;

// The dated EMV table's fixed points, pinned at compile time: zero
// through 2011, the liability-shift step, the 2019 majority value, and
// the freeze-outside-coverage clamp (the era-scale convention).
static_assert(derive::chipShareBasisPoints(1991) == 0U);
static_assert(derive::chipShareBasisPoints(2011) == 0U);
static_assert(derive::chipShareBasisPoints(2012) == 100U);
static_assert(derive::chipShareBasisPoints(2015) == 1'000U);
static_assert(derive::chipShareBasisPoints(2019) == 6'500U);
static_assert(derive::chipShareBasisPoints(2024) == 9'000U);
static_assert(derive::chipShareBasisPoints(2030) == 9'000U);

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// The card view's channel filter, exactly as streaming.hpp applies it.
[[nodiscard]] bool inCardView(const Txn &t) {
  return t.session.channel == channels::tag(channels::Legit::cardPurchase) ||
         t.session.channel == channels::tag(channels::Legit::merchant);
}

struct LegStats {
  std::size_t viewRows = 0;
  std::size_t onlineRows = 0;
  std::size_t chipRows = 0;
  std::size_t swipeRows = 0;
  std::size_t valueSetViolations = 0;
  std::size_t coherenceViolations = 0;
  std::size_t nonCatalogRows = 0;
  std::size_t fraudRows = 0;
  std::size_t fraudOnline = 0;
  std::size_t legitOnline = 0;
};

[[nodiscard]] LegStats analyze(const LegResult &leg) {
  // Destination -> footprint, from the leg's OWN acceptance catalogue.
  std::map<pl::entity::Key, Footprint> footprintByKey;
  for (const auto &rec : leg.merchants.records) {
    footprintByKey.emplace(rec.counterpartyId, rec.footprint);
  }

  LegStats s;
  for (const auto &t : leg.rows) {
    if (!inCardView(t)) {
      continue;
    }
    ++s.viewRows;

    std::optional<Footprint> footprint;
    if (const auto it = footprintByKey.find(t.target);
        it != footprintByKey.end()) {
      footprint = it->second;
    } else {
      ++s.nonCatalogRows;
    }

    const auto useChip = derive::useChipFor(t, footprint);
    const bool online = useChip == derive::kUseChipOnline;
    const bool chip = useChip == derive::kUseChipChip;
    const bool swipe = useChip == derive::kUseChipSwipe;
    if (!online && !chip && !swipe) {
      ++s.valueSetViolations;
    }

    // THE COHERENCE PIN: Online <=> geography-free acceptance endpoint.
    const bool geographyFree =
        !footprint.has_value() || *footprint == Footprint::online;
    if (geographyFree != online) {
      ++s.coherenceViolations;
    }

    s.onlineRows += online ? 1U : 0U;
    s.chipRows += chip ? 1U : 0U;
    s.swipeRows += swipe ? 1U : 0U;

    if (t.fraud.flag == 1) {
      ++s.fraudRows;
      s.fraudOnline += online ? 1U : 0U;
    } else {
      s.legitOnline += online ? 1U : 0U;
    }
  }
  return s;
}

void printStats(const char *label, const LegStats &s) {
  const auto legitRows = s.viewRows - s.fraudRows;
  const double fraudCnp =
      s.fraudRows == 0 ? 0.0
                       : static_cast<double>(s.fraudOnline) /
                             static_cast<double>(s.fraudRows);
  const double legitCnp =
      legitRows == 0 ? 0.0
                     : static_cast<double>(s.legitOnline) /
                           static_cast<double>(legitRows);
  std::printf("  [%s] view rows %zu (fraud %zu), online %zu, chip %zu, "
              "swipe %zu, non-catalog %zu\n",
              label, s.viewRows, s.fraudRows, s.onlineRows, s.chipRows,
              s.swipeRows, s.nonCatalogRows);
  std::printf("  [%s] CNP share: fraud %.4f vs legit %.4f  <- printed, "
              "not gated (modeled direction: fraud is CNP-majority)\n",
              label, fraudCnp, legitCnp);
  std::fflush(stdout);
}

// The pins every leg carries regardless of era.
void checkCommon(const char *label, const LegResult &leg, const LegStats &s) {
  const std::string tag{label};
  check(leg.joiners > 0,
        tag + ": production join cohort present (joiners " +
            std::to_string(leg.joiners) + ")");
  check(s.viewRows > 0, tag + ": card view populated");
  check(s.fraudRows > 0, tag + ": card view carries fraud rows");
  check(s.onlineRows > 0, tag + ": CNP population present");
  check(s.chipRows + s.swipeRows > 0, tag + ": card-present population "
                                            "present");
  check(s.valueSetViolations == 0,
        tag + ": every derived value is in the TabFormer value set (" +
            std::to_string(s.valueSetViolations) + " violations)");
  check(s.coherenceViolations == 0,
        tag + ": Online <=> geography-free destination (" +
            std::to_string(s.coherenceViolations) + " violations)");
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  // ------------------------------------------------- 1991: pre-EMV era
  pltest::announceLeg("use_chip leg (1991, 730d)");
  LegOptions early;
  early.seed = kSeed;
  early.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  early.window.days = 730;
  early.population = kPopulation;
  early.withBaseRoutines = true;
  early.withFamily = true;
  const auto earlyLeg = pltest::runLeg(pools, early);
  pltest::printLeg("use-chip-1991", earlyLeg);

  const auto earlyStats = analyze(earlyLeg);
  printStats("1991", earlyStats);
  checkCommon("1991", earlyLeg, earlyStats);

  // THE ERA PIN, sampling-free: no US chip transactions existed in
  // 1991-92, and chipShareBasisPoints is zero before 2012, so ONE chip
  // row here is a wiring defect (year extraction, table lookup, or a
  // reintroduced hash), never seed luck.
  check(earlyStats.chipRows == 0,
        "1991: zero Chip rows in the pre-EMV era (found " +
            std::to_string(earlyStats.chipRows) + ")");

  // ------------------------------------------- 2019: EMV-majority era
  // ONE calendar year (see the file comment): every staged row carries
  // a 2019 timestamp, so the .65 table value is the exact expectation
  // and the band below is a clean binomial statement.
  pltest::announceLeg("use_chip leg (2019, 365d)");
  LegOptions late;
  late.seed = kSeed;
  late.window.start = pl::time::makeTime(pl::time::CalendarDate{2019, 1, 1});
  late.window.days = 365;
  late.population = kPopulation;
  late.withBaseRoutines = true;
  late.withFamily = true;
  const auto lateLeg = pltest::runLeg(pools, late);
  pltest::printLeg("use-chip-2019", lateLeg);

  const auto lateStats = analyze(lateLeg);
  printStats("2019", lateStats);
  checkCommon("2019", lateLeg, lateStats);

  // POWER PRECONDITION (ROUND 5's law): the chip-share band below is
  // sized for >= 1,000 card-present rows (binomial 3 sigma +-0.045
  // around .65 at n = 1,000; tighter at the realized n). A smaller leg
  // FAILS as under-powered instead of passing vacuously; the repair is
  // a larger leg, never a wider band.
  const auto cardPresent = lateStats.chipRows + lateStats.swipeRows;
  check(cardPresent >= 1'000,
        "2019: card-present sample is powered (n = " +
            std::to_string(cardPresent) + ", need >= 1000)");

  check(lateStats.chipRows > 0, "2019: Chip rows exist");
  check(lateStats.swipeRows > 0, "2019: Swipe rows exist");

  if (cardPresent > 0) {
    const double chipShare = static_cast<double>(lateStats.chipRows) /
                             static_cast<double>(cardPresent);
    std::printf("  [2019] chip share of card-present rows %.4f "
                "(table value 0.6500, band [0.60, 0.70])\n",
                chipShare);
    check(chipShare >= 0.60 && chipShare <= 0.70,
          "2019: realized chip share " + std::to_string(chipShare) +
              " inside [0.60, 0.70]");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_use_chip: entry mode is causal (coherence 0 "
              "violations in both eras, chip era-dated)\n");
  return 0;
}
