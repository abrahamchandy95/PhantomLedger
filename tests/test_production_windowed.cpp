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
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using pltest::Txn;
namespace pl = pltest::pl;

using Declined = pl::transfers::legit::ledger::DeclinedAttempt;

struct MonolithicResult {
  std::string digest;
  std::vector<Txn> rows;
  std::uint64_t fraudRows = 0;
  std::uint64_t bookHash = 0;

  /* THE FUNDING DECLINES. They are the one product of the fold that never
   * reaches the row stream, so the digest above cannot see them — which is
   * exactly how 6de9c95's asymmetry shipped: the monolithic path exported
   * declines, the windowed path exported none, and every check here stayed
   * green because both agreed on the SETTLED rows. */
  std::vector<Declined> declined;
};

/* Field-wise, because DeclinedAttempt has no operator== and comparing the
 * count alone would accept two different populations of the same size. The
 * five fields are the ones the card-fraud export reads. */
[[nodiscard]] bool sameAttempt(const Declined &a, const Declined &b) noexcept {
  return a.txn.timestamp == b.txn.timestamp && a.txn.source == b.txn.source &&
         a.txn.target == b.txn.target && a.txn.amount == b.txn.amount &&
         a.reason == b.reason;
}

/* Index of the first disagreement, or npos when the two agree. */
[[nodiscard]] std::size_t
firstDeclinedDifference(std::span<const Declined> mono,
                        std::span<const Declined> windowed) noexcept {
  const auto shared = std::min(mono.size(), windowed.size());
  for (std::size_t i = 0; i < shared; ++i) {
    if (!sameAttempt(mono[i], windowed[i])) {
      return i;
    }
  }
  return mono.size() == windowed.size() ? std::string::npos : shared;
}

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
  out.declined = result.transfers.ledger.posted.declined;

  const auto wrap = pl::pipeline::chunk::Schedule::unpartitioned(window);
  pl::exporter::sinks::Golden golden;
  golden.beginSpan(*wrap.begin());
  golden.append(std::span<const Txn>(out.rows.data(), out.rows.size()));
  golden.endSpan(*wrap.begin());
  golden.finish();
  out.digest = golden.digest();

  return out;
}

[[nodiscard]] std::size_t outsideWindowRows(std::span<const Txn> rows,
                                            pl::time::Window window) {
  const auto start = pl::time::toEpochSeconds(window.start);
  const auto end = pl::time::toEpochSeconds(window.endExcl());
  return static_cast<std::size_t>(
      std::count_if(rows.begin(), rows.end(), [start, end](const Txn &txn) {
        return txn.timestamp < start || txn.timestamp >= end;
      }));
}

[[nodiscard]] Txn makeBoundaryTxn(pl::entity::Key source,
                                  pl::entity::Key target, double amount,
                                  std::int64_t timestamp,
                                  pl::channels::Tag channel) {
  Txn txn;
  txn.source = source;
  txn.target = target;
  txn.amount = amount;
  txn.timestamp = timestamp;
  txn.session.channel = channel;
  return txn;
}

void finalWindowBoundaryUnit() {
  pl::time::Window window{
      .start = pl::time::makeTime({2025, 1, 1}),
      .days = 1,
  };
  const auto schedule = pl::pipeline::chunk::Schedule::unpartitioned(window);
  const auto start = pl::time::toEpochSeconds(window.start);
  const auto end = pl::time::toEpochSeconds(window.endExcl());

  const auto source = pl::entity::makeKey(pl::entity::Role::account,
                                          pl::entity::Bank::internal, 1);
  const auto target = pl::entity::makeKey(pl::entity::Role::account,
                                          pl::entity::Bank::internal, 2);

  pl::clearing::Ledger opening;
  opening.initialize(2);
  opening.addAccount(source, 0);
  opening.addAccount(target, 1);
  opening.cash(0) = 500.0;

  const auto merchant = pl::channels::tag(pl::channels::Legit::merchant);
  const auto past = makeBoundaryTxn(source, target, 50.0, start - 1, merchant);
  const auto active = makeBoundaryTxn(source, target, 100.0, end - 1, merchant);
  const auto future = makeBoundaryTxn(source, target, 200.0, end + 1, merchant);

  pl::pipeline::stages::transfers::LedgerReplay replay;
  auto preRng = pl::random::Rng::fromSeed(101);
  auto candidate = replay.preFraudChunked(
      opening, preRng, std::vector<Txn>{past, active, future}, schedule);

  // The pre-fraud candidate count is the active fraud-budget denominator.
  PL_CHECK(candidate.txns.size() == 1);
  PL_CHECK(candidate.txns.front().timestamp == active.timestamp);

  auto postRng = pl::random::Rng::fromSeed(202);
  auto posted =
      replay.postFraudChunkedMerged(postRng, opening, std::move(candidate.txns),
                                    std::vector<Txn>{past, future}, schedule);

  PL_CHECK(posted.txns.size() == 1);
  PL_CHECK(posted.txns.front().timestamp == active.timestamp);
  PL_CHECK(outsideWindowRows(posted.txns, window) == 0);
  PL_CHECK(posted.book->cash(0) == 400.0);
  PL_CHECK(posted.book->cash(1) == 100.0);

  // A future inbound remains visible as cure lookahead. The active debit is
  // deferred rather than terminally dropped, but neither it nor the future
  // inbound is posted outside the corpus horizon.
  pl::clearing::Ledger cureBook;
  cureBook.initialize(2);
  cureBook.addAccount(source, 0);
  cureBook.addAccount(target, 1);

  const auto bill = pl::channels::tag(pl::channels::Legit::bill);
  const auto salary = pl::channels::tag(pl::channels::Legit::salary);
  const auto externalEmployer = pl::entity::makeKey(
      pl::entity::Role::business, pl::entity::Bank::external, 3);
  const auto activeDebit =
      makeBoundaryTxn(source, target, 25.0, end - 1'800, bill);
  const auto futureCure =
      makeBoundaryTxn(externalEmployer, source, 25.0, end + 1'800, salary);

  auto funding =
      pl::pipeline::stages::transfers::LedgerReplay::FundingBehavior{};
  funding.retry.blindProbability = 0.0;
  pl::pipeline::stages::transfers::LedgerReplay cureReplay;
  cureReplay.fundingBehavior(funding);

  auto cureRng = pl::random::Rng::fromSeed(303);
  const auto deferred = cureReplay.preFraudChunked(
      cureBook, cureRng, std::vector<Txn>{activeDebit, futureCure}, schedule);
  PL_CHECK(deferred.txns.empty());
  PL_CHECK(deferred.drops.byReason.empty());
}

} // namespace

int main() {
  std::printf("=== Production windowed mode vs monolithic run ===\n");

  finalWindowBoundaryUnit();

  constexpr std::uint64_t seed = 20260722;

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::announceLeg("monolithic pipeline.run()");
  const auto mono = runMonolithic(poolSet, seed, window);
  std::printf("  monolithic: rows=%zu fraud=%llu digest=%s\n", mono.rows.size(),
              static_cast<unsigned long long>(mono.fraudRows),
              mono.digest.c_str());
  std::fflush(stdout);

  PL_CHECK(!mono.rows.empty());
  PL_CHECK(mono.fraudRows > 0);
  PL_CHECK(outsideWindowRows(mono.rows, window) == 0);

  pltest::announceLeg("production pipeline.runWindowed()");

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{
      rng, window, makeEntities(poolSet, window), seed};

  pltest::CapturingGolden sink;
  // Default options: 3-month generation windows, the monolithic
  // settlement strategy, machine-resolved spending threads, binary spool.
  // The declines are asked for explicitly — this is the production drive
  // site's wiring, and it is the thing under test.
  std::vector<Declined> windowedDeclined;
  pl::pipeline::stages::transfers::WindowedRunOptions options{};
  options.declined = &windowedDeclined;
  const auto windowed = pipeline.runWindowed(sink, options);

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
  PL_CHECK(summary.phaseB.candidateRows == summary.phaseA.candidateRows);
  PL_CHECK(outsideWindowRows(sink.rows, window) == 0);

  // The posted-book handoff (the AML exporters' account vertices depend
  // on it): present, and consistent with the reported hash.
  PL_CHECK(windowed.transfers.postedBook != nullptr);
  PL_CHECK(pltest::acceptance::hashBook(*windowed.transfers.postedBook) ==
           windowed.transfers.postedBookHash);

  /* THE DECLINES MUST AGREE, AND THE FIRST CHECK IS THAT THEY EXIST.
   *
   * A pointer wired to a path that never fills it reads as an empty vector on
   * both sides, and an equality check alone would call that a match — the
   * precise failure mode of 6de9c95, where the binary exported zero declines
   * and nothing went red. So the floor comes first: the fold must decide at
   * least one funding decline at this leg, and both engines must agree on the
   * whole population field-wise, in order. */
  std::printf("  declines:   monolithic=%zu windowed=%zu\n",
              mono.declined.size(), windowedDeclined.size());
  std::fflush(stdout);

  const bool declinedPresent = !mono.declined.empty();
  const auto declinedDiff = firstDeclinedDifference(mono.declined,
                                                    windowedDeclined);
  const bool declinedEqual = declinedDiff == std::string::npos;

  const bool rowsEqual = mono.rows.size() == sink.rows.size();
  const bool digestEqual = mono.digest == sink.golden.digest();
  const bool fraudEqual = mono.fraudRows == summary.phaseB.fraudRows;
  const bool bookEqual = mono.bookHash == windowed.transfers.postedBookHash;

  if (rowsEqual && digestEqual && fraudEqual && bookEqual && declinedPresent &&
      declinedEqual) {
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

  if (!declinedPresent) {
    std::fprintf(stderr,
                 "  declines: the monolithic fold decided NONE, so the "
                 "comparison below is vacuous. Either the leg no longer "
                 "exercises the funding test, or the replay stopped "
                 "recording attempts.\n");
  }
  if (!declinedEqual) {
    std::fprintf(stderr, "  declines: %zu (monolithic) vs %zu (windowed)",
                 mono.declined.size(), windowedDeclined.size());
    if (declinedDiff < mono.declined.size() &&
        declinedDiff < windowedDeclined.size()) {
      const auto &a = mono.declined[declinedDiff];
      const auto &b = windowedDeclined[declinedDiff];
      std::fprintf(stderr,
                   "; first difference at [%zu]: ts %lld vs %lld, amount "
                   "%.2f vs %.2f",
                   declinedDiff, static_cast<long long>(a.txn.timestamp),
                   static_cast<long long>(b.txn.timestamp), a.txn.amount,
                   b.txn.amount);
    }
    std::fprintf(stderr, "\n");
  }

  pltest::reportFirstRowDifference(mono.rows, sink.rows);

  std::fprintf(stderr, "[production-windowed] HARD FAILURE: the production "
                       "windowed mode regressed\n");
  return EXIT_FAILURE;
}
