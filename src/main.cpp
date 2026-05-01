#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/run/options.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;

struct CliArgs {
  pl::run::UseCase usecase = pl::run::UseCase::standard;
  std::int64_t days = 365;
  std::int32_t population = 10'000;
  std::uint64_t seed = 0xDEADBEEFULL;
  std::filesystem::path outDir = "out_bank_data";
  bool showTransactions = false;
  bool progress = false;
  bool mlOnly = false;
  pl::time::CalendarDate startDate{2025, 1, 1};
};

void printUsage(const char *prog) {
  std::fprintf(stderr,
               "PhantomLedger — synthetic bank transaction generator\n"
               "\n"
               "Usage: %s [options]\n"
               "\n"
               "Options:\n"
               "  --usecase {standard,mule-ml,aml}  Exporter to run "
               "(default: standard)\n"
               "  --days N                          Simulation length in days "
               "(default: 365)\n"
               "  --population N                    Total population "
               "(default: 10000)\n"
               "  --seed N                          Top-level RNG seed "
               "(default: 0xDEADBEEF)\n"
               "  --out PATH                        Output directory "
               "(default: out_bank_data)\n"
               "  --start YYYY-MM-DD                Simulation start date "
               "(default: 2025-01-01)\n"
               "  --show-transactions               Emit raw "
               "transactions.csv\n"
               "  --progress                        Show progress (currently "
               "no-op)\n"
               "  --ml-only                         For mule-ml: skip standard "
               "tables\n"
               "  --help                            Show this message\n",
               prog);
}

[[nodiscard]] std::optional<std::int64_t> parseInt(std::string_view s) {
  if (s.empty()) {
    return std::nullopt;
  }
  std::string buf{s};
  char *end = nullptr;
  errno = 0;
  const auto v = std::strtoll(buf.c_str(), &end, 10);
  if (end != buf.c_str() + buf.size() || errno == ERANGE) {
    return std::nullopt;
  }
  return v;
}

[[nodiscard]] std::optional<std::uint64_t> parseU64(std::string_view s) {
  if (s.empty()) {
    return std::nullopt;
  }
  std::string buf{s};
  char *end = nullptr;
  errno = 0;
  const auto v = std::strtoull(buf.c_str(), &end, 0);
  if (end != buf.c_str() + buf.size() || errno == ERANGE) {
    return std::nullopt;
  }
  return v;
}

[[nodiscard]] std::optional<pl::time::CalendarDate>
parseDate(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
    return std::nullopt;
  }
  const auto y = parseInt(s.substr(0, 4));
  const auto m = parseInt(s.substr(5, 2));
  const auto d = parseInt(s.substr(8, 2));
  if (!y || !m || !d || *y < 1 || *m < 1 || *m > 12 || *d < 1 || *d > 31) {
    return std::nullopt;
  }
  pl::time::CalendarDate out;
  out.year = static_cast<int>(*y);
  out.month = static_cast<unsigned>(*m);
  out.day = static_cast<unsigned>(*d);
  return out;
}

[[nodiscard]] CliArgs parseArgs(int argc, char **argv) {
  CliArgs args;

  const auto die = [&](const std::string &msg) -> void {
    std::fprintf(stderr, "%s\n\n", msg.c_str());
    printUsage(argv[0]);
    std::exit(2);
  };

  const auto requireValue = [&](int &i,
                                std::string_view flag) -> std::string_view {
    if (i + 1 >= argc) {
      die("Missing value for " + std::string{flag});
    }
    ++i;
    return std::string_view{argv[i]};
  };

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};

    if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      std::exit(0);
    } else if (arg == "--usecase") {
      const auto value = requireValue(i, arg);
      const auto parsed = pl::run::parseUseCase(value);
      if (!parsed) {
        die("Unknown --usecase value: " + std::string{value});
      }
      args.usecase = *parsed;
    } else if (arg == "--days") {
      const auto value = requireValue(i, arg);
      const auto parsed = parseInt(value);
      if (!parsed || *parsed < 1) {
        die("--days must be a positive integer (got " + std::string{value} +
            ")");
      }
      args.days = *parsed;
    } else if (arg == "--population") {
      const auto value = requireValue(i, arg);
      const auto parsed = parseInt(value);
      if (!parsed || *parsed < 1) {
        die("--population must be a positive integer (got " +
            std::string{value} + ")");
      }
      args.population = static_cast<std::int32_t>(*parsed);
    } else if (arg == "--seed") {
      const auto value = requireValue(i, arg);
      const auto parsed = parseU64(value);
      if (!parsed) {
        die("--seed must be a non-negative integer (got " + std::string{value} +
            ")");
      }
      args.seed = *parsed;
    } else if (arg == "--out") {
      const auto value = requireValue(i, arg);
      args.outDir = std::filesystem::path{value};
    } else if (arg == "--start") {
      const auto value = requireValue(i, arg);
      const auto parsed = parseDate(value);
      if (!parsed) {
        die("--start must be YYYY-MM-DD (got " + std::string{value} + ")");
      }
      args.startDate = *parsed;
    } else if (arg == "--show-transactions") {
      args.showTransactions = true;
    } else if (arg == "--progress") {
      args.progress = true;
    } else if (arg == "--ml-only") {
      args.mlOnly = true;
    } else {
      die("Unknown argument: " + std::string{arg});
    }
  }

  return args;
}

[[nodiscard]] pl::entities::synth::pii::PoolSet
buildDefaultPoolSet(std::uint64_t seed) {
  pl::entities::synth::pii::PoolSet poolSet;
  pl::entities::synth::pii::PoolSizes sizes;
  const auto derived = static_cast<std::uint32_t>(seed ^ 0xA5A5A5A5ULL);
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::entities::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                                derived);
  return poolSet;
}

void runStandardExport(const pl::pipeline::SimulationResult &result,
                       const CliArgs &args) {
  pl::exporter::standard::Options opts;
  opts.showTransactions = args.showTransactions;
  pl::exporter::standard::exportAll(result, args.outDir, opts);
}

void runMuleMlExport(const pl::pipeline::SimulationResult &result,
                     const CliArgs &args,
                     const pl::entities::synth::pii::PoolSet *piiPools) {
  pl::exporter::mule_ml::Options opts;
  opts.showTransactions = args.showTransactions;
  opts.includeStandardExport = !args.mlOnly;
  opts.piiPools = piiPools;
  pl::exporter::mule_ml::exportAll(result, args.outDir, opts);
}

[[nodiscard]] pl::exporter::aml::Summary
runAmlExport(const pl::pipeline::SimulationResult &result,
             const CliArgs &args) {
  pl::exporter::aml::Options opts;
  opts.showTransactions = args.showTransactions;
  return pl::exporter::aml::exportAll(result, args.outDir, opts);
}

void printGenericSummary(const pl::pipeline::SimulationResult &result,
                         const CliArgs &args) {
  const auto totalTxns = result.transfers.finalTxns.size();
  std::size_t illicit = 0;
  for (const auto &tx : result.transfers.finalTxns) {
    if (tx.fraud.flag != 0) {
      ++illicit;
    }
  }
  const auto ratio =
      totalTxns == 0
          ? 0.0
          : (static_cast<double>(illicit) / static_cast<double>(totalTxns));

  std::printf("People: %u  Accounts: %zu\n",
              static_cast<unsigned>(result.entities.people.roster.count),
              result.entities.accounts.registry.records.size());
  std::printf("Transactions: %zu  Illicit: %zu (%.4f%%)\n", totalTxns, illicit,
              ratio * 100.0);
  std::printf("Output: %s/\n", args.outDir.string().c_str());
}

void printAmlSummary(const pl::exporter::aml::Summary &summary,
                     const CliArgs &args) {
  const auto ratio = summary.totalTxnCount == 0
                         ? 0.0
                         : (static_cast<double>(summary.illicitTxnCount) /
                            static_cast<double>(summary.totalTxnCount));

  std::printf("AML Export complete -> %s/aml/\n", args.outDir.string().c_str());
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
  try {
    const auto args = parseArgs(argc, argv);

    pl::time::Window window;
    window.start = pl::time::makeTime(args.startDate);
    window.days = static_cast<int>(args.days);

    const auto poolSet = buildDefaultPoolSet(args.seed);

    pl::pipeline::SimulateInputs in;
    in.window = window;
    in.seed = args.seed;
    in.entitiesIn.population = args.population;
    in.entitiesIn.window = window;
    in.entitiesIn.simStart = window.start;
    in.entitiesIn.piiPools = &poolSet;
    in.infraIn.window = window;

    auto rng = pl::random::Rng::fromSeed(args.seed);
    const auto result = pl::pipeline::simulate(rng, in);

    switch (args.usecase) {
    case pl::run::UseCase::standard:
      runStandardExport(result, args);
      printGenericSummary(result, args);
      break;
    case pl::run::UseCase::muleMl:
      runMuleMlExport(result, args, &poolSet);
      printGenericSummary(result, args);
      break;
    case pl::run::UseCase::aml: {
      const auto summary = runAmlExport(result, args);
      printAmlSummary(summary, args);
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
