#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <span>

namespace PhantomLedger::transfers::fraud {

class Injector {
public:
  struct Rules {
    TypologyWeights typology{};
    LayeringRules layering{};
    StructuringRules structuring{};
    CamouflageRates camouflage{};

    void validate() const {
      typology.validate();
      layering.validate();
      structuring.validate();
      camouflage.validate();
    }
  };

  struct Services {
    random::Rng *rng = nullptr;
    const infra::Router *router = nullptr;
    const infra::SharedInfra *ringInfra = nullptr;
  };

  struct FraudPopulation {
    const entities::synth::people::Fraud *profile = nullptr;
    time::Window window{};
    const entity::person::Topology *topology = nullptr;
    const entity::account::Registry *accounts = nullptr;
    const entity::account::Ownership *ownership = nullptr;
    std::span<const transactions::Transaction> baseTxns{};
  };

  struct LegitCounterparties {
    std::span<const entity::Key> billerAccounts{};
    std::span<const entity::Key> employers{};
  };

  explicit Injector(Services services) noexcept;
  Injector(Services services, Rules rules) noexcept;

  [[nodiscard]] InjectionOutput inject(const FraudPopulation &population) const;
  [[nodiscard]] InjectionOutput
  inject(const FraudPopulation &population,
         LegitCounterparties counterparties) const;

private:
  Services services_{};
  Rules rules_{};
};

} // namespace PhantomLedger::transfers::fraud
