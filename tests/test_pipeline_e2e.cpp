#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace pl = ::PhantomLedger;
namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

/// Verify the file exists and has at least a header line.
void expectNonEmptyFile(const fs::path &path) {
  if (!fs::exists(path)) {
    std::fprintf(stderr, "FAIL: file missing: %s\n", path.string().c_str());
    ++failures;
    return;
  }
  std::ifstream in(path);
  std::string line;
  if (!std::getline(in, line) || line.empty()) {
    std::fprintf(stderr, "FAIL: file empty: %s\n", path.string().c_str());
    ++failures;
  }
}

/// Build a temp-directory name with a 16-hex-digit random suffix.
/// Replaces the POSIX-only getpid() approach so the test compiles
/// on Windows too. Random source is just std::random_device — we
/// only need uniqueness across concurrent test runs, not security.
[[nodiscard]] fs::path uniqueTempDir(const std::string &prefix) {
  std::random_device rd;
  std::uint64_t mix = (static_cast<std::uint64_t>(rd()) << 32) |
                      static_cast<std::uint64_t>(rd());
  // Mix in the high-resolution clock so back-to-back invocations
  // never collide even if random_device has limited entropy.
  mix ^= static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());

  char buf[24];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(mix));
  return fs::temp_directory_path() / (prefix + buf);
}

[[nodiscard]] pl::pipeline::SimulationResult runSmallSim(std::uint64_t seed) {
  // Build a default PoolSet so the entities stage has somewhere to
  // pull names + addresses from. The 3-arg `buildLocalePool` overload
  // generates a synthetic pool — enough for a smoke test.
  pl::entities::synth::pii::PoolSet poolSet;
  pl::entities::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::entities::synth::pii::buildLocalePool(
          pl::locale::Country::us, sizes, static_cast<std::uint32_t>(seed));

  pl::time::Window window;
  window.start = pl::time::makeTime({2025, 1, 1});
  window.days = 7;

  pl::pipeline::SimulateInputs in;
  in.window = window;
  in.seed = seed;
  in.entitiesIn.population = 100;
  in.entitiesIn.window = window;
  in.entitiesIn.simStart = window.start;
  in.entitiesIn.piiPools = &poolSet;
  in.infraIn.window = window;

  auto rng = pl::random::Rng::fromSeed(seed);
  return pl::pipeline::simulate(rng, in);
}

void testStandardExport(const pl::pipeline::SimulationResult &result,
                        const fs::path &outDir) {
  pl::exporter::standard::Options opts{};
  pl::exporter::standard::exportAll(result, outDir, opts);

  // 14 standard CSVs at outDir root.
  for (const auto *name : {
           "person.csv",
           "accountnumber.csv",
           "phone.csv",
           "email.csv",
           "device.csv",
           "ipaddress.csv",
           "merchants.csv",
           "external_accounts.csv",
           "HAS_ACCOUNT.csv",
           "HAS_PHONE.csv",
           "HAS_EMAIL.csv",
           "HAS_USED.csv",
           "HAS_IP.csv",
           "HAS_PAID.csv",
       }) {
    expectNonEmptyFile(outDir / name);
  }
  // transactions.csv only emitted with showTransactions=true.
  check(!fs::exists(outDir / "transactions.csv"),
        "transactions.csv NOT emitted by default");
}

void testMuleMlExport(const pl::pipeline::SimulationResult &result,
                      const fs::path &outDir) {
  pl::exporter::mule_ml::Options opts{};
  // includeStandardExport stays at its default (true).
  pl::exporter::mule_ml::exportAll(result, outDir, opts);

  for (const auto *name : {
           "Party.csv",
           "Transfer_Transaction.csv",
           "Account_Device.csv",
           "Account_IP.csv",
       }) {
    expectNonEmptyFile(outDir / "ml_ready" / name);
  }
  // Standard bundle should also be present (includeStandardExport
  // defaults to true).
  expectNonEmptyFile(outDir / "person.csv");
}

void testAmlExport(const pl::pipeline::SimulationResult &result,
                   const fs::path &outDir) {
  pl::exporter::aml::Options opts{};
  const auto summary = pl::exporter::aml::exportAll(result, outDir, opts);

  // 17 vertex CSVs.
  for (const auto *name : {
           "Customer.csv",
           "Account.csv",
           "Counterparty.csv",
           "Name.csv",
           "Address.csv",
           "Country.csv",
           "Watchlist.csv",
           "Device.csv",
           "Transaction.csv",
           "SAR.csv",
           "Bank.csv",
           "Name_MinHash.csv",
           "Address_MinHash.csv",
           "Street_Line1_MinHash.csv",
           "City_MinHash.csv",
           "State_MinHash.csv",
           "Connected_Component.csv",
       }) {
    expectNonEmptyFile(outDir / "aml" / "vertices" / name);
  }
  // Representative edge CSVs (full count is 38 — checking a handful
  // is enough for smoke).
  for (const auto *name : {
           "customer_has_account.csv",
           "send_transaction.csv",
           "uses_device.csv",
           "customer_has_name_minhash.csv",
           "sar_covers.csv",
       }) {
    expectNonEmptyFile(outDir / "aml" / "edges" / name);
  }
  // Summary must reflect the small population.
  check(summary.customerCount == 100,
        "AML summary customerCount == 100, got " +
            std::to_string(summary.customerCount));
}

} // namespace

int main() {
  const auto base = uniqueTempDir("phantomledger_e2e_");
  fs::create_directories(base);

  try {
    const auto result = runSmallSim(/*seed=*/42);
    testStandardExport(result, base / "standard");
    testMuleMlExport(result, base / "mule_ml");
    testAmlExport(result, base / "aml");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    std::fprintf(stderr, "Output preserved at: %s\n", base.string().c_str());
    return 2;
  }

  if (failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed. Output preserved at: %s\n",
                 failures, base.string().c_str());
    return 1;
  }

  // Clean up on success only — leave the dir in place on failure
  // so a developer can inspect it.
  std::error_code ec;
  fs::remove_all(base, ec);

  std::printf("All E2E checks passed.\n");
  return 0;
}
