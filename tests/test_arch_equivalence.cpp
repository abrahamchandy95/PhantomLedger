//
// tests/test_arch_equivalence.cpp
//
// Monolithic vs windowed architecture equivalence (migration step 12),
// COMPLETE MODEL: family transfers included on both sides.
//
// Both paths share one deterministic regime: legit generation and fraud
// planning on the shared sequential stream; products on
// products/full_schedule routed from a pristine router snapshot; family
// on the family lanes routed from its own pristine snapshot; settlement
// on settlement/pre_fraud and settlement/post_fraud. This gate runs the
// SAME world through both architectures:
//
//   monolithic: SimulationPipeline::run()
//               (Simulator spending, one-shot builder with the default
//               family scenario, chunked replay)
//   windowed:   the two-phase WindowedTransferDriver composition
//               (Session spending, base routines via
//               addRoutinesWithoutSpending, family/product/base cursor
//               sources, in-memory spool), full-range generation window
//
// and requires byte-identical posted rows, Golden digest, fraud row count
// and final posted-book hash. Both legs use the monolithic settlement
// strategy (monthly spans, 6-day lookahead) and the machine-resolved
// spending thread count.
//
// HARD-ENFORCED: equivalence was reached (settlement/product/family
// RNG-lane re-pins + addRoutinesWithoutSpending seam + pristine-router
// snapshots). Any divergence now FAILS with full diagnostics: per-channel
// histogram, beyond-window-end counts, cursor emitted/remaining, windowed
// drop maps, tie-permutation verdict and first differing row.
//

#include "window_leg_support.hpp"

#include "phantomledger/pipeline/simulate.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {

using pltest::Txn;

struct MonolithicResult {
  std::string digest;
  std::vector<Txn> rows;
  std::uint64_t fraudRows = 0;
  std::uint64_t bookHash = 0;
};

[[nodiscard]] MonolithicResult
runMonolithic(const pltest::pl::synth::pii::PoolSet &poolSet,
              std::uint64_t seed, pltest::pl::time::Window window) {
  namespace pl = pltest::pl;

  const auto fraudProfile = pltest::scaledFraudProfile();

  pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = 300,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, seed};

  auto scope = pipeline.transferStage().legit().runScope();
  scope.window = window;
  scope.seed = seed;
  scope.chunkStrategy = pl::pipeline::chunk::Strategy{}; // 1 month / 6 days
  pipeline.transferStage().legit().runScope(scope);

  // Family runs with the production default scenario on this leg; the
  // windowed leg replicates it via LegOptions::withFamily.

  const auto result = pipeline.run();

  MonolithicResult out;
  out.rows = result.transfers.ledger.posted.txns;
  out.fraudRows =
      static_cast<std::uint64_t>(result.transfers.fraud.injectedCount);
  out.bookHash =
      pltest::acceptance::hashBook(*result.transfers.ledger.posted.book);

  const auto wrap = pl::pipeline::chunk::Schedule::unpartitioned(window);
  pl::exporter::sinks::Golden golden;
  golden.beginSpan(*wrap.begin());
  golden.append(std::span<const Txn>(out.rows.data(), out.rows.size()));
  golden.endSpan(*wrap.begin());
  golden.finish();
  out.digest = golden.digest();

  return out;
}

// True when both corpora contain the same multiset of full-audit rows,
// i.e. any digest difference is pure tie permutation.
[[nodiscard]] bool canonicallyEqual(std::vector<Txn> a, std::vector<Txn> b) {
  if (a.size() != b.size()) {
    return false;
  }

  const auto auditLess = [](const Txn &x, const Txn &y) {
    return pltest::pl::transactions::detail::auditKey(x) <
           pltest::pl::transactions::detail::auditKey(y);
  };
  std::sort(a.begin(), a.end(), auditLess);
  std::sort(b.begin(), b.end(), auditLess);

  for (std::size_t i = 0; i < a.size(); ++i) {
    if (pltest::pl::transactions::detail::auditKey(a[i]) !=
        pltest::pl::transactions::detail::auditKey(b[i])) {
      return false;
    }
  }
  return true;
}

void printChannelHistogramDelta(const std::vector<Txn> &mono,
                                const std::vector<Txn> &windowed) {
  std::map<unsigned, std::pair<std::size_t, std::size_t>> byChannel;
  for (const auto &txn : mono) {
    ++byChannel[static_cast<unsigned>(txn.session.channel.value)].first;
  }
  for (const auto &txn : windowed) {
    ++byChannel[static_cast<unsigned>(txn.session.channel.value)].second;
  }

  std::fprintf(stderr,
               "  per-channel counts (only channels that differ):\n"
               "    channel   monolithic   windowed   delta\n");
  bool any = false;
  for (const auto &[channel, counts] : byChannel) {
    if (counts.first == counts.second) {
      continue;
    }
    any = true;
    std::fprintf(stderr, "    0x%02x      %9zu   %8zu   %+lld\n", channel,
                 counts.first, counts.second,
                 static_cast<long long>(counts.second) -
                     static_cast<long long>(counts.first));
  }
  if (!any) {
    std::fprintf(stderr, "    (none — every channel count matches)\n");
  }
}

void printBeyondWindowCounts(const std::vector<Txn> &mono,
                             const std::vector<Txn> &windowed,
                             pltest::pl::time::Window window) {
  const auto endEpoch =
      pltest::pl::time::toEpochSeconds(window.endExcl());

  const auto countBeyond = [endEpoch](const std::vector<Txn> &rows) {
    std::size_t n = 0;
    for (const auto &txn : rows) {
      if (txn.timestamp >= endEpoch) {
        ++n;
      }
    }
    return n;
  };

  std::fprintf(stderr,
               "  rows at/after window end: %zu (monolithic) vs %zu "
               "(windowed)\n",
               countBeyond(mono), countBeyond(windowed));
}

void printDropMap(const char *label,
                  const pltest::acceptance::RunFingerprint::ReasonCounts &map) {
  std::fprintf(stderr, "  windowed %s:\n", label);
  if (map.empty()) {
    std::fprintf(stderr, "    (empty)\n");
    return;
  }
  for (const auto &[reason, count] : map) {
    std::fprintf(stderr, "    %-24s %u\n", reason.c_str(), count);
  }
}

void printChannelDropMap(
    const char *label,
    const pltest::acceptance::RunFingerprint::ChannelCounts &map) {
  std::fprintf(stderr, "  windowed %s:\n", label);
  if (map.empty()) {
    std::fprintf(stderr, "    (empty)\n");
    return;
  }
  for (const auto &[key, count] : map) {
    std::fprintf(stderr, "    %-20s ch=0x%02x   %u\n", key.first.c_str(),
                 key.second, count);
  }
}

} // namespace

int main() {
  std::printf("=== Monolithic vs Windowed Equivalence (complete model) ===\n");

  constexpr std::uint64_t seed = 20260720;

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::announceLeg("monolithic pipeline");
  const auto mono = runMonolithic(poolSet, seed, window);
  std::printf("  monolithic: rows=%zu fraud=%llu digest=%s\n",
              mono.rows.size(),
              static_cast<unsigned long long>(mono.fraudRows),
              mono.digest.c_str());
  std::fflush(stdout);

  PL_CHECK(!mono.rows.empty());
  PL_CHECK(mono.fraudRows > 0);

  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 0;
  options.settlementLookaheadDays = 6; // match the monolithic default
  options.withBaseRoutines = true;
  options.withFamily = true;
  options.threadCount = 0; // machine-resolved, matching SpendingRoutine::run

  pltest::announceLeg("windowed two-phase composition");
  const auto windowed = pltest::runLeg(poolSet, options);
  pltest::printLeg("windowed", windowed);

  PL_CHECK(windowed.fingerprint.rows > 0);
  PL_CHECK(windowed.fingerprint.fraudRows > 0);

  const bool rowsEqual = mono.rows.size() == windowed.rows.size();
  const bool digestEqual = mono.digest == windowed.fingerprint.digest;
  const bool fraudEqual = mono.fraudRows == windowed.fingerprint.fraudRows;
  const bool bookEqual = mono.bookHash == windowed.fingerprint.bookHash;

  if (rowsEqual && digestEqual && fraudEqual && bookEqual) {
    std::printf("ARCHITECTURE EQUIVALENCE HOLDS (complete model): monolithic "
                "and windowed corpora are byte-identical (digest %s).\n",
                mono.digest.c_str());
    return 0;
  }

  std::fprintf(stderr, "[equivalence] architectures diverge:\n");
  if (!rowsEqual) {
    std::fprintf(stderr, "  rows: %zu (monolithic) vs %zu (windowed)\n",
                 mono.rows.size(), windowed.rows.size());
  }
  if (!fraudEqual) {
    std::fprintf(stderr, "  fraudRows: %llu vs %llu\n",
                 static_cast<unsigned long long>(mono.fraudRows),
                 static_cast<unsigned long long>(
                     windowed.fingerprint.fraudRows));
  }
  if (!digestEqual) {
    std::fprintf(stderr, "  digest: %s vs %s\n", mono.digest.c_str(),
                 windowed.fingerprint.digest.c_str());
  }
  if (!bookEqual) {
    std::fprintf(stderr, "  bookHash: 0x%llx vs 0x%llx\n",
                 static_cast<unsigned long long>(mono.bookHash),
                 static_cast<unsigned long long>(
                     windowed.fingerprint.bookHash));
  }

  printChannelHistogramDelta(mono.rows, windowed.rows);
  printBeyondWindowCounts(mono.rows, windowed.rows, window);

  std::fprintf(
      stderr,
      "  windowed cursor sources: base emitted=%llu remaining=%llu | "
      "products emitted=%llu remaining=%llu\n",
      static_cast<unsigned long long>(windowed.baseSourceEmitted),
      static_cast<unsigned long long>(windowed.baseSourceRemaining),
      static_cast<unsigned long long>(windowed.productSourceEmitted),
      static_cast<unsigned long long>(windowed.productSourceRemaining));

  printDropMap("preDropsByReason", windowed.fingerprint.preDropsByReason);
  printChannelDropMap("preDropsByChannel",
                      windowed.fingerprint.preDropsByChannel);
  printDropMap("postDropsByReason", windowed.fingerprint.postDropsByReason);
  printChannelDropMap("postDropsByChannel",
                      windowed.fingerprint.postDropsByChannel);

  if (canonicallyEqual(mono.rows, windowed.rows)) {
    std::fprintf(stderr,
                 "  VERDICT: corpora are IDENTICAL up to equal-funds-key tie "
                 "permutation (the registered ordering risk). Resolution is "
                 "a tie-breaking comparator re-pin (requires approval) or a "
                 "canonicalized comparison order.\n");
  } else {
    std::fprintf(stderr,
                 "  VERDICT: SEMANTIC divergence (row multisets differ), not "
                 "just tie ordering. First differing row in output order:\n");
    pltest::reportFirstRowDifference(mono.rows, windowed.rows);
  }

  std::fprintf(stderr,
               "[equivalence] HARD FAILURE: architecture equivalence "
               "regressed\n");
  return EXIT_FAILURE;
}
