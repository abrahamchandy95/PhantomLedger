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
// Since card-fraud-realism-v2 this file also carries THE LABEL-LEAK
// GATE: the card-fraud exporter's four full-window entity labels must
// render as 0 in the feature graph, and their investigative content
// must appear in the quarantined ground-truth overlay instead. That is
// a content assertion, which is why it lives here (the capture holds
// the rendered bytes) rather than in the digest goldens, which would
// only tell us the bytes CHANGED.
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

// ------------------------------------------------- rendered-CSV probes
//
// Field extraction by comma index, valid ONLY for the columns probed
// below: every field at or before those indices is an identifier, a
// number or a formatted timestamp, so none of them can be quoted or
// carry an embedded comma. (Party.name can — it sits at index 5, well
// past the is_fraud column at index 1.)

[[nodiscard]] std::string_view fieldAt(std::string_view line,
                                       std::size_t index) {
  std::size_t start = 0;
  for (std::size_t i = 0; i < index; ++i) {
    const auto comma = line.find(',', start);
    if (comma == std::string_view::npos) {
      return {};
    }
    start = comma + 1;
  }
  const auto comma = line.find(',', start);
  auto field = line.substr(start, comma == std::string_view::npos
                                      ? std::string_view::npos
                                      : comma - start);
  if (!field.empty() && field.back() == '\r') {
    field.remove_suffix(1);
  }
  return field;
}

/// Count data rows (header skipped) whose column `index` equals `want`,
/// and the data rows in total.
struct ColumnCount {
  std::size_t matching = 0;
  std::size_t rows = 0;
};

[[nodiscard]] ColumnCount countColumn(const Capture &capture,
                                      const std::string &stem,
                                      std::size_t index,
                                      std::string_view want) {
  ColumnCount out;
  const auto it = capture.tables().find(stem);
  if (it == capture.tables().end()) {
    return out;
  }
  std::string_view text{it->second};
  bool header = true;
  while (!text.empty()) {
    const auto nl = text.find('\n');
    const auto line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view{}
                                        : text.substr(nl + 1);
    if (line.empty() || (line.size() == 1 && line.front() == '\r')) {
      continue;
    }
    if (header) {
      header = false;
      continue;
    }
    ++out.rows;
    if (fieldAt(line, index) == want) {
      ++out.matching;
    }
  }
  return out;
}

/// THE LABEL-LEAK GATE. A full-window entity verdict rendered into the
/// feature graph answers the training question before the model sees a
/// transaction, so every cell of the column must be 0.
void expectLabelWithheld(const Capture &capture, const std::string &stem,
                         std::size_t index, const std::string &column) {
  const auto counted = countColumn(capture, stem, index, "0");
  check(counted.rows > 0,
        "label-leak gate needs rows to check in " + stem);
  check(counted.matching == counted.rows,
        stem + "." + column +
            " must be withheld (0) in the feature graph: " +
            std::to_string(counted.rows - counted.matching) + " of " +
            std::to_string(counted.rows) + " rows carry a label");
}

[[nodiscard]] std::size_t groundTruthRowsFor(const Capture &capture,
                                             std::string_view entityType) {
  const auto counted = countColumn(capture, "Ground_Truth_Label", 0,
                                   entityType);
  return counted.matching;
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

// The card-fraud exporter's complete 35-table TF_GNN_v3 set (34 graph
// tables + the quarantined ground-truth overlay), via the one-code-path
// exportAll (the SAME streaming sink the windowed engine uses, run over
// the retained corpus, then the shared finisher).
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

  // card-fraud-realism-v2: the investigative overlay. Rendered always,
  // header included, even when the window produced no positives.
  expectTable(capture, "Ground_Truth_Label");

  check(!capture.has("transactions"),
        "the 'transactions' stem is the streamed corpus table and must "
        "never be rendered by the card-fraud exporter");

  // ------------------------------------------------ THE LABEL-LEAK GATE
  //
  // Four full-window entity verdicts used to ride the feature graph.
  // They keep their columns (TF_GNN_v3 loads positionally) and must
  // carry 0 in every row.
  expectLabelWithheld(capture, "Card", 1, "is_fraud");
  expectLabelWithheld(capture, "Party", 1, "is_fraud");
  expectLabelWithheld(capture, "Device", 1, "is_blocked");
  expectLabelWithheld(capture, "IP", 1, "is_blocked");

  // ...and the supervised target must SURVIVE on the streamed
  // transaction vertex. Withholding is not deletion: whenever the
  // window produced flagged card rows, the entity verdicts they imply
  // must appear in the overlay.
  const auto payments =
      countColumn(capture, "Payment_Transaction", 3, "1");
  std::printf("  card view: %zu rows, %zu flagged; ground truth "
              "card=%zu party=%zu device=%zu ip=%zu\n",
              payments.rows, payments.matching,
              groundTruthRowsFor(capture, "card"),
              groundTruthRowsFor(capture, "party"),
              groundTruthRowsFor(capture, "device"),
              groundTruthRowsFor(capture, "ip"));

  if (payments.matching > 0) {
    check(groundTruthRowsFor(capture, "card") > 0,
          "flagged card-view rows exist, so the ground-truth overlay "
          "must carry the cards they touched (the label moved, it was "
          "not deleted)");
  }

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
