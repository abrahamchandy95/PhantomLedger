#pragma once

#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml_txn_edges/export.hpp"
#include "phantomledger/exporter/card_fraud/export.hpp"
#include "phantomledger/pipeline/stages/transfers/orchestrator.hpp"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::app::reporting {

// Wall-clock and peak-RSS trace of the run's phases, on stderr
class PhaseMonitor {
public:
  PhaseMonitor();

  void mark(std::string_view label);

private:
  using Clock = std::chrono::steady_clock;
  Clock::time_point start_;
  Clock::time_point last_;
};

// Takes the world-scale counts BY VALUE
void windowedSummary(
    std::uint32_t peopleCount, std::size_t accountCount,
    const pipeline::stages::transfers::WindowedRunResult &transfers,
    std::uint64_t streamRows);

void amlSummary(const exporter::aml::Summary &summary);

void amlTxnEdgesSummary(const exporter::aml_txn_edges::Summary &summary);

void cardFraudSummary(const exporter::card_fraud::Summary &summary);

} // namespace PhantomLedger::app::reporting
