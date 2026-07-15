#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;

namespace {

int failures = 0;

void expect(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

[[nodiscard]] std::string
digestForStrategy(const pl::synth::pii::PoolSet &poolSet, std::uint64_t seed,
                  pl::pipeline::chunk::Strategy strategy) {
  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2; // two years: enough to exercise several chunks

  const pl::synth::people::Fraud fraudProfile{};

  pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = 300, // small enough to be fast, large enough for rings
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, seed};

  auto scope = pipeline.transferStage().legit().runScope();
  scope.window = window;
  scope.seed = seed;
  scope.chunkStrategy = strategy;
  pipeline.transferStage().legit().runScope(scope);
  pipeline.transferStage()
      .legit()
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  const auto result = pipeline.run();

  const auto wrap = pl::pipeline::chunk::Schedule::unpartitioned(window);
  pl::exporter::sinks::Golden g;
  g.beginSpan(*wrap.begin());
  g.append(std::span<const pl::transactions::Transaction>(
      result.transfers.ledger.posted.txns.data(),
      result.transfers.ledger.posted.txns.size()));
  g.endSpan(*wrap.begin());
  g.finish();
  return g.digest();
}

} // namespace

int main() {
  constexpr std::uint64_t seed = 20260715;
  const auto poolSet = buildPoolSet(seed);

  // Reference: the production default (1 month per chunk).
  const auto reference =
      digestForStrategy(poolSet, seed, pl::pipeline::chunk::Strategy{});
  expect(reference.size() == 64, "reference digest is 64 hex chars");

  // Invariance across chunk SIZE at the default lookahead.
  const auto unpartitioned = digestForStrategy(
      poolSet, seed, pl::pipeline::chunk::Strategy{.monthsPerChunk = 1200});
  expect(unpartitioned == reference,
         "whole-window partition matches 1-month default (" + unpartitioned +
             " vs " + reference + ")");

  const auto threeMonth = digestForStrategy(
      poolSet, seed, pl::pipeline::chunk::Strategy{.monthsPerChunk = 3});
  expect(threeMonth == reference, "3-month chunks match 1-month default (" +
                                      threeMonth + " vs " + reference + ")");

  const auto sixMonth = digestForStrategy(
      poolSet, seed, pl::pipeline::chunk::Strategy{.monthsPerChunk = 6});
  expect(sixMonth == reference, "6-month chunks match 1-month default (" +
                                    sixMonth + " vs " + reference + ")");

  // Diagnostic (printed, not asserted): does a LONGER lookahead change
  // the corpus? If it does, some cure horizon exceeds the default and
  // the design audit's lookahead-sufficiency item has a concrete case.
  const auto longLookahead = digestForStrategy(
      poolSet, seed,
      pl::pipeline::chunk::Strategy{.monthsPerChunk = 1, .lookaheadDays = 30});
  if (longLookahead != reference) {
    std::fprintf(stderr,
                 "DIAGNOSTIC: 30-day lookahead changes the digest (%s vs "
                 "%s). Default 6-day lookahead misses some cure horizon; "
                 "revisit Strategy.lookaheadDays before shrinking chunks in "
                 "production.\n",
                 longLookahead.c_str(), reference.c_str());
  } else {
    std::printf("diagnostic: 6-day and 30-day lookahead agree; 6 days covers "
                "the cure horizon at this scale.\n");
  }

  if (failures != 0) {
    std::fprintf(stderr, "chunk-invariance: %d failure(s)\n", failures);
    return EXIT_FAILURE;
  }

  std::printf("chunk-strategy invariance holds: unpartitioned, 1/3/6-month "
              "chunks all yield digest %s\n",
              reference.c_str());
  return EXIT_SUCCESS;
}
