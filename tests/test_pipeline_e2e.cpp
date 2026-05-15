#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

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
/// Randomness is only for avoiding collisions across concurrent test runs.
[[nodiscard]] fs::path uniqueTempDir(const std::string &prefix) {
  std::random_device rd;
  std::uint64_t mix = (static_cast<std::uint64_t>(rd()) << 32U) |
                      static_cast<std::uint64_t>(rd());

  mix ^= static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());

  char buf[24];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(mix));

  return fs::temp_directory_path() / (prefix + buf);
}

/// Build a US-only PII pool. The accompanying LocaleMix in the test
/// configuration is `usOnly()`, so no other country's slot will ever be
/// consulted — the `PoolSet::forCountry` assert in non-US slots stays
/// dormant by design.
[[nodiscard]] pl::entities::synth::pii::PoolSet
buildPoolSet(std::uint64_t seed) {
  pl::entities::synth::pii::PoolSet poolSet;
  pl::entities::synth::pii::PoolSizes sizes;

  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::entities::synth::pii::buildLocalePool(
          pl::locale::Country::us, sizes, static_cast<std::uint32_t>(seed));

  return poolSet;
}

[[nodiscard]] pl::time::Window smallWindow() {
  pl::time::Window window;
  window.start = pl::time::makeTime({2025, 1, 1});
  window.days = 7;
  return window;
}

/// Build the EntitySynthesis config tree for the small E2E test.
///
/// The struct is flat: `population` and `identity` are top-level
/// user-owned fields, `fraud` is a top-level calibration field. All
/// remaining calibration siblings (personaMix, accountsSizing,
/// merchants/landlords/counterpartyTargets plans, cards) keep their
/// research-backed defaults via designated-init omission.
[[nodiscard]] pl::pipeline::stages::entities::EntitySynthesis
smallEntitySynthesis(const pl::entities::synth::pii::PoolSet &poolSet,
                     pl::time::TimePoint simStart,
                     const pl::entities::synth::people::Fraud &fraudProfile) {
  return pl::pipeline::stages::entities::EntitySynthesis{
      .population = 100,
      .identity =
          pl::entities::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = simStart,
              .localeMix = pl::entities::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };
}

/// Run the full pipeline end-to-end with default configuration.
///
/// All entity flows (employers, clients, merchants, landlords, products,
/// fraud, recurring income) run with their library defaults. The point of
/// this test is to exercise the same public simulation pipeline production
/// callers use and assert that the three exporters produce the file shapes
/// downstream pipelines depend on.
///
/// The PoolSet is passed in by reference because it must outlive the
/// SimulationResult: PII samplers used during synthesis embed indices
/// into this pool, and the AML / mule_ml exporters resolve those indices
/// back to names and addresses against the same instance. Owning the
/// PoolSet in main() keeps that lifetime explicit.
[[nodiscard]] pl::pipeline::SimulationResult
runSmallSim(const pl::entities::synth::pii::PoolSet &poolSet,
            std::uint64_t seed) {
  const auto window = smallWindow();

  const pl::entities::synth::people::Fraud fraudProfile{};
  const auto entities =
      smallEntitySynthesis(poolSet, window.start, fraudProfile);

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(seed);

  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, seed};

  pipeline.transferStage()
      .legit()
      .window(window)
      .seed(seed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);

  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

void testStandardExport(const pl::pipeline::SimulationResult &result,
                        const fs::path &outDir) {
  pl::exporter::standard::Options opts{};
  pl::exporter::standard::exportAll(result, outDir, opts);

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

  check(!fs::exists(outDir / "transactions.csv"),
        "transactions.csv NOT emitted by default");
}

void testMuleMlExport(const pl::pipeline::SimulationResult &result,
                      const pl::entities::synth::pii::PoolSet &poolSet,
                      const fs::path &outDir) {
  pl::exporter::mule_ml::Options opts{};
  opts.piiPools = &poolSet;

  pl::exporter::mule_ml::exportAll(result, outDir, opts);

  for (const auto *name : {
           "Party.csv",
           "Transfer_Transaction.csv",
           "Account_Device.csv",
           "Account_IP.csv",
       }) {
    expectNonEmptyFile(outDir / "ml_ready" / name);
  }

  check(!fs::exists(outDir / "person.csv"),
        "Mule ML default export is ML-only and does not emit person.csv");
}

void testAmlExport(const pl::pipeline::SimulationResult &result,
                   const pl::entities::synth::pii::PoolSet &poolSet,
                   const fs::path &outDir) {
  pl::exporter::aml::Options opts{};
  opts.piiPools = &poolSet;
  const auto summary = pl::exporter::aml::exportAll(result, outDir, opts);

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

  for (const auto *name : {
           "customer_has_account.csv",
           "send_transaction.csv",
           "uses_device.csv",
           "customer_has_name_minhash.csv",
           "sar_covers.csv",
       }) {
    expectNonEmptyFile(outDir / "aml" / "edges" / name);
  }

  check(summary.customerCount == 100,
        "AML summary customerCount == 100, got " +
            std::to_string(summary.customerCount));
}

} // namespace

int main() {
  const auto base = uniqueTempDir("phantomledger_e2e_");
  fs::create_directories(base);

  constexpr std::uint64_t seed = 42;

  try {
    // PoolSet is owned here so it outlives every exporter: PII samplers
    // burned indices into this pool during synthesis, and the AML and
    // mule_ml exporters dereference those indices back to names / addresses
    // against the same instance. This mirrors production main.cpp's
    // ownership pattern.
    const auto poolSet = buildPoolSet(seed);
    const auto result = runSmallSim(poolSet, seed);

    testStandardExport(result, base / "standard");
    testMuleMlExport(result, poolSet, base / "mule_ml");
    testAmlExport(result, poolSet, base / "aml");
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

  std::error_code ec;
  fs::remove_all(base, ec);

  std::printf("All E2E checks passed.\n");
  return 0;
}
