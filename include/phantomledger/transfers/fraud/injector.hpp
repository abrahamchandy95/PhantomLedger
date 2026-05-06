#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <span>

namespace PhantomLedger::transfers::fraud {

class Injector {
public:
  struct Patterns {
    TypologyWeights typology{};
    LayeringRules layering{};
    StructuringRules structuring{};
    CamouflageRates camouflage{};

    void validate(primitives::validate::Report &r) const {
      typology.validate(r);
      layering.validate(r);
      structuring.validate(r);
      camouflage.validate(r);
    }

    void validate() const { primitives::validate::require(*this); }
  };

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

  explicit Injector(Services services) noexcept;
  Injector(Services services, Patterns patterns) noexcept;

  [[nodiscard]] InjectionOutput
  inject(RingView rings, AccountView accounts, time::Window window,
         std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] InjectionOutput
  inject(RingView rings, AccountView accounts, time::Window window,
         std::span<const transactions::Transaction> baseTxns,
         LegitCounterparties counterparties) const;

private:
  Services services_;
  Patterns patterns_{};
};

} // namespace PhantomLedger::transfers::fraud
