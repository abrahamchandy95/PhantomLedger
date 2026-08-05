//
// tests/test_table_golden.cpp
//
// The TABLE-DIGEST GOLDEN + the FRAUD-VISIBLE PIN: falsifiability
// carried by PostgreSQL content (skips with 77 when no server is
// reachable; honors PL_TEST_PG). The binary runs with no file flags —
// PhantomLedger writes no files. THREE sections, each with its own
// baseline file:
//
//   SECTION "standard" — the run golden's exact config (standard use
//   case, pop 2000, 60 days, seed 3405691582), digesting the corpus
//   stream (row_seq order, bookkeeping columns included — the
//   load-bearing corpus pin) plus every direct table the run wrote in
//   the public schema.
//   Baseline: tests/golden_tables.md5.
//
//   SECTION "fraud" — aml-txn-edges at a FRAUD-DENSE config (pop
//   10000, 60 days, seed 7, start 1991-01-01; ring density
//   probe-verified 2026-07-18 at this pop/seed: structuring rows
//   spanning the widened F1 band, derived shell scores, SARs, alerts,
//   CTRs, cases). This section exists because the standard-config
//   goldens are structurally blind to fraud-LABEL tables (the
//   fraud-audit-2026-07 coverage finding): ShellAccount, SAR and the
//   SAR edges, Alert/CTR/Disposition/InvestigationCase exist only in
//   the aml use cases. The gate HARD-REQUIRES those tables to be
//   present in the pin, and also pins the fraud-dense corpus itself
//   (the shared public.transactions stream, which this section's run
//   overwrites — hence it runs AFTER the standard section is
//   digested). Baseline: tests/golden_tables_aml.md5.
//
//   SECTION "card_fraud" — the SAME fraud-dense config as the fraud
//   section, under --usecase card-fraud (card-fraud T4). The config is
//   deliberately NOT the run golden's: sampleRingCount has no floor
//   (rings.hpp: max(0, round(lognormal * pop/1e4))), so a pop-2000
//   seed can deterministically draw ZERO rings — and with no rings
//   there are no victims and no unauthorized card fraud. The
//   fraud-dense config carries rings, and the flag-1 hard gate below
//   re-verifies fraud visibility on every run, so the card view's
//   fraud-visibility is a fact of the config, not a gamble on a
//   seed. Pins every cf_* table the run registers in schema card_fraud
//   (37 since the point-in-time session round: the 34-table TF_GNN_v3
//   set, the quarantined cf_Ground_Truth_Label overlay, and timestamped
//   cf_Transaction_Uses_Device/cf_Transaction_Uses_IP edges),
//   HARD-REQUIRES its core tables, HARD-REQUIRES flag-1 rows in
//   cf_Payment_Transaction, HARD-REQUIRES one device and one IP edge
//   per payment, and HARD-REQUIRES that the four full-window entity
//   label columns are WITHHELD while their verdicts survive in the
//   overlay. cf_Has_Device/cf_Has_IP ownership is HARD-REQUIRED to be
//   NON-EMPTY as of attacker-infra-2026-07 — the inverse of what this
//   file demanded for four rounds; see the check itself for what changed
//   in the generator and where the anti-shortcut claim now lives.
//   Because the config is IDENTICAL to
//   the fraud section's and exporters are export-side only (main.cpp
//   tees every use case's exporter alongside the same corpus sink),
//   this section also enforces that its
//   public.transactions digest line EQUALS the fraud section's — the
//   corpus stream is use-case-invariant, and this is where that law is
//   pinned. Runs strictly LAST (its run overwrites the shared stream
//   table too). Baseline: tests/golden_tables_card_fraud.md5.
//
// ERA LOCK (macro-history-v1 H0.6, owner directive #3): card-fraud is
// time-locked to the pinned economic era (EMBEDDED in
// synth/econ/era_data.hpp; 1990 through the measured frontier — 2024
// since the H1 coverage extension), so BOTH fraud-dense sections run
// at --start 1991-01-01 — they previously generated at the 2025
// default start, OUT of the era the card-fraud use case models (the
// confirmed axis inconsistency the lock round fixed). The two sections
// share one config because the corpus-invariance assertion compares
// their stream digests, so they moved in-era TOGETHER, re-pinning
// golden_tables_aml.md5 + golden_tables_card_fraud.md5 in the H0.6
// named round. The standard section is era-agnostic and keeps the
// default start.
//
// TABLE DISCOVERY: each section's table list comes from the
// DIRECT-TABLE REGISTRY (public.pl_direct_tables) that the run's own
// TableMirrors populate — the run itself declares what it wrote, and
// the registry is rewritten per schema by each run, so the gate is
// immune to stray tables in a shared database. The corpus stream table
// is digested explicitly with row_seq ordering.
//
// First live run captures a missing baseline (reported as SKIP so
// capture is explicit — a captured baseline belongs in git IMMEDIATELY,
// per-server like the run golden is per-toolchain); every later run
// enforces exact equality. Delete a baseline to re-pin after an
// intentional model change, and re-pin EVERY baseline the change
// touches in the SAME named commit.
//
// PROMOTION: the fraud section's first intentional-model-change round
// is the table golden's promotion trial (the 2026-07 batch's trial was
// vacuous — the standard config never sees fraud labels).
//

#undef NDEBUG

#include "phantomledger/exporter/card_fraud/schema.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif
#ifndef PL_TABLE_BASELINE
#error "PL_TABLE_BASELINE must be defined (path to the standard baseline)"
#endif
#ifndef PL_TABLE_BASELINE_AML
#error "PL_TABLE_BASELINE_AML must be defined (path to the fraud baseline)"
#endif
#ifndef PL_TABLE_BASELINE_CARD_FRAUD
#error                                                                         \
    "PL_TABLE_BASELINE_CARD_FRAUD must be defined (path to the card baseline)"
#endif

namespace fs = std::filesystem;
namespace sch_card = ::PhantomLedger::exporter::schema::card_fraud;
using PhantomLedger::postgres::Connection;

namespace {

// One digest line per table: "<md5>  <rowcount>  <table>". An empty
// `schema` means the connection's default (public).
[[nodiscard]] std::string digestLine(Connection &conn,
                                     const std::string &schema,
                                     const std::string &table,
                                     bool orderByRowSeq) {
  const auto qualified = schema.empty() ? conn.escapeIdentifier(table)
                                        : conn.escapeIdentifier(schema) + "." +
                                              conn.escapeIdentifier(table);
  const std::string order = orderByRowSeq ? "row_seq" : "t::text";
  const auto hash = conn.queryValue(
      "SELECT coalesce(md5(string_agg(t::text, E'\\n' ORDER BY " + order +
      ")), '') FROM " + qualified + " t");
  const auto rows = conn.queryValue("SELECT count(*) FROM " + qualified);
  return hash + "  " + rows + "  " + table;
}

// The run's own declaration of what it wrote: the direct-table
// registry, rewritten per schema by every run (table_mirror.cpp).
// Registry identifiers never contain newlines. PostgreSQL's ORDER BY follows
// the database collation, so normalize once more with C++ byte ordering: a
// baseline captured under en_US and checked under C must describe the same
// table set in the same order.
[[nodiscard]] std::vector<std::string>
registeredTables(Connection &conn, const std::string &schemaKey) {
  const auto agg = conn.queryValue(
      "SELECT coalesce(string_agg(table_name, E'\\n' ORDER BY table_name), "
      "'') FROM public.pl_direct_tables WHERE schema_name = '" +
      schemaKey + "'");
  std::vector<std::string> out;
  std::stringstream ss{agg};
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty()) {
      out.push_back(line);
    }
  }
  std::ranges::sort(out);
  return out;
}

[[nodiscard]] bool runBinary(const std::string &args, const fs::path &logPath,
                             const std::string &conninfo) {
  const std::string cmd = std::string{"PL_PG='"} + conninfo + "' \"" +
                          PL_BIN_PATH + "\" " + args + " > \"" +
                          logPath.string() + "\" 2>&1";
  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return false;
  }
  return true;
}

enum class Section : int { pass, captured, diverged };

// Capture-if-missing / enforce-if-present, shared by all sections.
[[nodiscard]] Section applyBaseline(const fs::path &baseline,
                                    const std::vector<std::string> &lines,
                                    const char *name) {
  if (!fs::exists(baseline)) {
    std::ofstream out{baseline};
    if (!out) {
      std::fprintf(stderr,
                   "table-golden[%s]: cannot open baseline for write: %s\n",
                   name, baseline.c_str());
      return Section::diverged;
    }
    for (const auto &line : lines) {
      out << line << '\n';
    }
    out.flush();
    if (!out) {
      std::fprintf(stderr, "table-golden[%s]: baseline write FAILED: %s\n",
                   name, baseline.c_str());
      return Section::diverged;
    }
    std::printf("table-golden[%s]: baseline captured (%zu tables) at %s — "
                "commit it in the capture round, not later\n",
                name, lines.size(), baseline.c_str());
    return Section::captured; // reported as SKIP: capture is never a pass
  }

  std::vector<std::string> expected;
  {
    std::ifstream in{baseline};
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty()) {
        expected.push_back(line);
      }
    }
  }

  // Baseline files created before table discovery was normalized can carry a
  // database-collation-specific line order. Table identity is the line set;
  // row identity within each table remains pinned by digestLine(). Comparing
  // sorted copies removes only that irrelevant PostgreSQL locale dependency.
  auto normalizedExpected = expected;
  auto normalizedActual = lines;
  std::ranges::sort(normalizedExpected);
  std::ranges::sort(normalizedActual);

  if (normalizedExpected == normalizedActual) {
    std::printf("table-golden[%s]: %zu tables digest-identical to baseline\n",
                name, lines.size());
    return Section::pass;
  }

  std::fprintf(stderr,
               "table-golden[%s]: POSTGRESQL CONTENT DIVERGES FROM "
               "BASELINE\n",
               name);
  // THE CAP USED TO BE `shown < 10` SHARED ACROSS BOTH LISTS, AND IT
  // TRUNCATED SILENTLY. A section with ten changed tables printed ten
  // `changed-or-new` lines, exhausted the budget, and emitted ZERO
  // `was-in-baseline` lines — so the reader saw new row counts with nothing
  // to compare them against, and any further changed table simply vanished
  // from the report. During merchant-churn-2026-07 that made
  // `cf_Merchant_Location` look UNMOVED while `cf_Has_Zip` had moved, which
  // is impossible (the exporter writes both in the same branch of the same
  // loop) and cost a full diagnostic cycle to unpick.
  //
  // A re-pin decision is made from THIS OUTPUT. Truncating it silently is
  // the same defect class as a gate that bounds coverage without saying so:
  // the report reads as complete when it is not. Each list now has its own
  // budget and every suppressed line is COUNTED and reported.
  constexpr std::size_t kMaxShownPerList = 40;

  const auto report = [&](const char *label,
                          const std::vector<std::string> &from,
                          const std::vector<std::string> &against) {
    std::size_t shown = 0;
    std::size_t suppressed = 0;
    for (const auto &line : from) {
      if (std::find(against.begin(), against.end(), line) != against.end()) {
        continue;
      }
      if (shown < kMaxShownPerList) {
        std::fprintf(stderr, "  %s: %s\n", label, line.c_str());
        ++shown;
      } else {
        ++suppressed;
      }
    }
    if (suppressed > 0) {
      std::fprintf(stderr,
                   "  ... and %zu more %s line(s) SUPPRESSED (cap %zu) — "
                   "raise kMaxShownPerList before deciding a re-pin from "
                   "this report\n",
                   suppressed, label, kMaxShownPerList);
    }
    return shown + suppressed;
  };

  const auto changed = report("changed-or-new", lines, expected);
  const auto missing = report("was-in-baseline", expected, lines);
  std::fprintf(stderr,
               "  divergence totals: %zu changed-or-new, %zu "
               "was-in-baseline\n",
               changed, missing);
  std::fprintf(stderr,
               "if this change was intentional, delete %s and rerun to "
               "re-pin — and re-pin every golden the change touches in the "
               "SAME named commit\n",
               baseline.c_str());
  return Section::diverged;
}

} // namespace

int main() {
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  std::optional<Connection> conn;
  try {
    conn.emplace(conninfo);
  } catch (const std::exception &) {
    std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
    return 77;
  }

  const fs::path tmp = fs::temp_directory_path();

  // ------------------------------------------------------------------
  // SECTION "standard": the run golden's exact config, so the two
  // goldens pin the SAME corpus and their catch-power is directly
  // comparable.
  // ------------------------------------------------------------------
  std::printf("=== table-digest golden (live PostgreSQL) ===\n"
              "  [standard] running binary ...\n");
  std::fflush(stdout);
  if (!runBinary("--population 2000 --days 60 --seed 3405691582",
                 tmp / "pl_table_golden.log", conninfo)) {
    return 1;
  }

  // Discovery via the direct-table registry: exactly the tables this
  // run's mirrors wrote into the public schema (the stream table is
  // handled explicitly with row_seq ordering below).
  const auto stdTables = registeredTables(*conn, "public");
  assert(stdTables.size() >= 10);

  std::vector<std::string> stdLines;
  stdLines.reserve(stdTables.size() + 1);
  stdLines.push_back(
      digestLine(*conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : stdTables) {
    stdLines.push_back(digestLine(*conn, "", table, /*orderByRowSeq=*/false));
  }

  // ------------------------------------------------------------------
  // SECTION "fraud": aml-txn-edges at the fraud-dense config, IN-ERA
  // (--start 1991-01-01; H0.6). Runs strictly AFTER the standard
  // section is digested — this run overwrites the shared
  // public.transactions stream table.
  // ------------------------------------------------------------------
  std::printf("  [fraud] running binary (aml-txn-edges, pop 10000, "
              "start 1991-01-01) ...\n");
  std::fflush(stdout);
  if (!runBinary("--usecase aml-txn-edges --population 10000 --days 60"
                 " --seed 7 --start 1991-01-01",
                 tmp / "pl_table_golden_aml.log", conninfo)) {
    return 1;
  }

  // Registry discovery for the dedicated aml_txn_edges schema; names
  // carry the TableMirror prefixing verbatim
  // ("aml_txn_edges_<subdir>_<stem>", stems verbatim incl. case).
  const auto fraudTables = registeredTables(*conn, "aml_txn_edges");
  assert(fraudTables.size() >= 40);

  // The whole point of this section: the fraud-LABEL tables the
  // standard config never produces MUST be under the pin. Stems are
  // VERBATIM from schema.hpp (SAR.csv / CTR.csv are uppercase).
  for (const char *required :
       {"aml_txn_edges_vertices_ShellAccount", "aml_txn_edges_vertices_SAR",
        "aml_txn_edges_vertices_Alert", "aml_txn_edges_vertices_CTR",
        "aml_txn_edges_vertices_InvestigationCase"}) {
    if (std::find(fraudTables.begin(), fraudTables.end(),
                  std::string{required}) == fraudTables.end()) {
      std::fprintf(stderr,
                   "table-golden[fraud]: required fraud-label table missing "
                   "from the run's registry: %s\n",
                   required);
      return 1;
    }
  }

  std::vector<std::string> fraudLines;
  fraudLines.reserve(fraudTables.size() + 1);
  // The fraud-dense corpus itself (structuring amounts included) —
  // F1-sensitive where the standard config is blind.
  fraudLines.push_back(
      digestLine(*conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : fraudTables) {
    fraudLines.push_back(
        digestLine(*conn, "aml_txn_edges", table, /*orderByRowSeq=*/false));
  }

  // ------------------------------------------------------------------
  // SECTION "card_fraud": the SAME fraud-dense in-era config as the
  // fraud section, under --usecase card-fraud (the era lock REQUIRES
  // an in-era window for this use case; the flag-1 gate below
  // re-verifies fraud visibility at this config on every run). Runs
  // strictly LAST — this run overwrites the shared
  // public.transactions stream table too.
  // ------------------------------------------------------------------
  std::printf("  [card_fraud] running binary (card-fraud, pop 10000, "
              "start 1991-01-01) ...\n");
  std::fflush(stdout);
  if (!runBinary("--usecase card-fraud --population 10000 --days 60"
                 " --seed 7 --start 1991-01-01",
                 tmp / "pl_table_golden_card_fraud.log", conninfo)) {
    return 1;
  }

  // Registry discovery for the card_fraud schema; the mirror prefixes
  // every stem with "cf_" (no subdirs; stems verbatim incl. case).
  //
  // EXACT, against the schema's own `kTableCount`. This was `>= 39` while the
  // export grew to 43, so it could see neither the four added tables nor a
  // table that went MISSING — a lower bound is not a count. The acceptance
  // script's two hard-coded 39s went stale behind exactly that weakness and
  // would have aborted the owner's acceptance run on a correct corpus.
  const auto cardTables = registeredTables(*conn, "card_fraud");
  if (cardTables.size() != sch_card::kTableCount) {
    std::fprintf(stderr,
                 "table-golden[card_fraud]: registry has %zu tables, schema "
                 "declares %zu (schema.hpp kTableCount). Adding or removing a "
                 "table must update that constant AND the two scalar counts "
                 "in docs/card_fraud_postgres_acceptance.sql, which cannot "
                 "include this header.\n",
                 cardTables.size(), sch_card::kTableCount);
    return 1;
  }

  // Core tables MUST be under the pin (the streamed vertex + edges,
  // the card/party layer, the documented header-only gap, and the
  // ground-truth overlay that now holds the withheld entity labels).
  for (const char *required :
       {"cf_Payment_Transaction", "cf_Card_Send_Transaction",
        "cf_Merchant_Receive_Transaction", "cf_Transaction_Uses_Device",
        "cf_Transaction_Uses_IP", "cf_Card", "cf_Party", "cf_Party_Has_Card",
        "cf_Merchant", "cf_Is_Merchant", "cf_Ground_Truth_Label"}) {
    if (std::find(cardTables.begin(), cardTables.end(),
                  std::string{required}) == cardTables.end()) {
      std::fprintf(stderr,
                   "table-golden[card_fraud]: required table missing from "
                   "the run's registry: %s\n",
                   required);
      return 1;
    }
  }

  // FRAUD-VISIBLE: at the fraud-dense config the modeled unauthorized
  // debit-card rail (.60 of the unauthorized mix) and the gift-card
  // scam ride the card_purchase channel, so the view must carry flag-1
  // rows — a blind card view here means the view filter or the fraud
  // attribution broke. Direct-table columns are text (mirror DDL):
  // compare against the rendered '1'.
  {
    const auto qualified = conn->escapeIdentifier("card_fraud") + "." +
                           conn->escapeIdentifier("cf_Payment_Transaction");
    const auto fraudRows = conn->queryValue(
        "SELECT count(*) FROM " + qualified + " WHERE is_fraud = '1'");
    if (fraudRows.empty() || fraudRows == "0") {
      std::fprintf(stderr,
                   "table-golden[card_fraud]: cf_Payment_Transaction carries "
                   "NO flag-1 rows at the fraud-dense config — the card view "
                   "must be fraud-visible (unauthorized debit-card rail + "
                   "gift-card scam ride card_purchase)\n");
      return 1;
    }
  }

  // THE LABEL-LEAK GATE at production scale (card-fraud-realism-v2,
  // gate 1 of docs/card_fraud_online_gnn.md). Card.is_fraud,
  // Party.is_fraud, Device.is_blocked and IP.is_blocked are FULL-WINDOW
  // entity verdicts; exported as features they answer the training
  // question before a model sees a transaction. The columns stay (the
  // owner's TF_GNN_v3 loading jobs map positionally) and must be 0
  // everywhere. Mirror DDL is text: compare the rendered '0'.
  {
    struct LabelColumn {
      const char *table;
      const char *column;
    };
    for (const auto &probe : {LabelColumn{"cf_Card", "is_fraud"},
                              LabelColumn{"cf_Party", "is_fraud"},
                              LabelColumn{"cf_Device", "is_blocked"},
                              LabelColumn{"cf_IP", "is_blocked"}}) {
      const auto qualified = conn->escapeIdentifier("card_fraud") + "." +
                             conn->escapeIdentifier(probe.table);
      const auto leaked =
          conn->queryValue("SELECT count(*) FROM " + qualified + " WHERE " +
                           conn->escapeIdentifier(probe.column) + " <> '0'");
      if (!leaked.empty() && leaked != "0") {
        std::fprintf(stderr,
                     "table-golden[card_fraud]: LABEL LEAK — %s.%s carries a "
                     "full-window verdict on %s rows; it must be withheld (0) "
                     "in the feature graph, with the verdict in "
                     "cf_Ground_Truth_Label\n",
                     probe.table, probe.column, leaked.c_str());
        return 1;
      }
    }

    // Withholding is not deletion. The card view carries flag-1 rows
    // (asserted above), so the overlay must carry the cards they
    // touched.
    const auto overlay = conn->escapeIdentifier("card_fraud") + "." +
                         conn->escapeIdentifier("cf_Ground_Truth_Label");
    const auto everFraud = conn->queryValue(
        "SELECT count(*) FROM " + overlay +
        " WHERE entity_type = 'card' AND label = 'ever_fraud'");
    if (everFraud.empty() || everFraud == "0") {
      std::fprintf(stderr,
                   "table-golden[card_fraud]: cf_Ground_Truth_Label carries no "
                   "ever_fraud cards although the card view has flag-1 rows — "
                   "the entity labels were DELETED, not quarantined\n");
      return 1;
    }
  }

  // ENDPOINT REACHABILITY GATE — INVERTED (attacker-infra-2026-07).
  //
  // This check used to hard-fail if either table carried ANY row, under
  // the heading "STATIC ENDPOINT LEAK", and the reasoning was sound while
  // it lasted: whole-window ownership distinguished customer endpoints
  // from attacker-only endpoints before the first payment, because every
  // customer endpoint had an owner and no attacker endpoint did.
  //
  // The generator no longer produces that asymmetry. Registry coverage is
  // partial (`infra::enrollment`), so legitimate rows sit on endpoints
  // with no Party edge; and a declared share of unauthorized cases runs
  // from the victim's own endpoint or exits through a residential proxy,
  // so fraud sits on endpoints that have one. The residual lift is SIZED
  // and BANDED by tests/test_card_endpoint_graph.cpp, which is where an
  // anti-shortcut claim belongs — a live-database row count cannot
  // measure it.
  //
  // What is hard-failed now is the OPPOSITE condition, because it is the
  // one that silently destroys the use case: with these tables empty and
  // TF_GNN_v3 carrying no transaction->endpoint edge type, cf_Device and
  // cf_IP are isolated vertices and the endpoint layer passes no
  // messages at all.
  for (const char *table : {"cf_Has_Device", "cf_Has_IP"}) {
    const auto qualified = conn->escapeIdentifier("card_fraud") + "." +
                           conn->escapeIdentifier(table);
    const auto rows = conn->queryValue("SELECT count(*) FROM " + qualified);
    if (rows.empty() || rows == "0") {
      std::fprintf(stderr,
                   "table-golden[card_fraud]: UNREACHABLE ENDPOINT LAYER — %s "
                   "is empty. Party is the only path TF_GNN_v3 has to Device "
                   "and IP, so an empty ownership table makes every endpoint "
                   "vertex isolated and the layer inert\n",
                   table);
      return 1;
    }
  }

  std::vector<std::string> cardLines;
  cardLines.reserve(cardTables.size() + 1);
  cardLines.push_back(
      digestLine(*conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : cardTables) {
    cardLines.push_back(
        digestLine(*conn, "card_fraud", table, /*orderByRowSeq=*/false));
  }

  // USE-CASE INVARIANCE OF THE CORPUS: identical config to the fraud
  // section, different --usecase — exporters are export-side only
  // (main.cpp tees each use case's exporter alongside the SAME corpus
  // sink), so the streamed corpus must be byte-identical. Enforced
  // against the fraud section's own digest, not the baseline, so it
  // holds even on capture runs.
  if (cardLines.front() != fraudLines.front()) {
    std::fprintf(stderr,
                 "table-golden[card_fraud]: corpus stream DIVERGES between "
                 "use cases at the same config —\n  aml-txn-edges: %s\n  "
                 "card-fraud:    %s\nthe card view is export-side only and "
                 "must never perturb the corpus\n",
                 fraudLines.front().c_str(), cardLines.front().c_str());
    return 1;
  }

  // ------------------------------------------------------------------
  // Baselines: capture-if-missing (SKIP), enforce-if-present.
  // ------------------------------------------------------------------
  const auto stdResult =
      applyBaseline(fs::path{PL_TABLE_BASELINE}, stdLines, "standard");
  const auto fraudResult =
      applyBaseline(fs::path{PL_TABLE_BASELINE_AML}, fraudLines, "fraud");
  const auto cardResult = applyBaseline(fs::path{PL_TABLE_BASELINE_CARD_FRAUD},
                                        cardLines, "card_fraud");

  if (stdResult == Section::diverged || fraudResult == Section::diverged ||
      cardResult == Section::diverged) {
    return 1;
  }
  if (stdResult == Section::captured || fraudResult == Section::captured ||
      cardResult == Section::captured) {
    return 77;
  }
  std::printf("table-golden: all three sections pinned (corpus via row_seq; "
              "fraud labels via the aml section; the card view + corpus "
              "use-case invariance via the card_fraud section; discovery "
              "via the direct-table registry)\n");
  return 0;
}
