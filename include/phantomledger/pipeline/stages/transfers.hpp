#pragma once

#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/state.hpp"
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

  [[nodiscard]] ::PhantomLedger::pipeline::Transfers
  build(::PhantomLedger::random::Rng &rng,
        const ::PhantomLedger::pipeline::Entities &entities,
        const ::PhantomLedger::pipeline::Infra &infra) const;

private:
  legit::LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
