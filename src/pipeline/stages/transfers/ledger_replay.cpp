#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"

#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;

LedgerReplay &LedgerReplay::ordering(Ordering value) noexcept {
  ordering_ = value;
  return *this;
}

LedgerReplay &LedgerReplay::fundingBehavior(FundingBehavior value) noexcept {
  ordering_.funding = value;
  return *this;
}

LedgerReplay::Candidate
LedgerReplay::preFraud(const ::PhantomLedger::clearing::Ledger &initialBook,
                       ::PhantomLedger::random::Rng &rng,
                       std::vector<Transaction> sorted) const {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, ordering_.funding,
      /*emitLiquidityEvents=*/true);

  accumulator.extend(std::move(sorted), /*presorted=*/true);

  Candidate out;
  out.txns = accumulator.takeTxns();
  out.drops.byReason = accumulator.dropCounts();
  out.drops.byChannel = accumulator.dropCountsByChannel();

  return out;
}

LedgerReplay::Posted
LedgerReplay::postFraud(::PhantomLedger::random::Rng &rng,
                        const ::PhantomLedger::clearing::Ledger &initialBook,
                        std::vector<Transaction> merged) const {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, ordering_.funding,
      /*emitLiquidityEvents=*/false);

  accumulator.extend(legit_ledger::sortForReplay(std::move(merged)),
                     /*presorted=*/true);

  Posted out;
  out.txns = accumulator.takeTxns();
  out.book = std::move(bookCopy);

  return out;
}

namespace {

void replayChunked(legit_ledger::ChronoReplayAccumulator &accumulator,
                   const std::vector<LedgerReplay::Transaction> &rows,
                   const ::PhantomLedger::pipeline::chunk::Schedule &schedule,
                   std::vector<LedgerReplay::Transaction> &out) {
  using Txn = LedgerReplay::Transaction;
  const auto tsBefore = [](const Txn &t, std::int64_t s) noexcept {
    return t.timestamp < s;
  };

  out.reserve(rows.size());
  std::size_t begin = static_cast<std::size_t>(
      std::lower_bound(rows.begin(), rows.end(),
                       time::toEpochSeconds(schedule.totalWindow().start),
                       tsBefore) -
      rows.begin());
  const std::size_t spanCount = schedule.size();

  for (std::size_t k = 0; k < spanCount; ++k) {
    const auto &span = schedule[k];
    const bool last = (k + 1 == spanCount);

    const auto bound = time::toEpochSeconds(span.activeWindow.endExcl());
    const auto end = static_cast<std::size_t>(
        std::lower_bound(rows.begin() + static_cast<std::ptrdiff_t>(begin),
                         rows.end(), bound, tsBefore) -
        rows.begin());
    std::size_t lookEnd = rows.size();

    if (!last) {
      const auto lookSec = span.lookaheadBoundExcl.time_since_epoch().count();
      lookEnd = static_cast<std::size_t>(
          std::lower_bound(rows.begin() + static_cast<std::ptrdiff_t>(end),
                           rows.end(), lookSec, tsBefore) -
          rows.begin());
    }

    accumulator.extendChunk(
        std::span<const Txn>{rows.data() + begin, end - begin},
        std::span<const Txn>{rows.data() + end, lookEnd - end}, bound);

    auto settled = accumulator.takeSettledBefore(bound);
    out.insert(out.end(), std::make_move_iterator(settled.begin()),
               std::make_move_iterator(settled.end()));
    begin = end;
  }
}

} // namespace

LedgerReplay::Candidate LedgerReplay::preFraudChunked(
    const ::PhantomLedger::clearing::Ledger &initialBook,
    ::PhantomLedger::random::Rng &rng, std::vector<Transaction> sorted,
    const ::PhantomLedger::pipeline::chunk::Schedule &schedule) const {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, ordering_.funding,
      /*emitLiquidityEvents=*/true);

  Candidate out;
  replayChunked(accumulator, sorted, schedule, out.txns);
  out.drops.byReason = accumulator.dropCounts();
  out.drops.byChannel = accumulator.dropCountsByChannel();
  return out;
}

LedgerReplay::Posted LedgerReplay::postFraudChunked(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::clearing::Ledger &initialBook,
    std::vector<Transaction> merged,
    const ::PhantomLedger::pipeline::chunk::Schedule &schedule) const {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, ordering_.funding,
      /*emitLiquidityEvents=*/false);

  const auto rows = legit_ledger::sortForReplay(std::move(merged));

  Posted out;
  replayChunked(accumulator, rows, schedule, out.txns);
  out.book = std::move(bookCopy);
  return out;
}

LedgerReplay::Posted LedgerReplay::postFraudChunkedMerged(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::clearing::Ledger &initialBook,
    std::vector<Transaction> candidatesSorted, std::vector<Transaction> fraud,
    const ::PhantomLedger::pipeline::chunk::Schedule &schedule) const {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, ordering_.funding,
      /*emitLiquidityEvents=*/false);

  const auto &cand = candidatesSorted; // replay-sorted (preFraud output)
  const auto fr = legit_ledger::sortForReplay(std::move(fraud));

  const auto tsBefore = [](const Transaction &t, std::int64_t s) noexcept {
    return t.timestamp < s;
  };
  const auto sliceTo = [&tsBefore](const std::vector<Transaction> &rows,
                                   std::size_t from, std::int64_t sec) {
    return static_cast<std::size_t>(
        std::lower_bound(rows.begin() + static_cast<std::ptrdiff_t>(from),
                         rows.end(), sec, tsBefore) -
        rows.begin());
  };

  Posted out;
  out.txns.reserve(cand.size() + fr.size());

  std::vector<Transaction> slice;
  std::vector<Transaction> look;
  const auto windowStart = time::toEpochSeconds(schedule.totalWindow().start);
  std::size_t cb = sliceTo(cand, 0, windowStart);
  std::size_t fb = sliceTo(fr, 0, windowStart);
  const std::size_t spanCount = schedule.size();

  for (std::size_t k = 0; k < spanCount; ++k) {
    const auto &span = schedule[k];
    const bool last = (k + 1 == spanCount);

    const auto bound = time::toEpochSeconds(span.activeWindow.endExcl());
    const auto cEnd = sliceTo(cand, cb, bound);
    const auto fEnd = sliceTo(fr, fb, bound);
    std::size_t cLook = cand.size();
    std::size_t fLook = fr.size();

    if (!last) {
      const auto lookSec = span.lookaheadBoundExcl.time_since_epoch().count();
      cLook = sliceTo(cand, cEnd, lookSec);
      fLook = sliceTo(fr, fEnd, lookSec);
    }

    slice.clear();
    slice.reserve((cEnd - cb) + (fEnd - fb));
    std::merge(cand.begin() + static_cast<std::ptrdiff_t>(cb),
               cand.begin() + static_cast<std::ptrdiff_t>(cEnd),
               fr.begin() + static_cast<std::ptrdiff_t>(fb),
               fr.begin() + static_cast<std::ptrdiff_t>(fEnd),
               std::back_inserter(slice), legit_ledger::detail::fundsLess);

    // Cure discovery only indexes the lookahead; order is irrelevant.
    look.clear();
    look.reserve((cLook - cEnd) + (fLook - fEnd));
    look.insert(look.end(), cand.begin() + static_cast<std::ptrdiff_t>(cEnd),
                cand.begin() + static_cast<std::ptrdiff_t>(cLook));
    look.insert(look.end(), fr.begin() + static_cast<std::ptrdiff_t>(fEnd),
                fr.begin() + static_cast<std::ptrdiff_t>(fLook));

    accumulator.extendChunk(
        std::span<const Transaction>{slice.data(), slice.size()},
        std::span<const Transaction>{look.data(), look.size()}, bound);

    auto settled = accumulator.takeSettledBefore(bound);
    out.txns.insert(out.txns.end(), std::make_move_iterator(settled.begin()),
                    std::make_move_iterator(settled.end()));
    cb = cEnd;
    fb = fEnd;
  }

  out.book = std::move(bookCopy);
  /* One accumulator serves every span, so this is the whole run's declined
   * population in replay-decision order. */
  out.declined = accumulator.takeDeclined();
  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
