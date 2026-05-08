#pragma once

#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/legit_assembly.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/random/rng.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

class TransferStage {
public:
  TransferStage() = default;

  [[nodiscard]] LegitAssembly &legit() noexcept;
  [[nodiscard]] const LegitAssembly &legit() const noexcept;

  [[nodiscard]] ProductReplay &products() noexcept;
  [[nodiscard]] const ProductReplay &products() const noexcept;

  [[nodiscard]] LedgerReplay &ledger() noexcept;
  [[nodiscard]] const LedgerReplay &ledger() const noexcept;

  [[nodiscard]] FraudEmission &fraud() noexcept;
  [[nodiscard]] const FraudEmission &fraud() const noexcept;

  [[nodiscard]] ::PhantomLedger::pipeline::Transfers
  build(::PhantomLedger::random::Rng &rng,
        const ::PhantomLedger::pipeline::Entities &entities,
        const ::PhantomLedger::pipeline::Infra &infra) const;

private:
  LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
