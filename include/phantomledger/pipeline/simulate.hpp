#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/pipeline/stages/transfers/orchestrator.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <functional>
#include <string_view>

namespace PhantomLedger::pipeline {

using PhaseObserver = std::function<void(std::string_view phase)>;

// Result of a windowed run: the built world (entities, infra; the
// transfers member of `world` stays empty) plus the transfer-fold summary.
// The posted corpus is NOT retained — it streamed to the caller's sink.
struct WindowedSimulationResult {
  SimulationResult world;
  ::PhantomLedger::pipeline::stages::transfers::WindowedRunResult transfers;
};

class SimulationPipeline {
public:
  using EntitySynthesis =
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis;
  using InfraStage = ::PhantomLedger::pipeline::stages::infra::AccessInfraStage;
  using ProductSynthesis =
      ::PhantomLedger::pipeline::stages::products::ObligationSynthesis;
  using TransferStage =
      ::PhantomLedger::pipeline::stages::transfers::TransferStage;
  using WindowedRunOptions =
      ::PhantomLedger::pipeline::stages::transfers::WindowedRunOptions;

  SimulationPipeline(::PhantomLedger::random::Rng &rng,
                     ::PhantomLedger::time::Window window,
                     EntitySynthesis entities, std::uint64_t seed = 0);

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

  [[nodiscard]] SimulationResult run(const PhaseObserver &onPhase = {}) const;

  // World build shared by run() and the windowed path: entities, products,
  // infra, consuming the shared sequential stream in the exact same order.
  // The transfers member of the returned result stays empty. Callers that
  // need the world BEFORE the transfer fold (e.g. to bind a streaming
  // exporter to the account registry) pair this with
  // runWindowedTransfers().
  [[nodiscard]] SimulationResult
  buildWorld(const PhaseObserver &onPhase = {}) const;

  // Windowed transfer fold over a world previously built by buildWorld()
  // on the SAME pipeline (the shared RNG has advanced accordingly). Rows
  // stream to `sink`.
  template <class S>
  [[nodiscard]] ::PhantomLedger::pipeline::stages::transfers::WindowedRunResult
  runWindowedTransfers(SimulationResult &world, S &sink,
                       const WindowedRunOptions &options = {},
                       const PhaseObserver &onPhase = {}) const {
    ::PhantomLedger::pipeline::stages::transfers::SinkRef ref(sink);
    return runWindowedTransfersErased(world, ref, options, onPhase);
  }

  [[nodiscard]] ::PhantomLedger::pipeline::stages::transfers::WindowedRunResult
  runWindowedTransfersErased(
      SimulationResult &world,
      ::PhantomLedger::pipeline::stages::transfers::SinkRef sink,
      const WindowedRunOptions &options, const PhaseObserver &onPhase) const;

  // Windowed two-phase run: identical world build and deterministic
  // regime as run(), but transfers fold through the bounded-memory
  // windowed driver and STREAM to `sink` (SinkRef-compatible: beginSpan/
  // append/endSpan/finish/rowsWritten) instead of accumulating a posted
  // corpus. Byte-identical output to run() (test_production_windowed).
  // Equivalent to buildWorld() followed by runWindowedTransfers().
  template <class S>
  [[nodiscard]] WindowedSimulationResult
  runWindowed(S &sink, const WindowedRunOptions &options = {},
              const PhaseObserver &onPhase = {}) const {
    ::PhantomLedger::pipeline::stages::transfers::SinkRef ref(sink);
    return runWindowedErased(ref, options, onPhase);
  }

  [[nodiscard]] WindowedSimulationResult runWindowedErased(
      ::PhantomLedger::pipeline::stages::transfers::SinkRef sink,
      const WindowedRunOptions &options, const PhaseObserver &onPhase) const;

private:
  void buildEntities(SimulationResult &result) const;

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
         SimulationPipeline::EntitySynthesis entities, std::uint64_t seed = 0,
         const PhaseObserver &onPhase = {});

} // namespace PhantomLedger::pipeline
