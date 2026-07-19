#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"

#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace legitLedger = ::PhantomLedger::transfers::legit::ledger;

// Compact when the consumed prefix is large, or when at least half of the
// backing vector has been consumed.
constexpr std::size_t kCompactionThreshold = 4'096;

[[nodiscard]] transactions::Comparator chronological() noexcept {
  return transactions::Comparator{
      transactions::Comparator::Scope::fundsTransfer,
  };
}

[[nodiscard]] bool
isReplaySorted(const std::vector<transactions::Transaction> &rows) {
  return std::is_sorted(rows.begin(), rows.end(), chronological());
}

} // namespace

// ------------------------------------------------- PrecomputedCursorSource

PrecomputedCursorSource::PrecomputedCursorSource(
    std::vector<transactions::Transaction> replaySortedRows)
    : rows_(std::move(replaySortedRows)) {
  if (!isReplaySorted(rows_)) {
    throw std::invalid_argument(
        "PrecomputedCursorSource requires replay-sorted rows");
  }
}

void PrecomputedCursorSource::emitUntil(
    std::int64_t endExclusiveEpochSeconds,
    std::vector<transactions::Transaction> &out) {
  if (boundSeen_ && endExclusiveEpochSeconds < lastBoundExcl_) {
    throw std::logic_error(
        "PrecomputedCursorSource::emitUntil bounds must be monotone");
  }

  lastBoundExcl_ = endExclusiveEpochSeconds;
  boundSeen_ = true;

  const auto first = rows_.begin() + static_cast<std::ptrdiff_t>(cursor_);

  const auto split = std::lower_bound(
      first, rows_.end(), endExclusiveEpochSeconds,
      [](const transactions::Transaction &txn, std::int64_t bound) {
        return txn.timestamp < bound;
      });

  const auto emittedNow = static_cast<std::size_t>(std::distance(first, split));

  out.reserve(out.size() + emittedNow);

  out.insert(out.end(), std::make_move_iterator(first),
             std::make_move_iterator(split));

  cursor_ += emittedNow;
  emittedTotal_ += emittedNow;

  maybeCompact();
}

void PrecomputedCursorSource::maybeCompact() {
  if (cursor_ == 0) {
    return;
  }

  const bool largePrefix = cursor_ >= kCompactionThreshold;

  const bool mostlyConsumed = cursor_ >= rows_.size() - cursor_;

  if (!largePrefix && !mostlyConsumed) {
    return;
  }

  rows_.erase(rows_.begin(),
              rows_.begin() + static_cast<std::ptrdiff_t>(cursor_));

  cursor_ = 0;
}

// ------------------------------------------------------------- builders

std::unique_ptr<PrecomputedCursorSource>
makeProductSource(
    time::Window window, std::uint64_t seed,
    const random::RngFactory &rngFactory, const transactions::Factory &txf,
    const pipeline::Holdings &holdings,
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates) {
  // Dedicated lane: product precomputation must not consume the persistent
  // Session's sequential generation RNG.
  auto productRng = rngFactory.rng({"products", "full_schedule"});

  // Factory::make() also consumes RNG for device/IP routing, so it must use
  // the same isolated product lane.
  const auto productTxf = txf.rebound(productRng);

  const auto primaryByPerson = primaryAccounts(holdings);

  ProductTxnEmitter emitter{
      window,
      seed,
      productRng,
      productTxf,
  };

  std::vector<transactions::Transaction> merged;

  // Match ProductReplay::merge exactly: each newly generated source is
  // sorted and merged through the existing replay-order helper.
  merged = legitLedger::mergeReplaySorted(
      std::move(merged), emitter.premiums(holdings, primaryByPerson));

  merged = legitLedger::mergeReplaySorted(
      std::move(merged),
      emitter.claims(claimRates, holdings, primaryByPerson));

  merged = legitLedger::mergeReplaySorted(
      std::move(merged), emitter.obligations(holdings, primaryByPerson));

  return std::make_unique<PrecomputedCursorSource>(std::move(merged));
}

std::unique_ptr<PrecomputedCursorSource>
makeFraudSource(const ::PhantomLedger::transfers::fraud::Injector &injector,
                time::Window window, std::size_t realizedBaseCount,
                ::PhantomLedger::transfers::fraud::InjectorLegitCounterparties
                    counterparties) {
  auto output =
      injector.inject(window, realizedBaseCount, std::move(counterparties));

  // Injector output is not promised to be globally replay-sorted.
  auto replaySorted = legitLedger::sortForReplay(std::move(output.injected));

  return std::make_unique<PrecomputedCursorSource>(std::move(replaySorted));
}

} // namespace PhantomLedger::pipeline::stages::transfers
