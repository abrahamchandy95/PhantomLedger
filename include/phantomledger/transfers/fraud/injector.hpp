#pragma once

#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/injector_inputs.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud {

class Injector {
public:
  Injector(InjectorServices services, InjectorRingView rings,
           InjectorAccountView accounts, const Behavior &behavior) noexcept;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns,
         InjectorLegitCounterparties counterparties) const;

private:
  [[nodiscard]] std::vector<transactions::Transaction>
  generateForTypology(IllicitContext &ctx, const Plan &plan, Typology typology,
                      std::int32_t budget) const;

  InjectorServices services_;
  InjectorRingView rings_{};
  InjectorAccountView accounts_{};
  const Behavior &behavior_;
};

} // namespace PhantomLedger::transfers::fraud
