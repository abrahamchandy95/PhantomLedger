#pragma once
//
// phantomledger/pipeline/stages/transfers/window_sources.hpp
//
// Precomputed cursor sources for product transactions and fraud.
//
// PRODUCTS
// PremiumGenerator, ClaimScheduler and obligations::Scheduler currently
// consume sequential RNG state. They therefore cannot be invoked separately
// for each generation window without making window size output-affecting.
//
// Products are generated once over the complete range using a dedicated,
// content-keyed product RNG lane:
//
//   products / full_schedule
//
// This is an explicit product-stream re-pin. It isolates product generation
// from the persistent spending Session so constructing the product schedule
// cannot advance or otherwise perturb the spending RNG.
//
// Product generation retains the existing source order:
//
//   premiums -> claims -> obligations
//
// and uses the same mergeReplaySorted() operation as ProductReplay.
//
// FRAUD
// Fraud retains the legacy budget meaning. realizedBaseCount must be the
// exact number of rows accepted by the authoritative pre-fraud replay.
//
// Consequently, the fraud source cannot be constructed before Phase A has
// completed. It is created at the boundary between pre-fraud and post-fraud
// replay, then drained chronologically during Phase B.
//
// MEMORY
// These sources retain transaction-scale schedules. Their consumed prefixes
// are compacted as the driver advances. Replacing this storage completely
// requires content-keyed per-occurrence product generation later.
//

#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

// Cursor over a fully materialized row set that is already sorted according
// to the project's funds-transfer replay ordering.
//
// emitUntil() appends rows whose timestamps are strictly less than the
// supplied bound. Bounds may repeat but must never move backward.
class PrecomputedCursorSource final : public ScheduleCursorSource {
public:
  explicit PrecomputedCursorSource(
      std::vector<transactions::Transaction> replaySortedRows);

  void emitUntil(std::int64_t endExclusiveEpochSeconds,
                 std::vector<transactions::Transaction> &out) override;

  [[nodiscard]] std::size_t remaining() const noexcept {
    return rows_.size() - cursor_;
  }

  [[nodiscard]] std::uint64_t emittedTotal() const noexcept {
    return emittedTotal_;
  }

private:
  void maybeCompact();

  std::vector<transactions::Transaction> rows_;
  std::size_t cursor_ = 0;
  std::uint64_t emittedTotal_ = 0;

  std::int64_t lastBoundExcl_ = 0;
  bool boundSeen_ = false;
};

// Generates the complete product schedule with a dedicated deterministic
// product RNG lane. The supplied transaction factory is rebound to that
// lane so device/IP routing draws are isolated as well. The primary-account
// index is derived internally from holdings (primaryAccounts() is pure and
// RNG-free): callers pass the world, not its derivations.
[[nodiscard]] std::unique_ptr<PrecomputedCursorSource>
makeProductSource(time::Window window, std::uint64_t seed,
                  const random::RngFactory &rngFactory,
                  const transactions::Factory &txf,
                  const pipeline::Holdings &holdings,
                  ::PhantomLedger::transfers::insurance::ClaimRates claimRates);

// Must be called only after authoritative pre-fraud replay has completed.
//
// realizedBaseCount is the exact number of accepted pre-fraud candidate
// rows. It must not be a planned spending target or raw proposed-row count.
[[nodiscard]] std::unique_ptr<PrecomputedCursorSource>
makeFraudSource(const ::PhantomLedger::transfers::fraud::Injector &injector,
                time::Window window, std::size_t realizedBaseCount,
                ::PhantomLedger::transfers::fraud::InjectorLegitCounterparties
                    counterparties);

} // namespace PhantomLedger::pipeline::stages::transfers
