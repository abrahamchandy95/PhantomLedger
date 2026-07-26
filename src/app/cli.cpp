#include "phantomledger/app/cli.hpp"

#include "phantomledger/app/options.hpp"
#include "phantomledger/app/parsers.hpp"
#include "phantomledger/synth/econ/catalog.hpp"

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
      "All output lands in PostgreSQL: the corpus streams into the\n"
      "'transactions' table during settlement and every exporter writes\n"
      "its tables directly (one schema per use case: public / mule_ml /\n"
      "aml / aml_txn_edges / card_fraud). No files are written.\n"
      "\n"
      "Options:\n"
      "  --usecase {standard,mule-ml,aml,aml-txn-edges,card-fraud}\n"
      "                                    Exporter to run "
      "(default: standard)\n"
      "  --days N                          Simulation length in days "
      "(default: 365)\n"
      "  --population N                    Total population "
      "(default: 70000)\n"
      "  --seed N                          Top-level RNG seed "
      "(default: 0xDEADBEEF)\n"
      "  --start YYYY-MM-DD                Simulation start date "
      "(default: 2025-01-01)\n"
      "                                    card-fraud windows must lie "
      "inside the\n"
      "                                    pinned economic era "
      "(docs/era_data_provenance.md;\n"
      "                                    canonical --start 1991-01-01)\n"
      "  --help, -h                        Show this message\n"
      "\n"
      "Environment:\n"
      "  PL_PG='host=... port=... dbname=...'\n"
      "      PostgreSQL conninfo (default: %s). A\n"
      "      reachable server is REQUIRED; the run fails fast before any\n"
      "      generation when no server answers. Reruns with the same\n"
      "      seed and config rewrite byte-identical content.\n"
      "\n"
      "Test infrastructure (not for production use):\n"
      "  PL_FILE_ONLY=1                    Skip PostgreSQL (serverless\n"
      "                                    harness escape; produces only\n"
      "                                    the stream digest; aml-txn-\n"
      "                                    edges cannot run this way)\n",
      prog, pl::app::kDefaultPgConninfo);
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

    if (arg == "--start") {
      const auto value = requireValue(i, arg);
      if (const auto parsed = parseDate(value)) {
        opts.startDate = *parsed;
      } else {
        die("--start must be YYYY-MM-DD (got {})", value);
      }
      continue;
    }

    if (arg == "--out" || arg == "--csv") {
      die("{} has been removed: PhantomLedger is PostgreSQL-native and "
          "writes no files. Every table lands directly in PostgreSQL "
          "(PL_PG) during the run.",
          arg);
    }

    if (arg == "--show-transactions") {
      die("--show-transactions has been removed: the raw ledger IS the "
          "streamed 'transactions' table in PostgreSQL — "
          "SELECT * FROM transactions ORDER BY row_seq.");
    }

    if (arg == "--engine" || arg == "--windowed") {
      die("{} has been removed: the windowed streaming engine is the "
          "only engine. Its retained-corpus reference implementation "
          "survives as library test infrastructure "
          "(SimulationPipeline::run, pinned by test_arch_equivalence "
          "and test_production_windowed).",
          arg);
    }

    die("Unknown argument: {}", arg);
  }

  // Era lock (macro-history-v1 H0.6, owner directive #3): card-fraud
  // realism is anchored to the frozen economic era EMBEDDED in
  // synth/econ/era_data.hpp, so an out-of-era window fails fast here
  // instead of silently simulating years the model has no data for.
  // The bounds come from the pinned series (appending fully published
  // years to the embedded data widens the lock — no engine change);
  // other use cases stay era-agnostic until macro-history H1 wiring.
  if (opts.usecase == pl::app::UseCase::cardFraud) {
    const auto &era = pl::synth::econ::macroSeries();
    if (!pl::app::windowInsideEra(opts, era.firstYear(), era.lastYear())) {
      const auto last = pl::app::lastSimulatedDay(opts);
      die("--usecase card-fraud is time-locked to the pinned economic era "
          "{}-{}: the requested window {:04}-{:02}-{:02} + {} days runs "
          "through {:04}-{:02}-{:02}. Pick a start/days inside the era "
          "(canonical: --start 1991-01-01) or extend the pinned series "
          "(docs/era_data_provenance.md).",
          era.firstYear(), era.lastYear(), opts.startDate.year,
          opts.startDate.month, opts.startDate.day, opts.days, last.year,
          last.month, last.day);
    }
  }

  return opts;
}

} // namespace PhantomLedger::app::cli
