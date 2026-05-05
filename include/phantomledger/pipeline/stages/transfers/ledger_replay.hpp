#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class LedgerReplay {
public:
  using Transaction = ::PhantomLedger::transactions::Transaction;
  using ChannelReasonKey =
      ::PhantomLedger::pipeline::Transfers::ChannelReasonKey;
  using ChannelReasonHash =
      ::PhantomLedger::pipeline::Transfers::ChannelReasonHash;

  struct Ordering {
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        replayRules{};
  };

  struct PreFraud {
    std::vector<Transaction> draftTxns;
    std::unordered_map<std::string, std::uint32_t> dropCounts;
    std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash>
        dropCountsByChannel;
  };

  struct PostFraud {
    std::vector<Transaction> finalTxns;
    std::unique_ptr<::PhantomLedger::clearing::Ledger> finalBook;
  };

  LedgerReplay() = default;

  LedgerReplay &ordering(Ordering value) noexcept;
  LedgerReplay &rules(
      ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
          value) noexcept;

  [[nodiscard]] PreFraud
  preFraud(const ::PhantomLedger::clearing::Ledger &initialBook,
           ::PhantomLedger::random::Rng &rng,
           std::vector<Transaction> sorted) const;

  [[nodiscard]] PostFraud
  postFraud(::PhantomLedger::random::Rng &rng,
            const ::PhantomLedger::clearing::Ledger &initialBook,
            std::vector<Transaction> merged) const;

private:
  Ordering ordering_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
