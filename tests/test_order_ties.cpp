//
// tests/test_order_ties.cpp
//
// Ordering register (migration step 8; S10 re-pin applied).
//
// The semantic funds-transfer key (timestamp, source, target, amount) is
// not a total order: distinct rows can tie on it while differing in fraud
// flags, channel, device or IP. The soak-scale register found such ties
// in real corpora, which made tie placement merge-history-dependent. The
// S10 ordering re-pin resolved this: the REPLAY ORDER is now the funds
// key totalized by the remaining audit fields (transactions::Comparator,
// legit ledger detail::fundsLess), so tied rows are content-ordered and
// rows that still compare equal are byte-identical.
//
// This gate does two things:
//
//   1. ENFORCES that the windowed driver's posted output is sorted under
//      the TOTAL replay order. If this fails, span stitching or a merge
//      site missed the re-pin.
//
//   2. REGISTERS the funds-key tie census: adjacent pairs equal on the
//      semantic funds key, and among them how many differ in audit
//      fields. Since the re-pin these are deterministically ordered —
//      the census is a record of how much tie-breaking the total order
//      is doing, not a risk.
//

#include "window_leg_support.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

int main() {
  std::printf("=== Replay Ordering Register (S10 total order) ===\n");

  constexpr std::uint64_t seed = 20260719;

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 3;

  pltest::announceLeg("3-month two-phase leg");
  const auto leg = pltest::runLeg(poolSet, options);
  pltest::printLeg("3-month two-phase leg", leg);

  PL_CHECK(leg.fingerprint.rows > 0);
  PL_CHECK(leg.fingerprint.fraudRows > 0);

  const auto &rows = leg.rows;
  const auto &replayLess = pltest::legitLedger::detail::fundsLess;

  // Enforced: posted output is globally sorted under the total replay
  // order across span seams.
  bool sorted = true;
  for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
    if (replayLess(rows[i + 1], rows[i])) {
      sorted = false;
      std::fprintf(stderr,
                   "  ORDER VIOLATION at rows %zu/%zu: ts %lld > %lld or "
                   "equal-ts key inversion\n",
                   i, i + 1, static_cast<long long>(rows[i].timestamp),
                   static_cast<long long>(rows[i + 1].timestamp));
      break;
    }
  }
  PL_CHECK(sorted);
  std::printf("  PASS: posted output is replay-sorted (total order) across "
              "%zu rows\n",
              rows.size());

  // Registered: adjacent rows equal on the SEMANTIC funds key (the
  // explicit fundsKey census — under the total order, "neither compares
  // less" would mean audit-identical, which is a different question).
  std::size_t tiePairs = 0;
  std::size_t auditDistinctPairs = 0;
  std::size_t fraudInvolved = 0;

  for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
    const auto &a = rows[i];
    const auto &b = rows[i + 1];
    if (pltest::pl::transactions::detail::fundsKey(a) ==
        pltest::pl::transactions::detail::fundsKey(b)) {
      ++tiePairs;
      if (pltest::pl::transactions::detail::auditKey(a) !=
          pltest::pl::transactions::detail::auditKey(b)) {
        ++auditDistinctPairs;
        if (a.fraud.flag != 0 || b.fraud.flag != 0) {
          ++fraudInvolved;
        }
      }
    }
  }

  std::printf("  register: %zu adjacent funds-key ties, %zu with distinct "
              "audit fields (content-ordered by the S10 tie-breakers), %zu "
              "involving fraud rows\n",
              tiePairs, auditDistinctPairs, fraudInvolved);

  if (auditDistinctPairs > 0) {
    std::printf("  note: these ties are deterministically ordered by the "
                "audit-key tie-breakers; they are a census, not an "
                "ambiguity.\n");
  } else {
    std::printf("  note: no audit-distinct funds-key ties in this corpus; "
                "the tie-breakers are idle at this scale.\n");
  }

  return 0;
}
