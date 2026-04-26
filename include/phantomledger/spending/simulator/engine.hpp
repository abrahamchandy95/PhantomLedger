#pragma once

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

/// Cross-cutting machinery the simulator needs at run time.
///
/// Phase 4 additions:
///
///  * `rngFactory` — pointer to a `random::RngFactory`. Used at the
///    start of each multi-threaded run to deterministically derive one
///    independent RNG per worker thread (via
///    `rngFactory->rng({"spending_thread", "<idx>"})`). This means
///    parallelism is fully reproducible for a fixed
///    `(baseSeed, threadCount)` pair — the per-thread RNGs are a
///    function of the seed alone, not of any prior consumption from
///    the main `rng` field.
///
///    May be left null when `threadCount == 1`; the serial path uses
///    the existing `rng` and `factory` members directly and produces
///    bit-identical output to the Phase 1+2 code.
///
///  * `threadCount` — number of worker threads for the per-spender
///    loop. Default `1` preserves the exact serial behaviour: same
///    RNG consumption order, same ledger update sequence.
///
/// When `threadCount > 1`:
///   - `rngFactory` MUST be non-null.
///   - `factory` MUST be non-null. Each worker thread gets its own
///     `Factory` instance by calling `factory->rebound(perThreadRng)`,
///     which preserves the original `router` / `ringInfra` pointers
///     while binding the new per-thread `Rng`.
struct Engine {
  random::Rng *rng = nullptr;
  const transactions::Factory *factory = nullptr;
  clearing::Ledger *ledger = nullptr;

  // ------------------------ Phase 4 additions ------------------------
  random::RngFactory *rngFactory = nullptr;
  std::uint32_t threadCount = 1;
};

} // namespace PhantomLedger::spending::simulator
