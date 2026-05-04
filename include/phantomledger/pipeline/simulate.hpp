#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

class SimulationPipeline {
public:
  SimulationPipeline(
      ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
      std::uint64_t seed = 0);

  SimulationPipeline(const SimulationPipeline &) = delete;
  SimulationPipeline &operator=(const SimulationPipeline &) = delete;
  SimulationPipeline(SimulationPipeline &&) noexcept = delete;
  SimulationPipeline &operator=(SimulationPipeline &&) noexcept = delete;

  SimulationPipeline &infraWindow(::PhantomLedger::time::Window value) noexcept;
  SimulationPipeline &ringBehavior(
      const ::PhantomLedger::infra::synth::rings::Config &value) noexcept;
  SimulationPipeline &deviceBehavior(
      const ::PhantomLedger::infra::synth::devices::Config &value) noexcept;
  SimulationPipeline &
  ipBehavior(const ::PhantomLedger::infra::synth::ips::Config &value) noexcept;
  SimulationPipeline &routingBehavior(
      ::PhantomLedger::pipeline::stages::infra::RoutingBehavior value) noexcept;
  SimulationPipeline &sharedInfra(
      ::PhantomLedger::pipeline::stages::infra::SharedInfraUse value) noexcept;

  SimulationPipeline &transferScope(
      ::PhantomLedger::pipeline::stages::transfers::RunScope value) noexcept;
  SimulationPipeline &recurringIncome(
      const ::PhantomLedger::pipeline::stages::transfers::RecurringIncome
          &value);
  SimulationPipeline &
  balanceBook(::PhantomLedger::pipeline::stages::transfers::BalanceBookRules
                  value) noexcept;
  SimulationPipeline &
  creditCards(::PhantomLedger::pipeline::stages::transfers::CreditCardLifecycle
                  value) noexcept;
  SimulationPipeline &
  family(::PhantomLedger::pipeline::stages::transfers::FamilyPrograms
             value) noexcept;
  SimulationPipeline &government(
      const ::PhantomLedger::pipeline::stages::transfers::GovernmentPrograms
          &value);
  SimulationPipeline &
  insurance(::PhantomLedger::pipeline::stages::transfers::InsuranceClaims
                value) noexcept;
  SimulationPipeline &
  replay(::PhantomLedger::pipeline::stages::transfers::LedgerReplay
             value) noexcept;
  SimulationPipeline &
  fraud(::PhantomLedger::pipeline::stages::transfers::FraudInjection
            value) noexcept;
  SimulationPipeline &
  population(::PhantomLedger::pipeline::stages::transfers::PopulationShape
                 value) noexcept;

  [[nodiscard]] SimulationResult run() const;

private:
  ::PhantomLedger::random::Rng *rng_ = nullptr;
  ::PhantomLedger::time::Window window_{};
  std::uint64_t seed_ = 0;

  ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities_;

  ::PhantomLedger::pipeline::stages::infra::AccessInfraStage infra_{};

  ::PhantomLedger::pipeline::stages::transfers::RunScope transferScope_{};
  ::PhantomLedger::pipeline::stages::transfers::RecurringIncome
      recurringIncome_{};
  ::PhantomLedger::pipeline::stages::transfers::BalanceBookRules balanceBook_{};
  ::PhantomLedger::pipeline::stages::transfers::CreditCardLifecycle
      creditCards_{};
  ::PhantomLedger::pipeline::stages::transfers::FamilyPrograms family_{};
  ::PhantomLedger::pipeline::stages::transfers::GovernmentPrograms
      government_{};
  ::PhantomLedger::pipeline::stages::transfers::InsuranceClaims insurance_{};
  ::PhantomLedger::pipeline::stages::transfers::LedgerReplay replay_{};
  ::PhantomLedger::pipeline::stages::transfers::FraudInjection fraud_{};
  ::PhantomLedger::pipeline::stages::transfers::PopulationShape population_{};
};

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
         std::uint64_t seed = 0);

} // namespace PhantomLedger::pipeline
