#pragma once
/*
  Precomputed cursor sources for product transactions and fraud.

  PRODUCTS. PremiumGenerator, ClaimScheduler and obligations::Scheduler consume
  sequential RNG state, so they cannot be invoked separately for each
  generation window without making window size output-affecting. Products are
  therefore generated once over the complete range on a dedicated,
  content-keyed product RNG lane — `products / full_schedule`. That isolates
  product generation from the persistent spending Session, so constructing the
  product schedule cannot advance or otherwise perturb the spending RNG.

  Product generation keeps the source order `premiums -> claims -> obligations`
  and uses the same mergeReplaySorted() operation as ProductReplay. The
  obligation events feeding the scheduler are derived transiently from the
  supplied ObligationSynthesis; the world retains only the burden slice.

  FRAUD. realizedBaseCount must be the exact number of rows accepted by the
  authoritative pre-fraud replay, so the fraud source cannot be constructed
  before Phase A has completed. It is created at the boundary between pre-fraud
  and post-fraud replay, then drained chronologically during Phase B.

  MEMORY. These sources retain transaction-scale schedules; their consumed
  prefixes are compacted as the driver advances. Removing this storage entirely
  would need content-keyed per-occurrence product generation.
 */

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

/* Cursor over a fully materialized row set already sorted by the project's
 * funds-transfer replay ordering. emitUntil() appends rows whose timestamps
 * are strictly less than the supplied bound; bounds may repeat but must never
 * move backward. */
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

/* Generates the complete product schedule on a dedicated deterministic
 * product RNG lane. The supplied transaction factory is rebound to that lane
 * so device/IP routing draws are isolated as well. The primary-account index
 * is derived internally from holdings (primaryAccounts() is pure and
 * RNG-free), so callers pass the world, not its derivations.
 * `obligationSynthesis` must be the same configuration that built the world's
 * portfolio terms — the emitter replays it for the transient whole-window
 * obligation stream — and it is only read during this call. */
[[nodiscard]] std::unique_ptr<PrecomputedCursorSource> makeProductSource(
    time::Window window, std::uint64_t seed,
    const random::RngFactory &rngFactory, const transactions::Factory &txf,
    const pipeline::People &people, const pipeline::Holdings &holdings,
    const stages::products::ObligationSynthesis &obligationSynthesis,
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates);

/* Must be called only after the authoritative pre-fraud replay has completed.
 * realizedBaseCount is the exact number of ACCEPTED pre-fraud candidate rows;
 * it must not be a planned spending target or a raw proposed-row count. */
[[nodiscard]] std::unique_ptr<PrecomputedCursorSource>
makeFraudSource(const ::PhantomLedger::transfers::fraud::Injector &injector,
                time::Window window, std::size_t realizedBaseCount,
                ::PhantomLedger::transfers::fraud::InjectorLegitCounterparties
                    counterparties);

} // namespace PhantomLedger::pipeline::stages::transfers
