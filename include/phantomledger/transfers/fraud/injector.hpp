#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud {

class Injector {
public:
  struct Services {
    random::Rng &rng;
    const infra::Router *router = nullptr;
    const infra::SharedInfra *ringInfra = nullptr;
  };

  struct RingView {
    const entities::synth::people::Fraud *profile = nullptr;
    const entity::person::Topology *topology = nullptr;
  };

  struct AccountView {
    const entity::account::Registry *registry = nullptr;
    const entity::account::Ownership *ownership = nullptr;
  };

  struct LegitCounterparties {
    std::span<const entity::Key> billerAccounts{};
    std::span<const entity::Key> employers{};
  };

  Injector(Services services, RingView rings, AccountView accounts) noexcept;

  // Per-component setters. Each typology / camouflage layer owns its
  // own rules; setting one is independent of the others.
  Injector &typologyWeights(TypologyWeights value) noexcept;
  Injector &layeringRules(typologies::layering::Rules value) noexcept;
  Injector &structuringRules(typologies::structuring::Rules value) noexcept;
  Injector &camouflageRates(camouflage::Rates value) noexcept;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns,
         LegitCounterparties counterparties) const;

private:
  /// Dispatch one ring through the typology selected for it. Owns the
  /// switch over `Typology`; pulls per-typology rules from members so
  /// the dispatch surface stays narrow.
  [[nodiscard]] std::vector<transactions::Transaction>
  generateForTypology(IllicitContext &ctx, const Plan &plan, Typology typology,
                      std::int32_t budget) const;

  Services services_;
  RingView rings_{};
  AccountView accounts_{};

  TypologyWeights typologyWeights_{};
  typologies::layering::Rules layeringRules_{};
  typologies::structuring::Rules structuringRules_{};
  camouflage::Rates camouflageRates_{};
};

} // namespace PhantomLedger::transfers::fraud
