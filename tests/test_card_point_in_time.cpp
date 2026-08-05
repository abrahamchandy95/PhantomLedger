//
// tests/test_card_point_in_time.cpp
//
// card-fraud-realism-v2 ROUND 2: THE POINT-IN-TIME FEATURE CONTRACT —
// minimum-realism gate 4 of docs/card_fraud_online_gnn.md:
//
//   "No feature may depend on information that did not exist at the
//    row's own timestamp."
//
// The contract itself is docs/card_fraud_feature_contract.md. This is
// its executable half.
//
// THE EXPERIMENT (a truncation experiment, which is the only way to
// test this property without a second implementation): generate ONE
// world, then export it TWICE through the same code path —
//
//   FULL    the streaming sink sees every settled row
//   PREFIX  the streaming sink sees only rows with ts < T, where T is a
//           mid-window cutoff. This is exactly the information a model
//           scoring at T could have had.
//
// Every feature-safe attribute of a row or entity that exists in the
// PREFIX export must be BYTE-IDENTICAL in the FULL export. If appending
// future events changes an earlier value, that value is a leak: at
// training time it silently carries the future, and at serving time it
// cannot be reproduced.
//
// WHY THIS IS NOT VACUOUS: before the v2 label round, `Card.is_fraud`
// meant "this card ever carried a flagged row over the whole window". A
// card first defrauded AFTER T rendered 0 in the PREFIX export and 1 in
// the FULL export — this gate would have failed on that column, which
// is precisely the defect it now protects against regressing.
//
// FOUR COMPARISON CLASSES, and choosing the right one per table is the
// whole design:
//
//   STREAM PREFIX   the five streamed transaction tables (payment,
//                   sender, receiver, device and IP). The PREFIX
//                   export's lines must be a byte-exact PREFIX of the
//                   FULL export's — strongest form available.
//   IDENTICAL       world-derived tables (Party, the PII layer) cannot
//                   depend on the transaction prefix at all.
//   KEYED           tables whose FIRST COLUMN is a unique entity id
//                   with attributes after it (Card, City,
//                   Merchant_Assigned). The row SET may grow; a row
//                   present in both must be identical. THIS is the
//                   class that catches a full-window entity label.
//   LINE SUBSET     pure edge / single-column tables (Party_Has_Card,
//                   Merchant, the geo edges). A row is fully determined
//                   by its endpoints, so every score-time line must
//                   appear verbatim in the full-window export.
//
// A NOTE ON THAT LAST CLASS, because getting it wrong cost a round: the
// first version of this gate keyed EVERY growing table by its first
// column and reported 244 "changed" rows in Party_Has_Card. Nothing had
// changed — a party owns several cards (one credit card plus a derived
// debit card per account), so the first column is NOT a unique key
// there, and `artifacts.cards` being a std::map over entity::Key means
// a newly-seen debit account inserts BEFORE an existing credit card and
// reorders that party's rows. The keyed comparison below now VERIFIES
// first-column uniqueness and tells the reader to reclassify rather
// than inventing a leak.
//
// EXCLUDED BY DESIGN: cf_Ground_Truth_Label. It is a full-window
// investigative overlay and IS future-dependent — that is why it lives
// outside the feature graph. This gate prints how much it moves as
// standing evidence for why it may never be joined into features.
//

#include "phantomledger/exporter/card_fraud/export.hpp"
#include "phantomledger/exporter/card_fraud/streaming.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include "gate_world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pl = ::PhantomLedger;

namespace {

constexpr std::uint64_t kSeed = 7;
constexpr std::int32_t kPopulation = 300;
constexpr int kDays = 365;

// The cutoff sits at 60% of the settled stream: late enough that the
// prefix has real history, early enough that a meaningful amount of
// fraud lands AFTER it (which is what makes the Card check bite).
constexpr double kCutoffFraction = 0.60;

int g_failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

class Capture final : public pl::exporter::common::TableCapture {
public:
  void put(std::string_view stem, const char *data, std::size_t size) override {
    tables_[std::string{stem}].append(data, size);
  }

  [[nodiscard]] std::vector<std::string> lines(const std::string &stem) const {
    std::vector<std::string> out;
    const auto it = tables_.find(stem);
    if (it == tables_.end()) {
      return out;
    }
    std::string_view text{it->second};
    while (!text.empty()) {
      const auto nl = text.find('\n');
      auto line = text.substr(0, nl);
      text = nl == std::string_view::npos ? std::string_view{}
                                          : text.substr(nl + 1);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      if (!line.empty()) {
        out.emplace_back(line);
      }
    }
    return out;
  }

  [[nodiscard]] bool has(const std::string &stem) const {
    return tables_.contains(stem);
  }

private:
  std::map<std::string, std::string> tables_;
};

// ------------------------------------------------------------- the world

[[nodiscard]] pl::time::Window window() {
  pl::time::Window w;
  w.start = pl::time::makeTime({1991, 1, 1});
  w.days = kDays;
  return w;
}

[[nodiscard]] pl::pipeline::SimulationResult
runWorld(const pl::synth::pii::PoolSet &poolSet,
         const pl::synth::people::Fraud &fraudProfile) {
  const auto w = window();

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = kPopulation,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = w.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(kSeed);
  pl::pipeline::SimulationPipeline pipeline{rng, w, entities, kSeed};
  pipeline.transferStage()
      .legit()
      .window(w)
      .seed(kSeed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

// One export of a given prefix of the settled stream, through exactly
// the production code path (streaming sink -> shared finisher).
void exportPrefix(const pl::pipeline::SimulationResult &result,
                  const pl::synth::pii::PoolSet &poolSet,
                  std::span<const pl::transactions::Transaction> txns,
                  Capture &capture) {
  pl::exporter::card_fraud::StreamingCardFraudExport sink({
      .registry = &result.holdings.accounts.registry,
      .lookup = &result.holdings.accounts.lookup,
      .membership = pl::synth::personas::join_cohort::membershipOf(
          result.people.personas, window()),
      .cards = &result.holdings.creditCards,
      .merchants = &result.counterparties.merchants,
      .pgMirror = nullptr,
      .capture = &capture,
  });
  sink.append(txns);
  sink.finish();

  pl::exporter::card_fraud::Options opts{};
  opts.piiPools = &poolSet;
  opts.window = window();
  opts.capture = &capture;
  (void)pl::exporter::card_fraud::exportFromArtifacts(result, opts,
                                                      sink.takeArtifacts());
}

// ------------------------------------------------------------ the checks

[[nodiscard]] std::string keyOf(const std::string &line) {
  const auto comma = line.find(',');
  return comma == std::string::npos ? line : line.substr(0, comma);
}

/// IDENTICAL: world-derived, so the transaction prefix cannot touch it.
void expectIdentical(const Capture &full, const Capture &prefix,
                     const std::string &stem) {
  const auto a = full.lines(stem);
  const auto b = prefix.lines(stem);
  if (a == b) {
    return;
  }
  check(false, stem + " must not depend on the transaction prefix at all (" +
                   std::to_string(a.size()) + " lines full vs " +
                   std::to_string(b.size()) + " lines prefix)");
}

/// KEYED: first column is a unique entity id, attributes follow. The row
/// SET may grow with arriving rows; a row present in both must be
/// identical. This is the class that catches a full-window entity label.
void expectKeyedStable(const Capture &full, const Capture &prefix,
                       const std::string &stem) {
  const auto a = full.lines(stem);
  const auto b = prefix.lines(stem);
  if (a.empty() || b.empty()) {
    check(false, stem + " rendered no lines in one of the two exports");
    return;
  }
  check(a.front() == b.front(), stem + " header differs between exports");

  std::map<std::string, std::string> byKey;
  for (std::size_t i = 1; i < a.size(); ++i) {
    byKey.emplace(keyOf(a[i]), a[i]);
  }
  // SELF-CHECK: this comparison is only meaningful when the first column
  // identifies exactly one row. Misclassifying a one-to-many table here
  // manufactures phantom "changes" out of row reordering.
  if (byKey.size() + 1 != a.size()) {
    check(false, stem + ": first column is NOT a unique key (" +
                     std::to_string(byKey.size()) + " distinct of " +
                     std::to_string(a.size() - 1) +
                     " rows) — classify it as a LINE SUBSET table, not a "
                     "keyed one");
    return;
  }

  std::size_t missing = 0;
  std::size_t changed = 0;
  std::string firstChange;
  for (std::size_t i = 1; i < b.size(); ++i) {
    const auto it = byKey.find(keyOf(b[i]));
    if (it == byKey.end()) {
      ++missing;
      continue;
    }
    if (it->second != b[i]) {
      ++changed;
      if (firstChange.empty()) {
        firstChange = "\n    at score time: " + b[i] +
                      "\n    full window:  " + it->second;
      }
    }
  }
  check(missing == 0, stem + ": " + std::to_string(missing) +
                          " row(s) present at score time vanished from the "
                          "full-window export (row sets may only GROW)");
  check(changed == 0,
        stem + ": " + std::to_string(changed) +
            " row(s) CHANGED when future events arrived — that attribute "
            "carries the future and is not feature-safe" +
            firstChange);
}

/// LINE SUBSET: a pure edge (or single-column) table. The row IS its
/// endpoints, so there is no attribute to drift — every score-time line
/// must still exist verbatim over the full window. A row that changed
/// shows up here as a row that vanished, which is equally fatal.
void expectLineSubset(const Capture &full, const Capture &prefix,
                      const std::string &stem) {
  const auto a = full.lines(stem);
  const auto b = prefix.lines(stem);
  if (a.empty() || b.empty()) {
    check(false, stem + " rendered no lines in one of the two exports");
    return;
  }
  check(a.front() == b.front(), stem + " header differs between exports");

  const std::set<std::string> fullSet{a.begin(), a.end()};
  std::size_t vanished = 0;
  std::string firstVanished;
  for (std::size_t i = 1; i < b.size(); ++i) {
    if (!fullSet.contains(b[i])) {
      ++vanished;
      if (firstVanished.empty()) {
        firstVanished = "\n    at score time: " + b[i] +
                        "\n    (absent from the full-window export)";
      }
    }
  }
  check(vanished == 0,
        stem + ": " + std::to_string(vanished) +
            " row(s) present at score time do not exist over the full "
            "window — an edge changed as future events arrived" +
            firstVanished);
  check(b.size() <= a.size(), stem + " shrank as rows were added (" +
                                  std::to_string(b.size()) + " > " +
                                  std::to_string(a.size()) + ")");
}

/// STREAM PREFIX: the strongest form — the score-time export must be a
/// byte-exact prefix of the full-window export.
void expectStreamPrefix(const Capture &full, const Capture &prefix,
                        const std::string &stem) {
  const auto a = full.lines(stem);
  const auto b = prefix.lines(stem);
  if (a.empty() || b.empty()) {
    check(false, stem + " rendered no lines in one of the two exports");
    return;
  }
  check(b.size() <= a.size(),
        stem + " has MORE rows at score time than over the full window (" +
            std::to_string(b.size()) + " > " + std::to_string(a.size()) + ")");
  const auto bound = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < bound; ++i) {
    if (a[i] != b[i]) {
      check(false, stem + " line " + std::to_string(i) +
                       " differs between the score-time and full-window "
                       "exports:\n    at score time: " +
                       b[i] + "\n    full window:  " + a[i]);
      return;
    }
  }
  check(b.size() > 1, stem + " carries no data rows at score time — the "
                             "cutoff produced nothing to compare");
}

[[nodiscard]] std::size_t overlayCards(const Capture &capture) {
  std::size_t out = 0;
  const auto rows = capture.lines("Ground_Truth_Label");
  for (std::size_t i = 1; i < rows.size(); ++i) {
    if (rows[i].starts_with("card,")) {
      ++out;
    }
  }
  return out;
}

} // namespace

int main() {
  try {
    const auto poolSet = pltest::buildPoolSet(kSeed);
    const auto fraudProfile = pltest::scaledFraudProfile();

    std::printf("point-in-time: building one 300-person 365-day 1991 world "
                "...\n");
    std::fflush(stdout);
    const auto result = runWorld(poolSet, fraudProfile);

    const auto &posted = result.transfers.ledger.posted.txns;
    check(posted.size() > 1000,
          "the world must settle a meaningful corpus, got " +
              std::to_string(posted.size()) + " rows");
    if (g_failures != 0) {
      return 1;
    }

    // The cutoff is a TIMESTAMP, not an index: the settled stream is
    // replay-sorted ascending, so "every row before T" is a prefix, and
    // taking whole tie groups keeps the cut reproducible.
    const auto pivot = static_cast<std::size_t>(
        static_cast<double>(posted.size()) * kCutoffFraction);
    const auto cutoffTs = posted[pivot].timestamp;
    std::size_t prefixCount = 0;
    while (prefixCount < posted.size() &&
           posted[prefixCount].timestamp < cutoffTs) {
      ++prefixCount;
    }
    check(prefixCount > 0 && prefixCount < posted.size(),
          "the cutoff must split the stream, got " +
              std::to_string(prefixCount) + " of " +
              std::to_string(posted.size()));
    if (g_failures != 0) {
      return 1;
    }

    Capture full;
    Capture prefix;
    exportPrefix(result, poolSet,
                 std::span<const pl::transactions::Transaction>{posted}, full);
    exportPrefix(result, poolSet,
                 std::span<const pl::transactions::Transaction>{posted.data(),
                                                                prefixCount},
                 prefix);

    std::printf("  corpus %zu rows; score-time cut at %zu (%.0f%%)\n",
                posted.size(), prefixCount,
                100.0 * static_cast<double>(prefixCount) /
                    static_cast<double>(posted.size()));

    // ---------------------------------------------- the streamed tables
    for (const char *stem :
         {"Payment_Transaction", "Card_Send_Transaction",
          "Merchant_Receive_Transaction", "Transaction_Uses_Device",
          "Transaction_Uses_IP"}) {
      expectStreamPrefix(full, prefix, stem);
    }

    // ------------------------------------------------- world-derived
    for (const char *stem :
         {"Party", "Address", "Phone", "Email", "ID", "Full_Name", "DOB",
          "Has_Address", "Has_Phone", "Has_Email", "Has_ID", "Has_DOB",
          "Has_Full_Name", "Has_Device", "Has_IP", "Merchant_Category",
          // party-geography-2026-07: home area is assigned once at PII
          // synthesis, so these belong in the STRICTEST class — byte
          // identity between the full window and any prefix. A party whose
          // exported home geography moved as later rows arrived would be
          // acausal, and unlike the merchant side there is no growing
          // vertex set to excuse it: the roster is fixed.
          "Has_Std_City", "Has_Std_Postcode", "Has_Std_State"}) {
      expectIdentical(full, prefix, stem);
    }

    // --------------------- growing row sets with per-entity attributes
    // Card is the one that matters: card_number is unique and is_fraud
    // sits beside it, so a restored full-window label fails HERE.
    // merchant-coordinates-2026-07: `Merchant_Location` belongs HERE, not
    // in the line-subset list below, and the difference is load-bearing.
    // Its row set grows exactly as `Merchant` does, but merchant_id is a
    // UNIQUE key within it (one area centroid per merchant), so the keyed
    // check applies — and it is the stronger one: it fails if a merchant's
    // coordinate ever differs between the prefix and the full window,
    // which is precisely the acausality a geography attribute could
    // smuggle in. The coordinate is world state (`Record.location` is
    // assigned once in G1c), so it must be prefix-invariant.
    for (const char *stem : {"Card", "Merchant_Assigned", "City",
                             "Merchant_Location", "Device", "IP"}) {
      expectKeyedStable(full, prefix, stem);
    }

    // -------------------------------- growing edge / identifier tables
    // Party_Has_Card is one-to-many (a party owns a credit card plus a
    // derived debit card per account), so it is a line subset, not a
    // keyed table.
    // merchant-ownership-2026-07: `Is_Merchant` joins this list rather
    // than the world-derived one above, and the distinction is the whole
    // point. The register itself IS world state, but it is emitted only
    // for merchants OBSERVED IN THE VIEW — because `Merchant` is a
    // stream-derived growing vertex set, and an edge to a vertex a prefix
    // has not written yet is a dangling edge the loader rejects. So the
    // row SET grows exactly as `Merchant` does while every row it does
    // emit is stable.
    for (const char *stem :
         {"Merchant", "Is_Merchant", "Party_Has_Card", "Has_City", "Has_State",
          "Has_Zip",
          "Assigned_To", "Located_In", "State", "Zipcode"}) {
      expectLineSubset(full, prefix, stem);
    }

    // ------------------------------- the quarantine, measured not gated
    const auto overlayFull = overlayCards(full);
    const auto overlayPrefix = overlayCards(prefix);
    std::printf("  ground-truth overlay (EXCLUDED from features): %zu "
                "ever_fraud cards at score time vs %zu over the full "
                "window\n",
                overlayPrefix, overlayFull);
    check(overlayPrefix <= overlayFull,
          "the ever-fraud overlay may only grow with arriving rows");
    if (overlayFull > overlayPrefix) {
      std::printf("  ^ future-dependent by %zu cards, which is exactly why "
                  "it lives outside the feature graph\n",
                  overlayFull - overlayPrefix);
    }

  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d point-in-time check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_point_in_time: every exported feature is reproducible "
              "at score time\n");
  return 0;
}
