#include "phantomledger/app/cli.hpp"

#include "phantomledger/app/options.hpp"
#include "phantomledger/app/parsers.hpp"

#include <cstdio>
#include <cstdlib>
#include <format>
#include <limits>
#include <print>
#include <string_view>
#include <utility>

namespace PhantomLedger::app::cli {

namespace {

namespace pl = ::PhantomLedger;

void writeUsage(const char *prog, std::FILE *stream) noexcept {
  std::fprintf(
      stream,
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
      "  --show-transactions               Emit raw transactions.csv\n"
      "  --help, -h                        Show this message\n"
      "\n"
      "Each --usecase emits exactly its own outputs:\n"
      "  standard  -> <out>/*.csv\n"
      "  mule-ml   -> <out>/ml_ready/*.csv\n"
      "  aml       -> <out>/aml/{vertices,edges}/*.csv\n"
      "\n"
      "To produce both standard and mule-ml CSVs over the same dataset,\n"
      "run twice with the same --seed; the directories don't collide.\n",
      prog);
}

} // namespace

void printUsage(const char *prog, std::FILE *stream) noexcept {
  writeUsage(prog, stream);
}

pl::app::RunOptions parse(int argc, char **argv) {
  pl::app::RunOptions opts;

  auto die = [&]<typename... T>(std::format_string<T...> fmt,
                                T &&...formatArgs) {
    std::println(stderr, fmt, std::forward<T>(formatArgs)...);
    std::println(stderr, "");
    writeUsage(argv[0], stderr);
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
      writeUsage(argv[0], stdout);
      std::exit(0);
    }

    if (arg == "--usecase") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = pl::app::parseUseCase(value)) {
        opts.usecase = *parsed;
      } else {
        die("Unknown --usecase value: {}", value);
      }
      continue;
    }

    if (arg == "--days") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseInt(value); parsed && *parsed > 0) {
        opts.days = *parsed;
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
      opts.population = static_cast<std::int32_t>(*parsed);
      continue;
    }

    if (arg == "--seed") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseU64(value)) {
        opts.seed = *parsed;
      } else {
        die("--seed must be a non-negative integer (got {})", value);
      }
      continue;
    }

    if (arg == "--out") {
      opts.outDir = requireValue(i, arg);
      continue;
    }

    if (arg == "--start") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseDate(value)) {
        opts.startDate = *parsed;
      } else {
        die("--start must be YYYY-MM-DD (got {})", value);
      }
      continue;
    }

    if (arg == "--show-transactions") {
      opts.showTransactions = true;
      continue;
    }

    die("Unknown argument: {}", arg);
  }

  return opts;
}

} // namespace PhantomLedger::app::cli
