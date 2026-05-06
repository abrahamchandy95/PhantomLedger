#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

class SimulationPipeline {
public:
  using EntitySynthesis =
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis;
  using InfraStage = ::PhantomLedger::pipeline::stages::infra::AccessInfraStage;
  using ProductSynthesis =
      ::PhantomLedger::pipeline::stages::products::ObligationSynthesis;
  using TransferStage =
      ::PhantomLedger::pipeline::stages::transfers::TransferStage;

  SimulationPipeline(::PhantomLedger::random::Rng &rng,
                     ::PhantomLedger::time::Window window,
                     EntitySynthesis entities, std::uint64_t seed = 0);

  SimulationPipeline(::PhantomLedger::random::Rng &rng,
                     ::PhantomLedger::time::Window window,
                     EntitySynthesis entities, ProductSynthesis products,
                     std::uint64_t seed = 0);

  SimulationPipeline(const SimulationPipeline &) = delete;
  SimulationPipeline &operator=(const SimulationPipeline &) = delete;
  SimulationPipeline(SimulationPipeline &&) noexcept = delete;
  SimulationPipeline &operator=(SimulationPipeline &&) noexcept = delete;

  [[nodiscard]] InfraStage &infraStage() noexcept;
  [[nodiscard]] const InfraStage &infraStage() const noexcept;

  [[nodiscard]] ProductSynthesis &products() noexcept;
  [[nodiscard]] const ProductSynthesis &products() const noexcept;

  [[nodiscard]] TransferStage &transferStage() noexcept;
  [[nodiscard]] const TransferStage &transferStage() const noexcept;

  [[nodiscard]] SimulationResult run() const;

private:
  ::PhantomLedger::random::Rng *rng_ = nullptr;
  ::PhantomLedger::time::Window window_{};
  std::uint64_t seed_ = 0;

  EntitySynthesis entities_;
  ProductSynthesis products_{};

  InfraStage infra_{};
  TransferStage transfers_{};
};

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         SimulationPipeline::EntitySynthesis entities, std::uint64_t seed = 0);

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         SimulationPipeline::EntitySynthesis entities,
         SimulationPipeline::ProductSynthesis products, std::uint64_t seed = 0);

} // namespace PhantomLedger::pipeline
