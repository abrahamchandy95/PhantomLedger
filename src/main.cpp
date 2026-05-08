#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
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

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

namespace pl = ::PhantomLedger;
namespace pii = pl::entities::synth::pii;

// TODO: make this file light

// TODO: improve cli args - progress always true, explicit usecase only out
struct CliArgs {
  pl::run::UseCase usecase = pl::run::UseCase::standard;
  std::int64_t days = 365;
  std::int32_t population = 70'000;
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
               "(default: 70000)\n"
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

// TODO: lift parseInt / parseU64 / parseDate into a cli/parsers.hpp helper
// module once a second consumer appears
[[nodiscard]] std::optional<std::int64_t> parseInt(std::string_view s) {
  if (s.empty()) {
    return std::nullopt;
  }

  std::int64_t value{};
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<std::uint64_t> parseU64(std::string_view s) {
  if (s.empty()) {
    return std::nullopt;
  }

  int base = 10;
  if (s.starts_with("0x") || s.starts_with("0X")) {
    base = 16;
    s.remove_prefix(2);
  } else if (s.starts_with('0') && s.size() > 1) {
    base = 8;
    s.remove_prefix(1);
  }

  if (s.empty()) {
    return std::nullopt;
  }

  std::uint64_t value{};
  const auto [ptr, ec] =
      std::from_chars(s.data(), s.data() + s.size(), value, base);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<pl::time::CalendarDate>
parseDate(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
    return std::nullopt;
  }

  const auto year = parseInt(s.substr(0, 4));
  const auto month = parseInt(s.substr(5, 2));
  const auto day = parseInt(s.substr(8, 2));

  if (!year || !month || !day || *year < 1 || *month < 1 || *month > 12 ||
      *day < 1 || *day > 31) {
    return std::nullopt;
  }

  return pl::time::CalendarDate{
      .year = static_cast<int>(*year),
      .month = static_cast<unsigned>(*month),
      .day = static_cast<unsigned>(*day),
  };
}

[[nodiscard]] CliArgs parseArgs(int argc, char **argv) {
  CliArgs args;

  auto die = [&]<typename... T>(std::format_string<T...> fmt,
                                T &&...formatArgs) {
    std::println(stderr, fmt, std::forward<T>(formatArgs)...);
    std::println(stderr, "");
    printUsage(argv[0]);
    std::exit(2);
  };

  const auto requireValue = [&](int &i,
                                std::string_view flag) -> std::string_view {
    if (i + 1 >= argc) {
      die("Missing value for {}", flag);
    }
    return argv[++i];
  };

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};

    if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      std::exit(0);
    }

    if (arg == "--usecase") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = pl::run::parseUseCase(value)) {
        args.usecase = *parsed;
      } else {
        die("Unknown --usecase value: {}", value);
      }
      continue;
    }

    if (arg == "--days") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseInt(value); parsed && *parsed > 0) {
        args.days = *parsed;
      } else {
        die("--days must be a positive integer (got {})", value);
      }
      continue;
    }

    if (arg == "--population") {
      const auto value = requireValue(i, arg);
      const auto parsed = parseInt(value);
      if (!parsed || *parsed < 1 ||
          *parsed > std::numeric_limits<std::int32_t>::max()) {
        die("--population must fit in a positive int32 (got {})", value);
      }
      args.population = static_cast<std::int32_t>(*parsed);
      continue;
    }

    if (arg == "--seed") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseU64(value)) {
        args.seed = *parsed;
      } else {
        die("--seed must be a non-negative integer (got {})", value);
      }
      continue;
    }

    if (arg == "--out") {
      args.outDir = requireValue(i, arg);
      continue;
    }

    if (arg == "--start") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseDate(value)) {
        args.startDate = *parsed;
      } else {
        die("--start must be YYYY-MM-DD (got {})", value);
      }
      continue;
    }

    if (arg == "--show-transactions") {
      args.showTransactions = true;
      continue;
    }

    if (arg == "--progress") {
      args.progress = true;
      continue;
    }

    if (arg == "--ml-only") {
      args.mlOnly = true;
      continue;
    }

    die("Unknown argument: {}", arg);
  }

  return args;
}

[[nodiscard]] std::size_t
poolSizeForFraction(double fraction, std::int32_t population) noexcept {
  constexpr std::size_t kPoolFloor = 1'000;
  constexpr std::size_t kPoolCap = 50'000;

  const auto expected = static_cast<std::size_t>(
      std::ceil(static_cast<double>(population) * fraction));
  const std::size_t targetN =
      (expected > kPoolCap / 4) ? kPoolCap : expected * 4;
  return std::max(kPoolFloor, targetN);
}

[[nodiscard]] std::uint32_t derivePoolSeed(std::uint64_t topLevelSeed,
                                           std::size_t countryIdx) noexcept {
  constexpr std::uint64_t kPoolSeedDomain = 0xA5A5A5A5ULL;
  constexpr std::uint64_t kCountryMixer = 0x9E3779B9ULL;
  return static_cast<std::uint32_t>(
      topLevelSeed ^ kPoolSeedDomain ^
      (static_cast<std::uint64_t>(countryIdx) * kCountryMixer));
}

[[nodiscard]] pii::LocalePool generateLocalePool(pl::locale::Country country,
                                                 double fraction,
                                                 std::int32_t population,
                                                 std::uint64_t seed) {
  assert(fraction > 0.0 && "Caller must filter zero-weight countries.");

  const auto idx = pl::taxonomies::enums::toIndex(country);
  const auto poolN = poolSizeForFraction(fraction, population);

  const pii::PoolSizes sizes{
      .firstNames = poolN,
      .middleNames = poolN,
      .lastNames = poolN,
      .streets = poolN,
  };

  return pii::buildLocalePool(country, sizes, derivePoolSeed(seed, idx));
}

/// Build the EntitySynthesis config tree.
///
/// The struct is flat. main provides the user-owned fields explicitly
/// (`population`, `identity`); calibration fields use their
/// research-backed defaults via designated-init omission.
[[nodiscard]] pl::pipeline::stages::entities::EntitySynthesis
createSynthesisConfig(const CliArgs &args, const pii::PoolSet &pools,
                      const pii::LocaleMix &mix, pl::time::TimePoint simStart) {
  return pl::pipeline::stages::entities::EntitySynthesis{
      .population = args.population,
      .identity =
          pl::pipeline::stages::entities::IdentitySource{
              .pools = &pools,
              .simStart = simStart,
              .localeMix = mix,
          },
  };
}

void runStandardExport(const pl::pipeline::SimulationResult &result,
                       const CliArgs &args) {
  pl::exporter::standard::Options opts;
  opts.showTransactions = args.showTransactions;
  pl::exporter::standard::exportAll(result, args.outDir, opts);
}

void runMuleMlExport(const pl::pipeline::SimulationResult &result,
                     const CliArgs &args, const pii::PoolSet *piiPools) {
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
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto totalTxns = postedTxns.size();

  std::size_t illicit = 0;
  for (const auto &tx : postedTxns) {
    if (tx.fraud.flag != 0) {
      ++illicit;
    }
  }

  const auto ratio = totalTxns == 0 ? 0.0
                                    : static_cast<double>(illicit) /
                                          static_cast<double>(totalTxns);

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
                         : static_cast<double>(summary.illicitTxnCount) /
                               static_cast<double>(summary.totalTxnCount);

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

    const auto mix = pii::LocaleMix::usBankDefault();

    double totalWeight = 0.0;
    for (const auto w : mix.weights) {
      totalWeight += w;
    }

    pii::PoolSet poolSet;
    if (totalWeight > 0.0) {
      for (const auto country : pl::locale::kCountries) {
        const auto idx = pl::taxonomies::enums::toIndex(country);
        const auto weight = mix.weights[idx];
        if (weight <= 0.0) {
          continue;
        }
        poolSet.byCountry[idx] = generateLocalePool(
            country, weight / totalWeight, args.population, args.seed);
      }
    }

    const auto entityConfig =
        createSynthesisConfig(args, poolSet, mix, window.start);

    auto rng = pl::random::Rng::fromSeed(args.seed);
    const auto result =
        pl::pipeline::simulate(rng, window, entityConfig, args.seed);

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
