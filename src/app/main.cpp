#include "phantomledger/app/cli.hpp"
#include "phantomledger/app/options.hpp"
#include "phantomledger/app/progress.hpp"
#include "phantomledger/app/setup.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml_txn_edges/export.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <sys/resource.h>

namespace {

namespace pl = ::PhantomLedger;

double peakRssMB() {
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(ru.ru_maxrss) / 1024.0;
#endif
}

class PhaseMonitor {
public:
  PhaseMonitor() : start_(Clock::now()), last_(start_) {}

  void mark(std::string_view label) {
    const auto now = Clock::now();
    const auto deltaMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_)
            .count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
            .count();
    std::fprintf(
        stderr, "[phase] %-24s  +%9.1fs  (total %9.1fs)  peakRSS=%9.1f MB\n",
        std::string{label}.c_str(), static_cast<double>(deltaMs) / 1000.0,
        static_cast<double>(totalMs) / 1000.0, peakRssMB());
    last_ = now;
  }

private:
  using Clock = std::chrono::steady_clock;
  Clock::time_point start_;
  Clock::time_point last_;
};

void printGenericSummary(const pl::pipeline::SimulationResult &result,
                         const pl::app::RunOptions &opts) {
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto totalTxns = postedTxns.size();

  std::size_t illicit = 0;
  for (const auto &tx : postedTxns) {
    if (tx.fraud.flag != 0) {
      ++illicit;
    }
  }

  const double ratio =
      (totalTxns == 0) ? 0.0 : static_cast<double>(illicit) / totalTxns;

  std::printf("People: %u  Accounts: %zu\n",
              static_cast<unsigned>(result.people.roster.roster.count),
              result.holdings.accounts.registry.records.size());
  std::printf("Transactions: %zu  Illicit: %zu (%.4f%%)\n", totalTxns, illicit,
              ratio * 100.0);
  std::printf("Output: %s/\n", opts.outDir.string().c_str());
}

void printAmlSummary(const pl::exporter::aml::Summary &summary,
                     const pl::app::RunOptions &opts) {

  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::printf("AML Export complete -> %s/aml/\n", opts.outDir.string().c_str());
  std::printf("  Customers:       %zu\n", summary.customerCount);
  std::printf("  Accounts:        %zu\n", summary.internalAccountCount);
  std::printf("  Counterparties:  %zu\n", summary.counterpartyCount);
  std::printf("  Transactions:    %zu  (illicit: %zu, %.4f%%)\n",
              summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::printf("  Fraud rings:     %zu\n", summary.fraudRingCount);
  std::printf("  Solo fraudsters: %zu\n", summary.soloFraudCount);
  std::printf("  SARs filed:      %zu\n", summary.sarsFiledCount);
}

void printAmlTxnEdgesSummary(
    const pl::exporter::aml_txn_edges::Summary &summary,
    const pl::app::RunOptions &opts) {

  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::printf("AML (txn-edges) Export complete -> %s/aml_txn_edges/\n",
              opts.outDir.string().c_str());
  std::printf("  Customers:       %zu\n", summary.customerCount);
  std::printf("  Accounts:        %zu\n", summary.internalAccountCount);
  std::printf("  Counterparties:  %zu\n", summary.counterpartyCount);
  std::printf("  Transactions:    %zu  (illicit: %zu, %.4f%%)\n",
              summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::printf("  Fraud rings:     %zu\n", summary.fraudRingCount);
  std::printf("  Solo fraudsters: %zu\n", summary.soloFraudCount);
  std::printf("  SARs filed:      %zu\n", summary.sarsFiledCount);
  std::printf("  Alerts:          %zu  (CTRs: %zu)\n", summary.alertCount,
              summary.ctrCount);
  std::printf("  Cases:           %zu  (businesses: %zu)\n", summary.caseCount,
              summary.businessCount);
  std::printf("  Flow-agg edges:  %zu  (link-comm: %zu)\n",
              summary.flowAggEdgeCount, summary.linkCommEdgeCount);
}

} // namespace

int main(int argc, char **argv) {
  using namespace ::PhantomLedger;
  namespace pii = synth::pii;
  namespace pg = app::progress;

  try {

    const auto opts = app::cli::parse(argc, argv);

    time::Window window;
    window.start = time::makeTime(opts.startDate);
    window.days = static_cast<int>(opts.days);

    pg::status("Building entity synthesis config...");
    const auto mix = pii::LocaleMix::usBankDefault();
    const auto pools = app::setup::buildPoolSet(opts, mix);
    const auto entityConfig =
        app::setup::buildEntitySynthesis(opts, pools, mix, window.start);

    auto rng = random::Rng::fromSeed(opts.seed);

    PhaseMonitor mon;

    pl::pipeline::SimulationResult result;
    {
      pg::Stage genStage("Generating (entities)", 4);
      const auto onPhase = [&](std::string_view phase) {
        mon.mark(phase);
        genStage.tick();
        genStage.setLabel("Generating (" + std::string{phase} + " done)");
      };
      result =
          pipeline::simulate(rng, window, entityConfig, opts.seed, onPhase);
    } // genStage destructor prints trailing newline here

    pg::status("Exporting...");
    switch (opts.usecase) {
    case app::UseCase::standard: {
      exporter::standard::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exportOpts.piiPools = &pools;
      exportOpts.window = window;
      exporter::standard::exportAll(result, opts.outDir, exportOpts);
      printGenericSummary(result, opts);
      break;
    }

    case app::UseCase::muleMl: {
      exporter::mule_ml::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exportOpts.piiPools = &pools;
      exporter::mule_ml::exportAll(result, opts.outDir, exportOpts);
      printGenericSummary(result, opts);
      break;
    }

    case app::UseCase::aml: {
      exporter::aml::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exportOpts.piiPools = &pools;
      const auto summary =
          exporter::aml::exportAll(result, opts.outDir, exportOpts);
      printAmlSummary(summary, opts);
      break;
    }

    case app::UseCase::amlTxnEdges: {
      exporter::aml_txn_edges::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exportOpts.piiPools = &pools;
      const auto summary =
          exporter::aml_txn_edges::exportAll(result, opts.outDir, exportOpts);
      printAmlTxnEdgesSummary(summary, opts);
      break;
    }
    }

    mon.mark("export");

    pg::status("Done.");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "fatal: unknown exception\n");
    return 1;
  }
}
