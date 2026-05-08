#include "phantomledger/cli/args.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/run/options.hpp"
#include "phantomledger/run/setup.hpp"

#include <cstddef>
#include <cstdio>
#include <exception>

namespace {

namespace pl = ::PhantomLedger;

void printGenericSummary(const pl::pipeline::SimulationResult &result,
                         const pl::run::RunOptions &opts) {
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
              static_cast<unsigned>(result.entities.people.roster.count),
              result.entities.accounts.registry.records.size());
  std::printf("Transactions: %zu  Illicit: %zu (%.4f%%)\n", totalTxns, illicit,
              ratio * 100.0);
  std::printf("Output: %s/\n", opts.outDir.string().c_str());
}

void printAmlSummary(const pl::exporter::aml::Summary &summary,
                     const pl::run::RunOptions &opts) {

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

} // namespace

int main(int argc, char **argv) {
  using namespace ::PhantomLedger;
  namespace pii = entities::synth::pii;

  try {

    const auto opts = cli::parse(argc, argv);

    time::Window window;
    window.start = time::makeTime(opts.startDate);
    window.days = static_cast<int>(opts.days);

    const auto mix = pii::LocaleMix::usBankDefault();
    const auto pools = run::setup::buildPoolSet(opts, mix);
    const auto entityConfig =
        run::setup::buildEntitySynthesis(opts, pools, mix, window.start);

    auto rng = random::Rng::fromSeed(opts.seed);
    const auto result =
        pipeline::simulate(rng, window, entityConfig, opts.seed);

    switch (opts.usecase) {
    case run::UseCase::standard: {
      exporter::standard::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exporter::standard::exportAll(result, opts.outDir, exportOpts);
      printGenericSummary(result, opts);
      break;
    }

    case run::UseCase::muleMl: {
      exporter::mule_ml::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      exportOpts.piiPools = &pools;
      exporter::mule_ml::exportAll(result, opts.outDir, exportOpts);
      printGenericSummary(result, opts);
      break;
    }

    case run::UseCase::aml: {
      exporter::aml::Options exportOpts;
      exportOpts.showTransactions = opts.showTransactions;
      const auto summary =
          exporter::aml::exportAll(result, opts.outDir, exportOpts);
      printAmlSummary(summary, opts);
      break;
    }
    }

    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "fatal: unknown exception\n");
    return 1;
  }
}
