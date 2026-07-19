//
// tests/test_production_windowed.cpp
//
// Production windowed mode acceptance: SimulationPipeline::runWindowed()
// (Session generation, binary candidate spool, streaming sink) must
// reproduce SimulationPipeline::run() (the golden-pinned monolithic
// composition) byte-for-byte — rows, Golden digest, fraud row count and
// final posted-book hash — from the same seed and entity configuration.
//
// This differs from test_arch_equivalence: that gate proves the windowed
// COMPOSITION through the test harness; this gate proves the PRODUCTION
// API end to end, including the generation prologue seam
// (LegitTransferBuilder::buildWindowedPrologue / buildFamilyRows), the
// shared card-lifecycle config, the streaming account validation, the
// file-backed spool as the production default, and the posted-book
// handoff the AML exporters depend on.
//
// HARD-ENFORCED.
//

#include "window_leg_support.hpp"

#include "phantomledger/pipeline/simulate.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using pltest::Txn;
namespace pl = pltest::pl;

struct MonolithicResult {
  std::string digest;
  std::vector<Txn> rows;
  std::uint64_t fraudRows = 0;
  std::uint64_t bookHash = 0;
};

[[nodiscard]] pl::pipeline::stages::entities::EntitySynthesis
makeEntities(const pl::synth::pii::PoolSet &poolSet,
             pl::time::Window window) {
  return pl::pipeline::stages::entities::EntitySynthesis{
      .population = 300,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = pltest::scaledFraudProfile(),
  };
}

[[nodiscard]] MonolithicResult
runMonolithic(const pl::synth::pii::PoolSet &poolSet, std::uint64_t seed,
              pl::time::Window window) {
  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window,
                                            makeEntities(poolSet, window),
                                            seed};

  auto scope = pipeline.transferStage().legit().runScope();
  scope.window = window;
  scope.seed = seed;
  pipeline.transferStage().legit().runScope(scope);
  pipeline.transferStage().settlementChunking(
      pl::pipeline::chunk::Strategy{}); // 1 month / 6 days

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

} // namespace

int main() {
  std::printf("=== Production windowed mode vs monolithic run ===\n");

  constexpr std::uint64_t seed = 20260722;

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::announceLeg("monolithic pipeline.run()");
  const auto mono = runMonolithic(poolSet, seed, window);
  std::printf("  monolithic: rows=%zu fraud=%llu digest=%s\n",
              mono.rows.size(),
              static_cast<unsigned long long>(mono.fraudRows),
              mono.digest.c_str());
  std::fflush(stdout);

  PL_CHECK(!mono.rows.empty());
  PL_CHECK(mono.fraudRows > 0);

  pltest::announceLeg("production pipeline.runWindowed()");

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window,
                                            makeEntities(poolSet, window),
                                            seed};

  pltest::CapturingGolden sink;
  // Default options: 3-month generation windows, the monolithic
  // settlement strategy, machine-resolved spending threads, binary spool.
  const auto windowed = pipeline.runWindowed(sink);

  const auto &summary = windowed.transfers.summary;
  std::printf("  windowed:   rows=%llu L=%llu fraud=%llu digest=%s\n",
              static_cast<unsigned long long>(sink.rowsWritten()),
              static_cast<unsigned long long>(summary.phaseA.candidateRows),
              static_cast<unsigned long long>(summary.phaseB.fraudRows),
              sink.golden.digest().c_str());
  std::printf("  spool file: rows=%llu bytes=%llu (%.1f MiB)\n",
              static_cast<unsigned long long>(windowed.transfers.spoolRows),
              static_cast<unsigned long long>(windowed.transfers.spoolBytes),
              static_cast<double>(windowed.transfers.spoolBytes) /
                  (1024.0 * 1024.0));
  std::fflush(stdout);

  // The production default really is the file spool, and every accepted
  // candidate crossed through it.
  PL_CHECK(windowed.transfers.spoolRows == summary.phaseA.candidateRows);
  PL_CHECK(windowed.transfers.spoolBytes > 0);

  // The posted-book handoff (the AML exporters' account vertices depend
  // on it): present, and consistent with the reported hash.
  PL_CHECK(windowed.transfers.postedBook != nullptr);
  PL_CHECK(pltest::acceptance::hashBook(*windowed.transfers.postedBook) ==
           windowed.transfers.postedBookHash);

  const bool rowsEqual = mono.rows.size() == sink.rows.size();
  const bool digestEqual = mono.digest == sink.golden.digest();
  const bool fraudEqual = mono.fraudRows == summary.phaseB.fraudRows;
  const bool bookEqual = mono.bookHash == windowed.transfers.postedBookHash;

  if (rowsEqual && digestEqual && fraudEqual && bookEqual) {
    std::printf("PRODUCTION WINDOWED MODE HOLDS: runWindowed() reproduces "
                "run() byte-for-byte (digest %s).\n",
                mono.digest.c_str());
    return 0;
  }

  std::fprintf(stderr, "[production-windowed] paths diverge:\n");
  if (!rowsEqual) {
    std::fprintf(stderr, "  rows: %zu (monolithic) vs %zu (windowed)\n",
                 mono.rows.size(), sink.rows.size());
  }
  if (!fraudEqual) {
    std::fprintf(stderr, "  fraudRows: %llu vs %llu\n",
                 static_cast<unsigned long long>(mono.fraudRows),
                 static_cast<unsigned long long>(summary.phaseB.fraudRows));
  }
  if (!digestEqual) {
    std::fprintf(stderr, "  digest: %s vs %s\n", mono.digest.c_str(),
                 sink.golden.digest().c_str());
  }
  if (!bookEqual) {
    std::fprintf(stderr, "  bookHash: 0x%llx vs 0x%llx\n",
                 static_cast<unsigned long long>(mono.bookHash),
                 static_cast<unsigned long long>(
                     windowed.transfers.postedBookHash));
  }

  pltest::reportFirstRowDifference(mono.rows, sink.rows);

  std::fprintf(stderr, "[production-windowed] HARD FAILURE: the production "
                       "windowed mode regressed\n");
  return EXIT_FAILURE;
}
