#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace PhantomLedger::pipeline {

struct ReplayDrops {
  ::PhantomLedger::transfers::legit::ledger::ReplayDropLedger::Counts byReason;
  ::PhantomLedger::transfers::legit::ledger::ReplayDropLedger::CountsByChannel
      byChannel;
};

struct CandidateLedgerReplay {
  std::vector<transactions::Transaction> txns;
  ReplayDrops drops;
};

struct PostedLedgerReplay {
  std::vector<transactions::Transaction> txns;
  std::unique_ptr<clearing::Ledger> book;
};

struct TransferLedger {
  CandidateLedgerReplay candidate;
  PostedLedgerReplay posted;
};

struct FraudInjectionSummary {
  std::size_t injectedCount = 0;
};

struct Transfers {
  transfers::legit::ledger::LegitTransferResult legit;
  FraudInjectionSummary fraud;
  TransferLedger ledger;
};

} // namespace PhantomLedger::pipeline
