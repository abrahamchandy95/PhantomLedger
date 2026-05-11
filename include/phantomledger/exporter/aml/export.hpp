#pragma once

#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/pipeline/result.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::aml {

struct Options {
  bool showTransactions = false;

  const ::PhantomLedger::entities::synth::pii::PoolSet *piiPools = nullptr;
};

/// Summary statistics returned to the caller for printout.
struct Summary {
  std::size_t customerCount = 0;
  std::size_t internalAccountCount = 0;
  std::size_t counterpartyCount = 0;
  std::size_t totalTxnCount = 0;
  std::size_t illicitTxnCount = 0;
  std::size_t fraudRingCount = 0;
  std::size_t soloFraudCount = 0;
  std::size_t sarsFiledCount = 0;
};

[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const std::filesystem::path &outDir, const Options &options = {});

} // namespace PhantomLedger::exporter::aml
