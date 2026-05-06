#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"

#include "phantomledger/transfers/legit/ledger/streams.hpp"

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

  auto sorted = legit_ledger::sortForReplay(
      std::span<const Transaction>{merged.data(), merged.size()});
  accumulator.extend(std::move(sorted), /*presorted=*/true);

  Posted out;
  out.txns = accumulator.takeTxns();
  out.book = std::move(bookCopy);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
