//
// tests/test_pipeline_e2e.cpp
//
// Serverless smoke gate for the corpus exporters (standard, mule-ml,
// aml, card-fraud): run a small simulation, then require each exporter
// to render its complete table set with real content. PhantomLedger
// writes no files, so the observation seam is common::TableCapture
// (table.hpp) — the exact bytes each table's csv::Writer renders,
// keyed by stem, which is also exactly what the PostgreSQL COPY
// receives on a live run. Schema placement (mule_ml / aml / card_fraud
// schemas, table prefixes) is a mirror concern pinned by the live-PG
// table golden, not here.
//
// No exporter may EVER render a table with stem "transactions": that
// is the streamed corpus table's name, and the canonical stream must
// never be overwritten by a rendered twin.
//

#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/card_fraud/export.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <string>
#include <string_view>

namespace pl = ::PhantomLedger;

namespace {

int failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

// Accumulates every rendered table's bytes by stem — the serverless
// stand-in for the PostgreSQL COPY destination.
class Capture final : public pl::exporter::common::TableCapture {
public:
  void put(std::string_view stem, const char *data,
           std::size_t size) override {
    tables_[std::string{stem}].append(data, size);
  }

  [[nodiscard]] const std::map<std::string, std::string> &tables() const {
    return tables_;
  }

  [[nodiscard]] bool has(const std::string &stem) const {
    return tables_.contains(stem);
  }

private:
  std::map<std::string, std::string> tables_;
};

/// Verify the table was rendered and carries at least a header line.
void expectTable(const Capture &capture, const std::string &stem) {
  const auto it = capture.tables().find(stem);
  if (it == capture.tables().end()) {
    std::fprintf(stderr, "FAIL: table missing: %s\n", stem.c_str());
    ++failures;
    return;
  }
  if (it->second.empty()) {
    std::fprintf(stderr, "FAIL: table empty: %s\n", stem.c_str());
    ++failures;
  }
}

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;

  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));

  return poolSet;
}

[[nodiscard]] pl::time::Window smallWindow() {
  pl::time::Window window;
  window.start = pl::time::makeTime({2025, 1, 1});
  window.days = 7;
  return window;
}

[[nodiscard]] pl::pipeline::stages::entities::EntitySynthesis
smallEntitySynthesis(const pl::synth::pii::PoolSet &poolSet,
                     pl::time::TimePoint simStart,
                     const pl::synth::people::Fraud &fraudProfile) {
  return pl::pipeline::stages::entities::EntitySynthesis{
      .population = 100,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = simStart,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };
}

[[nodiscard]] pl::pipeline::SimulationResult
runSmallSim(const pl::synth::pii::PoolSet &poolSet, std::uint64_t seed) {
  const auto window = smallWindow();

  const pl::synth::people::Fraud fraudProfile{};
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

void testStandardExport(const pl::pipeline::SimulationResult &result) {
  Capture capture;
  pl::exporter::standard::Options opts{};
  opts.capture = &capture;
  pl::exporter::standard::exportAll(result, opts);

  for (const auto *stem : {
           "person",
           "accountnumber",
           "phone",
           "email",
           "device",
           "ipaddress",
           "merchants",
           "external_accounts",
           "HAS_ACCOUNT",
           "HAS_PHONE",
           "HAS_EMAIL",
           "HAS_USED",
           "HAS_IP",
           "HAS_PAID",
       }) {
    expectTable(capture, stem);
  }

  check(!capture.has("transactions"),
        "the 'transactions' stem is the streamed corpus table and must "
        "never be rendered by the standard exporter");
}

void testMuleMlExport(const pl::pipeline::SimulationResult &result,
                      const pl::synth::pii::PoolSet &poolSet) {
  Capture capture;
  pl::exporter::mule_ml::Options opts{};
  opts.piiPools = &poolSet;
  opts.capture = &capture;

  pl::exporter::mule_ml::exportAll(result, opts);

  for (const auto *stem : {
           "Party",
           "Transfer_Transaction",
           "Account_Device",
           "Account_IP",
       }) {
    expectTable(capture, stem);
  }

  check(!capture.has("person"),
        "Mule ML default export is ML-only and does not render person");
}

void testAmlExport(const pl::pipeline::SimulationResult &result,
                   const pl::synth::pii::PoolSet &poolSet) {
  Capture capture;
  pl::exporter::aml::Options opts{};
  opts.piiPools = &poolSet;
  opts.capture = &capture;
  const auto summary = pl::exporter::aml::exportAll(result, opts);

  // Vertex tables (incl. Transaction, streamed by StreamingAmlExport).
  for (const auto *stem : {
           "Customer",
           "Account",
           "Counterparty",
           "Name",
           "Address",
           "Country",
           "Watchlist",
           "Device",
           "Transaction",
           "SAR",
           "Bank",
           "Name_MinHash",
           "Address_MinHash",
           "Street_Line1_MinHash",
           "City_MinHash",
           "State_MinHash",
           "Connected_Component",
       }) {
    expectTable(capture, stem);
  }

  // Edge tables.
  for (const auto *stem : {
           "customer_has_account",
           "send_transaction",
           "uses_device",
           "customer_has_name_minhash",
           "sar_covers",
       }) {
    expectTable(capture, stem);
  }

  check(summary.customerCount == 100,
        "AML summary customerCount == 100, got " +
            std::to_string(summary.customerCount));
}

// The card-fraud exporter's complete 34-table TF_GNN_v3 set, via the
// one-code-path exportAll (the SAME streaming sink the windowed engine
// uses, run over the retained corpus, then the shared finisher).
void testCardFraudExport(const pl::pipeline::SimulationResult &result,
                         const pl::synth::pii::PoolSet &poolSet) {
  Capture capture;
  pl::exporter::card_fraud::Options opts{};
  opts.piiPools = &poolSet;
  opts.window = smallWindow();
  opts.capture = &capture;

  const auto summary = pl::exporter::card_fraud::exportAll(result, opts);

  // Streamed by StreamingCardFraudExport during the fold.
  for (const auto *stem : {
           "Payment_Transaction",
           "Card_Send_Transaction",
           "Merchant_Receive_Transaction",
       }) {
    expectTable(capture, stem);
  }

  // Finisher: cards, merchants + the geo chain, categories, parties.
  // Is_Merchant ships header-only (no modeled merchant-owning-party
  // link) — expectTable's header requirement is exactly its contract.
  for (const auto *stem : {
           "Card",
           "Party_Has_Card",
           "Merchant",
           "Merchant_Assigned",
           "Merchant_Category",
           "Has_State",
           "Has_City",
           "Has_Zip",
           "City",
           "State",
           "Zipcode",
           "Assigned_To",
           "Located_In",
           "Party",
           "Is_Merchant",
       }) {
    expectTable(capture, stem);
  }

  // The PII investigative layer (DEMO ONLY in TF_GNN_v3; PhantomLedger
  // populates it from its PII synthesis).
  for (const auto *stem : {
           "Address",
           "Phone",
           "Email",
           "IP",
           "Device",
           "ID",
           "Full_Name",
           "DOB",
           "Has_Address",
           "Has_Phone",
           "Has_Email",
           "Has_ID",
           "Has_IP",
           "Has_Device",
           "Has_DOB",
           "Has_Full_Name",
       }) {
    expectTable(capture, stem);
  }

  check(!capture.has("transactions"),
        "the 'transactions' stem is the streamed corpus table and must "
        "never be rendered by the card-fraud exporter");

  check(summary.partyCount == 100,
        "card-fraud summary partyCount == 100, got " +
            std::to_string(summary.partyCount));
  check(summary.totalRows == result.transfers.ledger.posted.txns.size(),
        "card-fraud totalRows must count every settled row (the "
        "T<row_seq> identity domain), got " +
            std::to_string(summary.totalRows));
  check(summary.viewRows > 0,
        "card-fraud view (card_purchase + merchant channels) must be "
        "non-empty at pop 100 x 7 days");
}

} // namespace

int main() {
  constexpr std::uint64_t seed = 42;

  try {

    const auto poolSet = buildPoolSet(seed);
    const auto result = runSmallSim(poolSet, seed);

    testStandardExport(result);
    testMuleMlExport(result, poolSet);
    testAmlExport(result, poolSet);
    testCardFraudExport(result, poolSet);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", failures);
    return 1;
  }

  std::printf("All E2E checks passed.\n");
  return 0;
}
