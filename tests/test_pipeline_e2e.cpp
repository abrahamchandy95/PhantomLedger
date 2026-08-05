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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
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
  void put(std::string_view stem, const char *data, std::size_t size) override {
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
    text =
        nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
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

[[nodiscard]] std::set<std::string> columnValues(const Capture &capture,
                                                 const std::string &stem,
                                                 std::size_t index) {
  std::set<std::string> out;
  const auto it = capture.tables().find(stem);
  if (it == capture.tables().end()) {
    return out;
  }
  std::string_view bytes{it->second};
  bool header = true;
  while (!bytes.empty()) {
    const auto nl = bytes.find('\n');
    auto line = bytes.substr(0, nl);
    bytes = nl == std::string_view::npos ? std::string_view{}
                                         : bytes.substr(nl + 1);
    if (header) {
      header = false;
      continue;
    }
    const auto value = fieldAt(line, index);
    if (!value.empty()) {
      out.emplace(value);
    }
  }
  return out;
}

/// Two columns of one table as a map, for the invariants that need a row
/// read together rather than a column read alone (the coordinate gate has
/// to compare a merchant's point against its OWN zipcode's point).
[[nodiscard]] std::map<std::string, std::string>
columnPairs(const Capture &capture, const std::string &stem,
            std::size_t keyIndex, std::size_t valueIndex) {
  std::map<std::string, std::string> out;
  const auto it = capture.tables().find(stem);
  if (it == capture.tables().end()) {
    return out;
  }
  std::string_view bytes{it->second};
  bool header = true;
  while (!bytes.empty()) {
    const auto nl = bytes.find('\n');
    const auto line = bytes.substr(0, nl);
    bytes = nl == std::string_view::npos ? std::string_view{}
                                        : bytes.substr(nl + 1);
    if (line.empty() || (line.size() == 1 && line.front() == '\r')) {
      continue;
    }
    if (header) {
      header = false;
      continue;
    }
    const auto key = fieldAt(line, keyIndex);
    if (key.empty()) {
      continue;
    }
    out.emplace(std::string{key}, std::string{fieldAt(line, valueIndex)});
  }
  return out;
}

/// THE LABEL-LEAK GATE. A full-window entity verdict rendered into the
/// feature graph answers the training question before the model sees a
/// transaction, so every cell of the column must be 0.
void expectLabelWithheld(const Capture &capture, const std::string &stem,
                         std::size_t index, const std::string &column) {
  const auto counted = countColumn(capture, stem, index, "0");
  check(counted.rows > 0, "label-leak gate needs rows to check in " + stem);
  check(counted.matching == counted.rows,
        stem + "." + column + " must be withheld (0) in the feature graph: " +
            std::to_string(counted.rows - counted.matching) + " of " +
            std::to_string(counted.rows) + " rows carry a label");
}

[[nodiscard]] std::size_t groundTruthRowsFor(const Capture &capture,
                                             std::string_view entityType) {
  const auto counted =
      countColumn(capture, "Ground_Truth_Label", 0, entityType);
  return counted.matching;
}

// party-geography-2026-07: THE HARNESS NOW RUNS THE PRODUCTION LOCALE MIX,
// and closing that divergence is part of the round rather than a cleanup.
//
// Every gate harness in this suite ran `LocaleMix::usOnly()` while
// production runs `usBankDefault()` — 96% US plus fifteen foreign weights —
// so **no test had ever exported a foreign-domiciled party.** That was
// harmless while party geography did not exist. It stopped being harmless
// the moment an edge began resolving a party's home area, because the
// foreign areas are the ones whose city ids, subdivision codes and postal
// formats differ from every US row.
//
// The foreign pools are built SMALL: the mix draws ~4% of a 100-person
// roster, and a pool only has to be large enough that index draws are
// in-range. The US pool keeps the full default sizes.
[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;

  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));

  pl::synth::pii::PoolSizes minority;
  minority.firstNames = 512;
  minority.middleNames = 512;
  minority.lastNames = 512;
  minority.streets = 512;
  minority.businessNames = 128;

  for (const auto country : pl::locale::kCountries) {
    if (country == pl::locale::Country::us) {
      continue;
    }
    const auto idx = pl::taxonomies::enums::toIndex(country);
    poolSet.byCountry[idx] = pl::synth::pii::buildLocalePool(
        country, minority, static_cast<std::uint32_t>(seed) + 1U +
                               static_cast<std::uint32_t>(idx));
  }

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
              // THE PRODUCTION MIX, not usOnly — see buildPoolSet above.
              .localeMix = pl::synth::pii::LocaleMix::usBankDefault(),
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

// The card-fraud exporter's complete 39-table set (the 34-table
// TF_GNN_v3-compatible set, two event-time session edges, the
// quarantined ground-truth overlay, and the email LSH minhash
// vertex/edge pair), via the one-code-path
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
           "Transaction_Uses_Device",
           "Transaction_Uses_IP",
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
           "Merchant_Location",
           "Has_Std_City",
           "Has_Std_Postcode",
           "Has_Std_State",
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
  // populates it from its PII synthesis). Has_Device and Has_IP are
  // populated as of attacker-infra-2026-07 — see the inverted assertion
  // below for why they were withheld and what changed in the generator.
  for (const auto *stem : {
           "Address",
           "Phone",
           "Email",
           "Email_Minhash",
           "Has_Email_Minhash",
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
  // ------------------------------- THE OWNERSHIP TOPOLOGY, RESTORED
  //
  // THIS ASSERTION IS INVERTED FROM WHAT IT SAID FOR FOUR ROUNDS
  // (attacker-infra-2026-07). It used to require both tables to be
  // HEADER-ONLY, and that was right at the time: every legitimate
  // endpoint had an owning Party and no attacker endpoint did, so "no
  // Party edge" was an exact synonym for "attacker endpoint" — a free,
  // perfect label. The withholding was a stopgap for a GENERATION
  // defect, and the generator has now changed on both sides of it:
  //
  //   * `infra::enrollment` models the endpoint registry as INCOMPLETE,
  //     so legitimate rows arrive on endpoints with no Party edge.
  //   * the fraud planner puts a declared share of unauthorized cases on
  //     the victim's own endpoint and routes a share of operator sessions
  //     through a residential proxy, so fraud arrives on endpoints that
  //     DO have one.
  //
  // Neither direction is deterministic any more, and
  // tests/test_card_endpoint_graph.cpp SIZES the residual lift and fails
  // if it climbs back. Emptiness was never free: with `Has_*` empty and
  // no transaction->endpoint edge type in TF_GNN_v3, Device and IP were
  // unreachable vertices and the whole endpoint layer was inert.
  //
  // What is gated here is PRESENCE plus REFERENTIAL INTEGRITY. The
  // anti-shortcut property is not measurable from a 100-person, 7-day
  // window — it needs a fraud population — so it lives in its own gate
  // rather than being asserted vacuously here.
  const auto hasDeviceRows = countColumn(capture, "Has_Device", 0, "__never__");
  const auto hasIpRows = countColumn(capture, "Has_IP", 0, "__never__");
  check(hasDeviceRows.rows > 0,
        "Has_Device must carry the party->device associations the "
        "institution has on file; empty leaves cf_Device unreachable from "
        "Party and the endpoint layer inert");
  check(hasIpRows.rows > 0,
        "Has_IP must carry the party->address associations on file; empty "
        "leaves cf_IP unreachable from Party");

  // ------------------------------ Is_Merchant: the ownership register
  //
  // ALSO INVERTED (merchant-ownership-2026-07). It was header-only
  // because `Role::business` and `Role::merchant` keys were disjoint
  // populations the world never joined — and an empty table hard-aborts
  // the downstream tf_gnn_loader_v2 push at 001_validate_sources.sql with
  // "cf_Is_Merchant is empty; Party_Is_Merchant edges would not load".
  // A validator in another repository is not something this suite can
  // see, which is exactly why the requirement is pinned here now.
  const auto isMerchantRows = countColumn(capture, "Is_Merchant", 0, "__never__");
  check(isMerchantRows.rows > 0,
        "Is_Merchant must carry the merchant->proprietor register; empty "
        "aborts the downstream loader before any data reaches TigerGraph");

  {
    const auto partyIds = columnValues(capture, "Party", 0);
    const auto merchantIds = columnValues(capture, "Merchant", 0);
    const auto deviceIds = columnValues(capture, "Device", 0);
    const auto ipIds = columnValues(capture, "IP", 0);
    check(std::ranges::includes(merchantIds,
                                columnValues(capture, "Is_Merchant", 0)),
          "every Is_Merchant merchant_id resolves to a Merchant vertex — the "
          "register is restricted to view-observed merchants because the "
          "vertex set is stream-derived and grows");
    check(std::ranges::includes(partyIds,
                                columnValues(capture, "Is_Merchant", 1)),
          "every Is_Merchant party_id resolves to a Party vertex");
    check(std::ranges::includes(partyIds, columnValues(capture, "Has_Device", 0)),
          "every Has_Device party_id resolves to a Party vertex");
    check(std::ranges::includes(deviceIds,
                                columnValues(capture, "Has_Device", 1)),
          "every Has_Device device_id resolves to a Device vertex");
    check(std::ranges::includes(partyIds, columnValues(capture, "Has_IP", 0)),
          "every Has_IP party_id resolves to a Party vertex");
    check(std::ranges::includes(ipIds, columnValues(capture, "Has_IP", 1)),
          "every Has_IP ip_id resolves to an IP vertex");
  }

  // ------------------------------------------- THE COORDINATE GATE
  //
  // merchant-coordinates-2026-07. The world has carried area centroids
  // since geo-causal-v1 and used them to drive distance-decay selection,
  // but no exporter wrote one, so tf_gnn_loader_v2 filled
  // Merchant_Location/City/Zipcode lat+lon from schema defaults with
  // has_coordinates=false. What is gated here is not "a coordinate column
  // exists" — it is that the point is the RIGHT point.
  //
  // A count of coordinates is not a measurement of geography: three of
  // these five checks would pass against a table of constant 0,0, which is
  // a real place off West Africa. So the gate pins the coordinate against
  // the merchant's OWN zipcode row, bounds it to the US (placeGeography's
  // domesticAreas() filters on Country::us, so anything outside is a
  // defect), and requires more than one distinct point.
  {
    const auto merchantIds = columnValues(capture, "Merchant", 0);
    const auto locationRows =
        countColumn(capture, "Merchant_Location", 0, "__never__");
    check(locationRows.rows > 0,
          "Merchant_Location must carry the merchant coordinate pair; empty "
          "leaves TF_GNN_v3's Merchant_Location.lat/lon on their defaults and "
          "has_coordinates false");
    check(std::ranges::includes(merchantIds,
                                columnValues(capture, "Merchant_Location", 0)),
          "every Merchant_Location merchant_id resolves to a Merchant vertex");

    // ROW PRESENCE IS THE has_coordinates MASK, so the coordinate-bearing
    // population must be EXACTLY the geography-bearing one. A mismatch
    // either way is a defect: extra rows mean a coordinate was invented for
    // a geography-free merchant, missing rows mean a physical outlet lost
    // its point.
    check(columnValues(capture, "Merchant_Location", 0) ==
              columnValues(capture, "Has_Zip", 0),
          "Merchant_Location must cover exactly the merchants that have a "
          "modelled area — row presence is the has_coordinates mask");

    const auto zipLat = columnPairs(capture, "Zipcode", 0, 1);
    const auto zipLon = columnPairs(capture, "Zipcode", 0, 2);
    const auto merchantZip = columnPairs(capture, "Has_Zip", 0, 1);
    const auto merchantLat = columnPairs(capture, "Merchant_Location", 0, 1);
    const auto merchantLon = columnPairs(capture, "Merchant_Location", 0, 2);

    // US bounds, generous enough for Alaska and Hawaii. Catches a sign
    // flip, a swapped lat/lon (a US longitude is never a valid US
    // latitude) and the 0,0 default.
    constexpr double kLatLo = 18.0;
    constexpr double kLatHi = 72.0;
    constexpr double kLonLo = -180.0;
    constexpr double kLonHi = -66.0;

    std::size_t outOfBounds = 0;
    std::size_t disagreeing = 0;
    std::set<std::string> distinctPoints;
    for (const auto &[merchantId, latText] : merchantLat) {
      const auto lonIt = merchantLon.find(merchantId);
      if (lonIt == merchantLon.end()) {
        ++disagreeing;
        continue;
      }
      const auto lat = std::stod(latText);
      const auto lon = std::stod(lonIt->second);
      if (!(lat >= kLatLo && lat <= kLatHi && lon >= kLonLo && lon <= kLonHi)) {
        ++outOfBounds;
      }
      distinctPoints.emplace(latText + "," + lonIt->second);

      // The strong check: the merchant's point must be the point its own
      // Zipcode vertex reports, byte for byte. Both render the same
      // catalogue field through the same writer, so exact equality is the
      // correct assertion — any drift means the two tables resolved
      // different areas.
      const auto zipIt = merchantZip.find(merchantId);
      if (zipIt == merchantZip.end()) {
        ++disagreeing;
        continue;
      }
      const auto latIt = zipLat.find(zipIt->second);
      const auto lonZipIt = zipLon.find(zipIt->second);
      if (latIt == zipLat.end() || lonZipIt == zipLon.end() ||
          latIt->second != latText || lonZipIt->second != lonIt->second) {
        ++disagreeing;
      }
    }

    check(outOfBounds == 0,
          "every merchant coordinate must sit inside the US bounding box (" +
              std::to_string(outOfBounds) + " outside): merchants are placed "
              "by placeGeography, which draws only from Country::us areas");
    check(disagreeing == 0,
          "every merchant coordinate must equal its own Zipcode vertex's "
          "coordinate (" +
              std::to_string(disagreeing) +
              " disagree) — one area, one point, reported identically");
    check(distinctPoints.size() > 1,
          "the coordinate layer must carry more than one distinct point (" +
              std::to_string(distinctPoints.size()) +
              "); a constant column would pass every check above and carry no "
              "geography at all");
    std::printf("  merchant coordinates: %zu merchants across %zu distinct "
                "area centroids (co-located merchants share a point by "
                "construction — the world models areas, not street addresses)\n",
                locationRows.rows, distinctPoints.size());
    if (!merchantLat.empty()) {
      const auto &first = *merchantLat.begin();
      const auto lonIt = merchantLon.find(first.first);
      std::printf("  e.g. %s -> lat %s, lon %s\n", first.first.c_str(),
                  first.second.c_str(),
                  lonIt == merchantLon.end() ? "?" : lonIt->second.c_str());
    }

    // ------------------------------- THE DISTANCE-COMPUTABILITY GATE
    //
    // party-geography-2026-07. Merchant coordinates alone did not make
    // cardholder-to-merchant DISTANCE computable — the feature the
    // generator's own selection kernel is built on. It needs BOTH
    // endpoints, and the party endpoint was absent: `cf_Address` is a bare
    // street string and `pii::Address::geoArea` had no exported form.
    //
    // What is gated is the JOIN PATH END TO END, not the presence of three
    // more tables: every Party must reach a Zipcode that carries a
    // coordinate, so a haversine is actually evaluable for every
    // cardholder. Referential integrity is checked in BOTH directions
    // because a party home area the merchant loop never visited has to
    // have been unioned into the vertex tables — that is the one way this
    // round could dangle an edge and have the loader reject the push.
    const auto partyIdsAll = columnValues(capture, "Party", 0);
    const auto stdCityRows = countColumn(capture, "Has_Std_City", 0, "__never__");
    check(stdCityRows.rows > 0,
          "Has_Std_City must carry party home geography; empty leaves "
          "cardholder-to-merchant distance uncomputable downstream");
    // relocation-2026-07: ONE ROW PER TENURE, so the count is now
    // >= the party count and equality is only the no-move case. This gate's
    // window is SEVEN DAYS, where nobody moves — so an equality assertion
    // would still pass here and would be measuring nothing. What must hold at
    // any window length is that every party is COVERED exactly once per
    // tenure, so the check is on DISTINCT parties.
    //
    // The rate and the multi-tenure shape are gated where they can actually be
    // seen, in `test_relocation` over a 20-year window. Asserting them here
    // would be asserting them against a window that cannot produce them.
    const auto stdCityParties = columnValues(capture, "Has_Std_City", 0);
    std::set<std::string> distinctStdCityParties(stdCityParties.begin(),
                                                 stdCityParties.end());
    check(distinctStdCityParties.size() == partyIdsAll.size(),
          "every Party must have at least one home-city edge (" +
              std::to_string(distinctStdCityParties.size()) +
              " distinct parties covered for " +
              std::to_string(partyIdsAll.size()) +
              " parties): the production locale mix names exactly the "
              "countries the catalogue covers, so partial coverage is a "
              "defect, not a modelling choice");
    check(stdCityRows.rows >= partyIdsAll.size(),
          "Has_Std_City must carry at least one row per party (" +
              std::to_string(stdCityRows.rows) + " rows for " +
              std::to_string(partyIdsAll.size()) + " parties)");
    // The three edge tables are written from ONE loop over the same tenures,
    // so their row counts must agree exactly. A mismatch means one of the
    // three writers was missed when the tenure loop was introduced — the
    // failure mode that would silently give a mover a current city and a
    // stale state.
    check(countColumn(capture, "Has_Std_Postcode", 0, "__never__").rows ==
                  stdCityRows.rows &&
              countColumn(capture, "Has_Std_State", 0, "__never__").rows ==
                  stdCityRows.rows,
          "Has_Std_City / _Postcode / _State must carry the SAME row count — "
          "they are written from one tenure loop, so a divergence means a "
          "mover got a current city beside a stale state");
    // Every `since_unix_time` must be inside the window. A tenure stamped
    // before the corpus starts would make the earliest transaction resolve to
    // no home at all.
    {
      const auto sinceValues = columnValues(capture, "Has_Std_City", 2);
      std::size_t outOfWindow = 0;
      const auto windowStartEpoch =
          pl::time::toEpochSeconds(smallWindow().start);
      const auto windowEndEpoch =
          pl::time::toEpochSeconds(pl::time::addDays(smallWindow().start, smallWindow().days));
      for (const auto &value : sinceValues) {
        if (value.empty()) {
          ++outOfWindow;
          continue;
        }
        const auto since = std::stoll(value);
        if (since < windowStartEpoch || since >= windowEndEpoch) {
          ++outOfWindow;
        }
      }
      check(outOfWindow == 0,
            "every Has_Std_City.since_unix_time must fall inside the export "
            "window (" +
                std::to_string(outOfWindow) +
                " do not). A tenure stamped before the corpus starts leaves "
                "the earliest transaction with no resolvable home");
    }

    for (const auto *stem : {"Has_Std_City", "Has_Std_Postcode",
                             "Has_Std_State"}) {
      check(std::ranges::includes(partyIdsAll, columnValues(capture, stem, 0)),
            std::string{stem} + " party_id must resolve to a Party vertex");
    }
    check(std::ranges::includes(columnValues(capture, "City", 0),
                                columnValues(capture, "Has_Std_City", 1)),
          "every party home city must be a City VERTEX — party areas the "
          "merchant loop never visited must be unioned into the vertex "
          "tables or the loader rejects the edge");
    check(std::ranges::includes(columnValues(capture, "Zipcode", 0),
                                columnValues(capture, "Has_Std_Postcode", 1)),
          "every party home zipcode must be a Zipcode vertex");
    check(std::ranges::includes(columnValues(capture, "State", 0),
                                columnValues(capture, "Has_Std_State", 1)),
          "every party home state must be a State vertex");

    // The end-to-end assertion: walk Party -> Zipcode -> (lat, lon) and
    // require a usable coordinate at the end of every walk.
    const auto partyZip = columnPairs(capture, "Has_Std_Postcode", 0, 1);
    std::size_t unreachable = 0;
    std::set<std::string> homePoints;
    for (const auto &[partyId, zip] : partyZip) {
      const auto latIt = zipLat.find(zip);
      const auto lonIt = zipLon.find(zip);
      if (latIt == zipLat.end() || lonIt == zipLon.end() ||
          latIt->second.empty() || lonIt->second.empty()) {
        ++unreachable;
        continue;
      }
      homePoints.emplace(latIt->second + "," + lonIt->second);
    }
    check(unreachable == 0,
          "every Party must reach a coordinate-bearing Zipcode (" +
              std::to_string(unreachable) +
              " cannot): this two-hop walk IS the distance feature, and a "
              "single gap makes it unevaluable for that cardholder");
    check(homePoints.size() > 1,
          "party home coordinates must span more than one point (" +
              std::to_string(homePoints.size()) +
              "); one point means every cardholder is equidistant from "
              "every merchant and the feature is constant");

    // THE GATE GATES ITS OWN COVERAGE.
    //
    // The party half of the geography layer branches on nothing but
    // `contains(geoArea)`, so the FOREIGN path shares the US code exactly —
    // what differs is the DATA: foreign areas carry non-US subdivision
    // codes, non-US postal formats and coordinates outside the US box. This
    // harness ran `usOnly()` until this round and could not have seen any
    // of it. Now that it runs the production mix, at least one home
    // centroid must land outside the US bounding box, or the harness has
    // silently lost the coverage again — which is exactly how a foreign
    // party being dropped would go unnoticed, and "has a Std_City edge"
    // would quietly become a US-residency flag.
    //
    // Deterministic, not probabilistic: the seed is fixed, so this either
    // holds or the harness changed and should say so.
    std::size_t foreignHomes = 0;
    for (const auto &point : homePoints) {
      const auto comma = point.find(',');
      const auto lat = std::stod(point.substr(0, comma));
      const auto lon = std::stod(point.substr(comma + 1));
      if (!(lat >= kLatLo && lat <= kLatHi && lon >= kLonLo &&
            lon <= kLonHi)) {
        ++foreignHomes;
      }
    }
    check(foreignHomes > 0,
          "the harness must exercise at least one FOREIGN-domiciled party's "
          "home geography (found 0 of " + std::to_string(homePoints.size()) +
              " home centroids outside the US box). Production runs "
              "LocaleMix::usBankDefault (96% US + 15 foreign weights); a "
              "us-only harness cannot see the foreign areas, which are the "
              "ones whose city ids and postal formats differ");
    std::printf("  distance feature: %zu parties -> %zu distinct home "
                "centroids (%zu foreign, so the non-US path is covered); "
                "both endpoints of a haversine now exported\n",
                partyZip.size(), homePoints.size(), foreignHomes);
  }

  // ...and the supervised target must SURVIVE on the streamed
  // transaction vertex. Withholding is not deletion: whenever the
  // window produced flagged card rows, the entity verdicts they imply
  // must appear in the overlay.
  const auto payments = countColumn(capture, "Payment_Transaction", 3, "1");
  const auto deviceEdges =
      countColumn(capture, "Transaction_Uses_Device", 0, "__never__");
  const auto ipEdges =
      countColumn(capture, "Transaction_Uses_IP", 0, "__never__");
  check(deviceEdges.rows == payments.rows,
        "every visible payment has exactly one event-time device edge");
  check(ipEdges.rows == payments.rows,
        "every visible payment has exactly one event-time IP edge");

  const auto deviceVertices = columnValues(capture, "Device", 0);
  const auto deviceEndpoints =
      columnValues(capture, "Transaction_Uses_Device", 1);
  const auto ipVertices = columnValues(capture, "IP", 0);
  const auto ipEndpoints = columnValues(capture, "Transaction_Uses_IP", 1);
  check(std::ranges::includes(deviceVertices, deviceEndpoints),
        "every observed transaction device is present in cf_Device");
  check(std::ranges::includes(ipVertices, ipEndpoints),
        "every observed transaction IP is present in cf_IP");

  std::size_t deviceWidth = 0;
  bool roleNeutralDeviceIds = true;
  for (const auto &id : deviceVertices) {
    if (deviceWidth == 0) {
      deviceWidth = id.size();
    }
    roleNeutralDeviceIds = roleNeutralDeviceIds && id.starts_with("D") &&
                           !id.starts_with("FD") && !id.starts_with("LD") &&
                           id.size() == deviceWidth;
  }
  check(roleNeutralDeviceIds,
        "device ids share one opaque prefix and fixed width; owner role is "
        "not encoded");
  std::printf(
      "  card view: %zu rows, %zu flagged; ground truth "
      "card=%zu party=%zu device=%zu ip=%zu\n",
      payments.rows, payments.matching, groundTruthRowsFor(capture, "card"),
      groundTruthRowsFor(capture, "party"),
      groundTruthRowsFor(capture, "device"), groundTruthRowsFor(capture, "ip"));
  // Printed, not banded: this leg is 100 people over 7 days, so coverage
  // is whatever the endpoint registry hash happens to select and a band
  // here would be pinning a hash, not a mechanism. The COVERAGE LEVELS
  // are gated where they mean something — test_card_endpoint_graph, on a
  // world with a fraud population.
  std::printf("  endpoint registry: Has_Device %zu, Has_IP %zu (device "
              "vertices %zu, ip vertices %zu); merchant register: "
              "Is_Merchant %zu of %zu merchant vertices\n",
              hasDeviceRows.rows, hasIpRows.rows,
              columnValues(capture, "Device", 0).size(),
              columnValues(capture, "IP", 0).size(), isMerchantRows.rows,
              columnValues(capture, "Merchant", 0).size());

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
