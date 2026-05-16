#pragma once

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/pipeline/result.hpp"

#include <cstddef>
#include <filesystem>

namespace PhantomLedger::exporter::aml_txn_edges {

using Options = ::PhantomLedger::exporter::common::ExportOptions;

struct Summary : ::PhantomLedger::exporter::common::BaseSummary {
  std::size_t alertCount = 0;
  std::size_t ctrCount = 0;
  std::size_t caseCount = 0;
  std::size_t businessCount = 0;
  std::size_t flowAggEdgeCount = 0;
  std::size_t linkCommEdgeCount = 0;
  std::size_t chainCount = 0;
  std::size_t shellCount = 0;
};

[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const std::filesystem::path &outDir, const Options &options = {});

} // namespace PhantomLedger::exporter::aml_txn_edges
