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
      "All output lands in PostgreSQL: the corpus streams into the\n"
      "'transactions' table during settlement and every exporter writes\n"
      "its tables directly (one schema per use case: public / mule_ml /\n"
      "aml / aml_txn_edges). No files are written.\n"
      "\n"
      "Options:\n"
      "  --usecase {standard,mule-ml,aml,aml-txn-edges}  Exporter to run "
      "(default: standard)\n"
      "  --days N                          Simulation length in days "
      "(default: 365)\n"
      "  --population N                    Total population "
      "(default: 70000)\n"
      "  --seed N                          Top-level RNG seed "
      "(default: 0xDEADBEEF)\n"
      "  --start YYYY-MM-DD                Simulation start date "
      "(default: 2025-01-01)\n"
      "  --help, -h                        Show this message\n"
      "\n"
      "Environment:\n"
      "  PL_PG='host=... port=... dbname=...'\n"
      "      PostgreSQL conninfo (default: dbname=phantomledger). A\n"
      "      reachable server is REQUIRED; the run fails fast before any\n"
      "      generation when no server answers. Reruns with the same\n"
      "      seed and config rewrite byte-identical content.\n"
      "\n"
      "Test infrastructure (not for production use; retired with the\n"
      "CSV arc):\n"
      "  PL_FILE_ONLY=1                    Skip PostgreSQL (serverless\n"
      "                                    harness escape; aml-txn-edges\n"
      "                                    cannot run this way)\n"
      "  PL_ENGINE={windowed,monolithic}   Force the corpus engine (the\n"
      "                                    monolithic reference engine\n"
      "                                    backs the equivalence gates)\n"
      "  --out PATH                        Legacy CSV tree for the\n"
      "                                    file-based parity gates\n"
      "  --show-transactions               Legacy raw-ledger dump\n"
      "                                    (requires --out)\n",
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

  // Engine selection is automatic; PL_ENGINE is the test-infrastructure
  // override for the monolithic reference engine (never a CLI flag).
  if (const char *env = std::getenv("PL_ENGINE")) {
    if (const auto parsed = pl::app::parseEngine(env)) {
      opts.engine = *parsed;
    } else {
      die("Unknown PL_ENGINE value: {} (expected windowed or monolithic)",
          std::string_view{env});
    }
  }

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
      // Test infrastructure: keeps the file-based parity gates alive
      // until they are deleted with the rest of the CSV arc. A default
      // run writes no files.
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
      // Test infrastructure (see --out); a ledger dump needs a file
      // tree to land in.
      opts.showTransactions = true;
      continue;
    }

    if (arg == "--csv") {
      die("--csv does not exist: PhantomLedger is PostgreSQL-native and "
          "writes no files. The file-based test gates use --out until "
          "they are retired.");
    }

    if (arg == "--engine") {
      die("--engine has been removed: engine selection is automatic and "
          "output bytes are engine-independent. The monolithic reference "
          "engine is test infrastructure, reachable via "
          "PL_ENGINE=monolithic.");
    }

    if (arg == "--windowed") {
      die("--windowed has been removed: the windowed engine is the "
          "default for every use case. The monolithic reference engine "
          "is test infrastructure, reachable via PL_ENGINE=monolithic.");
    }

    die("Unknown argument: {}", arg);
  }

  return opts;
}

} // namespace PhantomLedger::app::cli
