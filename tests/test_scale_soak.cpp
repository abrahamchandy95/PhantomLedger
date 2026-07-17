//
// tests/test_scale_soak.cpp
//
// Opt-in scale soak for the production windowed engine (roadmap: scale
// soaks). Skips (77) unless PL_SOAK=1, so `make test` stays fast;
// configure with:
//
//   PL_SOAK=1            run the soak
//   PL_SOAK_POP=N        population           (default 10000)
//   PL_SOAK_DAYS=N       window length, days  (default 365)
//   PL_SOAK_SEED=N       run seed             (default 20260724)
//   PL_SOAK_THREADS=N    emission workers     (default 0 = machine)
//
// The soak runs SimulationPipeline::buildWorld + runWindowedTransfers
// with the PRODUCTION DEFAULT fraud profile (real ring statistics at
// real populations — no test scaling) and a NON-RETAINING sink, then:
//
//   HARD   the total replay order (S10: funds key + audit tie-breakers)
//          holds across the entire settled stream, spanning span seams
//   HARD   every accepted candidate crossed through the binary spool
//   HARD   fraud engaged (population >= 5000, where the default profile
//          deterministically plans rings)
//   REGISTER  adjacent funds-key tie census at soak scale — since the
//          S10 re-pin these are deterministically content-ordered; the
//          count is a record of how much tie-breaking the total order
//          performs, not a risk
//   REPORT peak RSS after world build and after the fold, spool size,
//          stream digest, and wall-clock durations
//
// The digest line makes soak runs comparable across machines and
// configurations for the record. (First soak at 10k/365d, pre-S10:
// 3,047,712 rows, 3 funds-key ties, order clean, spool 191.8 MiB,
// fold RSS delta ~1.4 GB dominated by the generation prologue.)
//

#include "window_leg_support.hpp"

#include "phantomledger/pipeline/simulate.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <sys/resource.h>

namespace {

using pltest::Txn;
namespace pl = pltest::pl;

[[nodiscard]] double peakRssMB() {
  struct rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(ru.ru_maxrss) / 1024.0;
#endif
}

[[nodiscard]] std::uint64_t envU64(const char *name, std::uint64_t fallback) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return fallback;
  }
  return std::strtoull(value, nullptr, 10);
}

// Golden digest plus a streaming order check and adjacent funds-key tie
// census. Retains ONE row (the previous one), never the corpus — this is
// what makes the soak a memory measurement and not a memory consumer.
struct SoakSink {
  pl::exporter::sinks::Golden golden;

  Txn last{};
  bool haveLast = false;
  std::uint64_t adjacentTies = 0;
  std::uint64_t orderViolations = 0;

  void beginSpan(const pl::pipeline::chunk::Span &span) {
    golden.beginSpan(span);
  }

  void append(std::span<const Txn> txns) {
    // Total replay order (S10). Ties are censused on the SEMANTIC funds
    // key explicitly — under the total order, "neither compares less"
    // would mean audit-identical rows, a different question.
    constexpr pl::transactions::Comparator replayLess{
        pl::transactions::Comparator::Scope::fundsTransfer};
    for (const auto &tx : txns) {
      if (haveLast) {
        if (replayLess(tx, last)) {
          ++orderViolations;
        } else if (pl::transactions::detail::fundsKey(tx) ==
                   pl::transactions::detail::fundsKey(last)) {
          ++adjacentTies;
        }
      }
      last = tx;
      haveLast = true;
    }
    golden.append(txns);
  }

  void endSpan(const pl::pipeline::chunk::Span &span) { golden.endSpan(span); }

  void finish() { golden.finish(); }

  [[nodiscard]] std::uint64_t rowsWritten() const {
    return golden.rowsWritten();
  }
};

[[nodiscard]] double secondsBetween(std::chrono::steady_clock::time_point a,
                                    std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

} // namespace

int main() {
  const char *soak = std::getenv("PL_SOAK");
  if (soak == nullptr || std::strcmp(soak, "1") != 0) {
    std::printf("scale-soak: opt-in gate; set PL_SOAK=1 (optionally "
                "PL_SOAK_POP / PL_SOAK_DAYS / PL_SOAK_SEED / "
                "PL_SOAK_THREADS) to run\n");
    return 77;
  }

  const auto population =
      static_cast<std::int32_t>(envU64("PL_SOAK_POP", 10'000));
  const auto days = static_cast<int>(envU64("PL_SOAK_DAYS", 365));
  const std::uint64_t seed = envU64("PL_SOAK_SEED", 20'260'724ULL);
  const auto threads =
      static_cast<std::uint32_t>(envU64("PL_SOAK_THREADS", 0));

  std::printf("=== Scale soak: production windowed engine ===\n");
  std::printf("  population=%d  days=%d  seed=%llu  threads=%u (0=machine)\n",
              population, days, static_cast<unsigned long long>(seed),
              threads);
  std::fflush(stdout);

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = days;

  const auto poolSet = pltest::buildPoolSet(seed);

  // PRODUCTION DEFAULT fraud profile: at soak populations the planned
  // ring count is real (~6 rings per 10k people). Do NOT substitute the
  // gates' scaled profile here — the soak validates real statistics.
  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = population,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
  };

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, seed};

  using Clock = std::chrono::steady_clock;
  const auto tStart = Clock::now();

  pltest::announceLeg("world build (entities, products, infra)");
  auto world = pipeline.buildWorld();
  const auto tWorld = Clock::now();
  const auto rssWorld = peakRssMB();

  std::printf("  world: people=%u accounts=%zu  %.1fs  peakRSS=%.1f MB\n",
              static_cast<unsigned>(world.people.roster.roster.count),
              world.holdings.accounts.registry.records.size(),
              secondsBetween(tStart, tWorld), rssWorld);
  std::fflush(stdout);

  SoakSink sink;

  pl::pipeline::SimulationPipeline::WindowedRunOptions options;
  if (threads != 0) {
    options.threadCount = threads;
  }

  pltest::announceLeg("windowed transfer fold (binary spool)");
  const auto transfers = pipeline.runWindowedTransfers(world, sink, options);
  const auto tFold = Clock::now();
  const auto rssEnd = peakRssMB();

  const auto &summary = transfers.summary;

  std::printf("  rows=%llu  digest=%s\n",
              static_cast<unsigned long long>(sink.rowsWritten()),
              sink.golden.digest().c_str());
  std::printf("  fraud=%llu  candidates L=%llu  card events=%llu\n",
              static_cast<unsigned long long>(summary.phaseB.fraudRows),
              static_cast<unsigned long long>(summary.phaseA.candidateRows),
              static_cast<unsigned long long>(summary.phaseA.cardEvents));
  std::printf("  spool: rows=%llu  %.1f MiB on disk\n",
              static_cast<unsigned long long>(transfers.spoolRows),
              static_cast<double>(transfers.spoolBytes) / (1024.0 * 1024.0));
  std::printf("  tie register: adjacent funds-key ties=%llu  order "
              "violations=%llu\n",
              static_cast<unsigned long long>(sink.adjacentTies),
              static_cast<unsigned long long>(sink.orderViolations));
  std::printf("  peakRSS: %.1f MB after world, %.1f MB after fold "
              "(delta %.1f MB)\n",
              rssWorld, rssEnd, rssEnd - rssWorld);
  std::printf("  wall: world %.1fs  fold %.1fs  total %.1fs\n",
              secondsBetween(tStart, tWorld), secondsBetween(tWorld, tFold),
              secondsBetween(tStart, tFold));
  std::fflush(stdout);

  PL_CHECK(sink.rowsWritten() > 0);
  PL_CHECK(sink.orderViolations == 0);
  PL_CHECK(transfers.spoolRows == summary.phaseA.candidateRows);
  PL_CHECK(transfers.spoolBytes > 0);
  PL_CHECK(sink.rowsWritten() == summary.phaseB.rowsFlushed);

  // The default profile plans ~6 rings / 10k people and ROUNDS; below
  // ~5k people a zero plan is possible, so the fraud assertion only
  // holds at real soak populations.
  if (population >= 5'000) {
    PL_CHECK(summary.phaseB.fraudRows > 0);
  }

  if (sink.adjacentTies != 0) {
    std::printf("REGISTER: %llu adjacent funds-key ties at soak scale — "
                "deterministically content-ordered by the S10 audit-key "
                "tie-breakers; recorded for the register.\n",
                static_cast<unsigned long long>(sink.adjacentTies));
  } else {
    std::printf("REGISTER CLEAN: zero adjacent funds-key ties at "
                "population %d.\n",
                population);
  }

  std::printf("SCALE SOAK COMPLETE: windowed engine held the total replay "
              "order over %llu rows with the candidate corpus on disk.\n",
              static_cast<unsigned long long>(sink.rowsWritten()));
  return 0;
}
