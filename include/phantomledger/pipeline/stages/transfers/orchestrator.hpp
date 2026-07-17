#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud_ns = ::PhantomLedger::transfers::fraud;

// Configuration for the windowed two-phase production run.
struct WindowedRunOptions {
  // Bounded generation windows; the lookahead covers card finalization
  // lag so a window's finalized coverage settles cleanly.
  chunk::Strategy generation{
      .monthsPerChunk = 3,
      .lookaheadDays = 35,
  };

  // Settlement must mirror the monolithic replay schedule (the default
  // chunk::Strategy: 1 month / 6 days) for the corpora to stay
  // byte-identical.
  chunk::Strategy settlement{};

  // Spending emission workers; nullopt leaves the count machine-resolved,
  // exactly like the monolithic SpendingRoutine::run. Output is
  // thread-count-invariant either way (test_thread_invariance).
  std::optional<std::uint32_t> threadCount{};

  // File-backed candidate spool (bounded memory, the production default);
  // false retains the O(L) in-memory vector spool.
  bool binarySpool = true;
};

struct WindowedRunResult {
  RunSummary summary{};

  // The final posted book, handed off from the driver after Phase B. The
  // AML exporters' account vertices read balances from it; hash below is
  // computed from this same book for fingerprint comparison.
  std::unique_ptr<clearing::Ledger> postedBook;

  // FNV hash of the final posted book, comparable with
  // acceptance::hashBook on the monolithic result's book.
  std::uint64_t postedBookHash = 0;

  // Candidate rows/bytes that crossed the Phase A / Phase B boundary
  // through the binary spool (zero when binarySpool is false).
  std::uint64_t spoolRows = 0;
  std::uint64_t spoolBytes = 0;
};

class TransferStage {
public:
  TransferStage() = default;

  [[nodiscard]] legit::LegitAssembly &legit() noexcept;
  [[nodiscard]] const legit::LegitAssembly &legit() const noexcept;

  [[nodiscard]] ProductReplay &products() noexcept;
  [[nodiscard]] const ProductReplay &products() const noexcept;

  [[nodiscard]] LedgerReplay &ledger() noexcept;
  [[nodiscard]] const LedgerReplay &ledger() const noexcept;

  [[nodiscard]] FraudEmission &fraud() noexcept;
  [[nodiscard]] const FraudEmission &fraud() const noexcept;

  TransferStage &infra(const pipeline::Infra &value);

  [[nodiscard]] legit::ledger::LegitTransferResult
  buildLegit(random::Rng &rng, const pipeline::People &people,
             const pipeline::Holdings &holdings,
             const pipeline::Counterparties &cps) const;

  [[nodiscard]] std::vector<transactions::Transaction>
  mergeProducts(random::Rng &rng, const pipeline::Holdings &holdings,
                legit::ledger::LegitTxnStreams streams) const;

  [[nodiscard]] fraud_ns::Injector
  makeFraudInjector(random::Rng &rng, const pipeline::People &people,
                    const pipeline::Holdings &holdings) const;

  // Windowed two-phase production run: Session generation and pre-fraud
  // fold into a candidate spool, exact-realized-count fraud boundary,
  // post-fraud fold streaming into `sink`. Byte-identical to the
  // monolithic composition (test_production_windowed); posted rows never
  // accumulate in memory, and with options.binarySpool the candidate
  // corpus lives on disk. Rows are validated against the account registry
  // as they stream through, matching the monolithic posted-corpus
  // validation.
  template <class S>
  [[nodiscard]] WindowedRunResult
  runWindowed(random::Rng &rng, const pipeline::People &people,
              const pipeline::Holdings &holdings,
              const pipeline::Counterparties &cps, S &sink,
              const WindowedRunOptions &options = {}) const {
    SinkRef ref(sink);
    return runWindowedErased(rng, people, holdings, cps, ref, options);
  }

  [[nodiscard]] WindowedRunResult
  runWindowedErased(random::Rng &rng, const pipeline::People &people,
                    const pipeline::Holdings &holdings,
                    const pipeline::Counterparties &cps, SinkRef sink,
                    const WindowedRunOptions &options) const;

private:
  legit::LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
  const pipeline::Infra *infra_ = nullptr;

  // Pristine snapshot of the router, taken when infra() is set — before
  // any transfer routing. The Router carries mutable sticky per-person
  // device/IP state, so routing is order-dependent across generators.
  // Products route from this copy so product generation cannot perturb
  // (or be perturbed by) the sticky state that spending and fraud share;
  // generation order stops being output-defining, which the windowed
  // decomposition requires. The windowed run takes its product AND family
  // copies from this same snapshot.
  ::PhantomLedger::infra::Router productRouter_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
