#pragma once

#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/pipeline/result.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::aml {

using Options = ::PhantomLedger::exporter::common::ExportOptions;

struct Summary : ::PhantomLedger::exporter::common::BaseSummary {};

[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const std::filesystem::path &outDir, const Options &options = {});

} // namespace PhantomLedger::exporter::aml
