#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

namespace legit = ::PhantomLedger::transfers::legit;
namespace fraud_ns = ::PhantomLedger::transfers::fraud;

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

  TransferStage &infra(const pipeline::Infra &value) noexcept;

  [[nodiscard]] legit::ledger::LegitTransferResult
  buildLegit(random::Rng &rng, const pipeline::People &people,
             const pipeline::Holdings &holdings,
             const pipeline::Counterparties &cps) const;

  [[nodiscard]] std::vector<transactions::Transaction>
  mergeProducts(random::Rng &rng, const pipeline::Holdings &holdings,
                legit::ledger::LegitTxnStreams streams) const;

  [[nodiscard]] LedgerReplay::Candidate
  preFraudReplay(random::Rng &rng, const clearing::Ledger &initialBook,
                 std::vector<transactions::Transaction> sortedStream) const;

  [[nodiscard]] fraud_ns::Injector
  makeFraudInjector(random::Rng &rng, const pipeline::People &people,
                    const pipeline::Holdings &holdings) const;

  [[nodiscard]] LedgerReplay::Posted
  postFraudReplay(random::Rng &rng, const clearing::Ledger &initialBook,
                  std::vector<transactions::Transaction> merged) const;

private:
  legit::LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
  const pipeline::Infra *infra_ = nullptr;
};

} // namespace PhantomLedger::pipeline::stages::transfers
