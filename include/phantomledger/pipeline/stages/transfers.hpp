#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/transfers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transfers/legit/assembly.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

namespace legit = ::PhantomLedger::transfers::legit;

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

  [[nodiscard]] pipeline::Transfers build(random::Rng &rng,
                                          const pipeline::People &people,
                                          const pipeline::Holdings &holdings,
                                          const pipeline::Counterparties &cps,
                                          const pipeline::Infra &infra) const;

private:
  legit::LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
