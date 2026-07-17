//
// tests/test_resume.cpp
//
// Checkpoint/resume acceptance (skips with 77 when no server is
// reachable; honors PL_TEST_PG). Determinism IS the checkpoint: an
// interrupted run is resumed by regenerating the stream, verifying the
// already-durable spans (recorded digest + lockstep read-back), and
// skipping their COPYs. This gate pins:
//
//   1. POSITIONAL BYTE-IDENTITY: an interrupted-then-resumed run leaves
//      the transactions table identical (row_seq, span_index, every
//      cell) to an uninterrupted run of the same config, and the
//      regenerated golden digest matches.
//   2. TAIL TRIM: rows durable past the last journaled span (a COPY
//      that landed without its ledger row) are deleted and regenerated.
//   3. SKIP ACCOUNTING: exactly the committed prefix is skipped; the
//      Postgres sink COPYs only the remainder, continuing row_seq.
//   4. DRIFTED CONFIG: a different config hash never matches an
//      interrupted run.
//   5. DIGEST TAMPER: a corrupted span digest in the journal hard-fails
//      the resume (throw), never silently rewrites.
//   6. DURABLE TAMPER: an out-of-band change to a committed row is
//      caught by the lockstep read-back comparison (throw).
//   7. STRUCTURAL DAMAGE: missing rows make prepareResume return false
//      (fall back to a clean rewrite), never throw.
//   8. SUPERSEDE: a fresh run marks every 'running' manifest row
//      superseded, so stale crash records cannot be resumed over
//      another run's rows.
//   9. JOURNAL BOUNDS: span rows are resume state, pruned on
//      completion/failure/supersede — rerunning the program any number
//      of times never accumulates journal bulk (the manifest keeps one
//      small audit row per run; data tables are full rewrites).
//
// Runs against isolated tables (pl_resume_*) so it never touches the
// real pl_run_manifest journal or the shared transactions table.
//
// HARD-ENFORCED where it runs.
//

#undef NDEBUG

#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/exporter/sinks/run_ledger.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/constants.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace PhantomLedger;
using exporter::sinks::GatedSink;
using exporter::sinks::Postgres;
using exporter::sinks::ResumableSpanSink;
using exporter::sinks::ResumePlan;
using exporter::sinks::RunLedger;
using pipeline::chunk::Schedule;
using pipeline::chunk::Tee;
using transactions::Transaction;

namespace {

constexpr const char *kTxnTable = "pl_resume_txns";
constexpr std::size_t kPrefix = 2; // spans committed before the "crash"

constexpr std::uint64_t kSeed = 20260717ULL;
constexpr std::int32_t kPopulation = 999;
constexpr std::int64_t kDays = 120;
constexpr time::CalendarDate kStart{2025, 3, 1};

const RunLedger::Tables kTables{.manifest = "pl_resume_manifest",
                                .spans = "pl_resume_spans"};

// Deterministic synthetic corpus (same recipe as test_pg_readback):
// resume mechanics depend on spans and bytes, not on model semantics.
[[nodiscard]] std::vector<Transaction> buildFixture(std::int64_t startEpoch,
                                                    std::size_t count) {
  std::vector<Transaction> rows;
  rows.reserve(count);

  const std::int64_t step = (120 * time::kSecondsPerDay) / 1250;

  for (std::size_t i = 0; i < count; ++i) {
    Transaction tx;
    tx.timestamp = startEpoch + static_cast<std::int64_t>(i) * step +
                   static_cast<std::int64_t>(i % 5);

    const auto acct = entity::makeKey(entity::Role::account,
                                      entity::Bank::internal, 100 + (i % 50));
    if (i % 3 == 0) {
      tx.source = acct;
      tx.target = entity::makeKey(entity::Role::merchant,
                                  entity::Bank::external, 1 + (i % 15));
    } else {
      tx.source = entity::makeKey(entity::Role::family, entity::Bank::external,
                                  1 + (i % 25));
      tx.target = acct;
    }

    switch (i % 4) {
    case 0:
      tx.amount = 0.1 + 0.2;
      break;
    case 1:
      tx.amount = 1000.0 / 3.0;
      break;
    case 2:
      tx.amount = std::nextafter(100.0, 200.0);
      break;
    default:
      tx.amount = static_cast<double>((i * 9973) % 100000) / 100.0;
      break;
    }

    if (i % 7 == 0) {
      tx.fraud.flag = 1;
      tx.fraud.ringId = static_cast<std::uint32_t>((i % 5) + 1);
    }
    if (i % 11 != 0) {
      tx.session.deviceId = devices::Identity::person(1 + (i % 40), 1);
    }
    tx.session.ipAddress =
        network::Ipv4::pack(10, 0, static_cast<std::uint8_t>(i % 200), 5);
    tx.session.channel = channels::tag(channels::Legit::merchant);

    rows.push_back(tx);
  }
  return rows;
}

// Streams the first `spanCount` spans, slicing rows by activeWindow —
// the same slicing for every leg, so legs differ only in where they
// stop, never in span content.
template <class SinkT>
std::uint64_t streamSpans(SinkT &sink, const Schedule &sched,
                          std::span<const Transaction> rows,
                          std::size_t spanCount, bool finishSink) {
  std::uint64_t streamed = 0;
  for (std::size_t i = 0; i < spanCount; ++i) {
    const auto &span = sched[i];
    sink.beginSpan(span);
    std::vector<Transaction> slice;
    for (const auto &tx : rows) {
      const auto ts = time::fromEpochSeconds(tx.timestamp);
      if (ts >= span.activeWindow.start && ts < span.activeWindow.endExcl()) {
        slice.push_back(tx);
      }
    }
    sink.append(slice);
    sink.endSpan(span);
    streamed += slice.size();
  }
  if (finishSink) {
    sink.finish();
  }
  return streamed;
}

[[nodiscard]] std::string tableMd5(postgres::Connection &conn) {
  return conn.queryValue(
      std::string{"SELECT coalesce(md5(string_agg((t.*)::text, E'\\n' "
                  "ORDER BY row_seq)), '') FROM "} +
      kTxnTable + " t");
}

[[nodiscard]] std::string status(postgres::Connection &conn, long long id) {
  return conn.queryValue("SELECT status FROM " + kTables.manifest +
                         " WHERE id = " + std::to_string(id));
}

[[nodiscard]] std::string spanRows(postgres::Connection &conn, long long id) {
  return conn.queryValue("SELECT count(*) FROM " + kTables.spans +
                         " WHERE manifest_id = " + std::to_string(id));
}

// Begins an interrupted run: kPrefix spans committed, then "crash"
// (no finish, status stays 'running'). Returns the manifest id.
[[nodiscard]] long long
interruptedRun(const std::string &conninfo, RunLedger &ledger,
               const std::string &hash, const Schedule &sched,
               std::span<const Transaction> rows) {
  ledger.supersedeRunning();
  const long long id =
      ledger.beginRun(hash, kSeed, kPopulation, kDays, kStart);

  Postgres pgSink({.conninfo = conninfo, .table = kTxnTable});
  GatedSink<Postgres> gate{.inner = &pgSink};
  ResumableSpanSink<GatedSink<Postgres>> sink{{.inner = &gate,
                                               .copyGate = &gate.open,
                                               .ledger = &ledger,
                                               .manifestId = id,
                                               .plan = nullptr,
                                               .conninfo = conninfo,
                                               .txnTable = kTxnTable}};
  streamSpans(sink, sched, rows, kPrefix, /*finishSink=*/false);
  return id;
}

} // namespace

int main() {
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  std::optional<postgres::Connection> conn;
  try {
    conn.emplace(conninfo);
  } catch (const std::exception &) {
    std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
    return 77;
  }

  conn->exec(std::string{"DROP TABLE IF EXISTS "} + kTxnTable);
  conn->exec("DROP TABLE IF EXISTS " + kTables.manifest);
  conn->exec("DROP TABLE IF EXISTS " + kTables.spans);

  RunLedger ledger{*conn, kTables};
  ledger.ensureTables();
  ledger.ensureTables(); // idempotent, including the migration ALTERs

  const auto hash =
      RunLedger::configHash("windowed", kSeed, kPopulation, kDays, kStart);
  const auto otherHash =
      RunLedger::configHash("windowed", kSeed + 1, kPopulation, kDays, kStart);
  assert(hash != otherHash);

  const time::Window run{time::makeTime(kStart), static_cast<int>(kDays)};
  const auto sched = Schedule::partition(run, {});
  assert(sched.size() >= 3);
  assert(kPrefix < sched.size());

  constexpr std::size_t kRows = 1200;
  const auto rows =
      buildFixture(time::toEpochSeconds(run.start), kRows);

  // ---- 1. Uninterrupted baseline -----------------------------------
  std::string md5A;
  std::string digestA;
  {
    ledger.supersedeRunning();
    const long long id =
        ledger.beginRun(hash, kSeed, kPopulation, kDays, kStart);

    exporter::sinks::Golden golden;
    Postgres pgSink({.conninfo = conninfo, .table = kTxnTable});
    GatedSink<Postgres> gate{.inner = &pgSink};
    Tee tee{golden, gate};
    ResumableSpanSink<decltype(tee)> sink{{.inner = &tee,
                                           .copyGate = &gate.open,
                                           .ledger = &ledger,
                                           .manifestId = id,
                                           .plan = nullptr,
                                           .conninfo = conninfo,
                                           .txnTable = kTxnTable}};
    const auto streamed =
        streamSpans(sink, sched, rows, sched.size(), /*finishSink=*/true);
    assert(streamed == kRows);
    assert(pgSink.rowsWritten() == kRows);
    assert(sink.spansWritten() == sched.size());
    assert(sink.spansSkipped() == 0);

    digestA = golden.digest();
    ledger.finishRun(id, golden.rowsWritten(), digestA);
    md5A = tableMd5(*conn);
    assert(!md5A.empty());
    const auto st = status(*conn, id);
    assert(st == "complete");

    // Journal bounds: completion prunes the run's span rows.
    const auto remaining = spanRows(*conn, id);
    assert(remaining == "0");

    // A completed run must not be resumable.
    const auto none = ledger.findResumable(hash);
    assert(!none.has_value());
  }
  std::printf("resume: baseline captured (%zu spans, md5 %s)\n", sched.size(),
              md5A.substr(0, 12).c_str());
  std::fflush(stdout);

  // ---- 2. Interrupt, then resume to positional byte-identity -------
  {
    const long long idB = interruptedRun(conninfo, ledger, hash, sched, rows);

    // Simulate a span whose COPY landed but whose journal row didn't:
    // clone three early rows past the committed tail.
    auto planPeek = ledger.findResumable(hash);
    assert(planPeek.has_value());
    const auto committedRows = planPeek->rows;
    conn->exec(std::string{"INSERT INTO "} + kTxnTable +
               " SELECT row_seq + " + std::to_string(committedRows) +
               ", span_index, src_acct, dst_acct, amount, ts, is_fraud, "
               "ring_id, fraud_type, device_id, ip_address, channel FROM " +
               kTxnTable + " WHERE row_seq <= 3");

    // Drifted config never matches.
    const auto drifted = ledger.findResumable(otherHash);
    assert(!drifted.has_value());

    auto plan = ledger.findResumable(hash);
    assert(plan.has_value());
    assert(plan->manifestId == idB);
    assert(plan->spans.size() == kPrefix);
    assert(plan->rows == committedRows);

    const bool prepared = ledger.prepareResume(*plan, kTxnTable);
    assert(prepared); // and the bogus tail is now trimmed
    const auto count =
        conn->queryValue(std::string{"SELECT count(*) FROM "} + kTxnTable);
    assert(count == std::to_string(plan->rows));

    exporter::sinks::Golden golden;
    Postgres pgSink({.conninfo = conninfo,
                     .table = kTxnTable,
                     .createTable = false,
                     .truncateFirst = false,
                     .startRowSeq = plan->rows});
    GatedSink<Postgres> gate{.inner = &pgSink};
    Tee tee{golden, gate};
    ResumableSpanSink<decltype(tee)> sink{{.inner = &tee,
                                           .copyGate = &gate.open,
                                           .ledger = &ledger,
                                           .manifestId = idB,
                                           .plan = &*plan,
                                           .conninfo = conninfo,
                                           .txnTable = kTxnTable}};
    const auto streamed =
        streamSpans(sink, sched, rows, sched.size(), /*finishSink=*/true);
    assert(streamed == kRows);
    assert(sink.spansSkipped() == kPrefix);
    assert(sink.spansWritten() == sched.size() - kPrefix);
    assert(pgSink.rowsWritten() == kRows);

    // Regeneration is the same stream, and the table is positionally
    // identical to the uninterrupted baseline — bookkeeping included.
    assert(golden.digest() == digestA);
    ledger.finishRun(idB, golden.rowsWritten(), golden.digest());
    const auto md5B = tableMd5(*conn);
    assert(md5B == md5A);
    const auto st = status(*conn, idB);
    assert(st == "complete");
  }
  std::printf("resume: interrupted run resumed to positional "
              "byte-identity (skipped %zu spans)\n",
              kPrefix);
  std::fflush(stdout);

  // ---- 3. Journal digest tamper => hard error ----------------------
  {
    const long long id = interruptedRun(conninfo, ledger, hash, sched, rows);
    conn->exec("UPDATE " + kTables.spans +
               " SET span_digest = '0badc0de' WHERE manifest_id = " +
               std::to_string(id) + " AND span_index = 1");

    auto plan = ledger.findResumable(hash);
    assert(plan.has_value());
    const bool prepared = ledger.prepareResume(*plan, kTxnTable);
    assert(prepared); // structure intact; the lie is in the digest

    Postgres pgSink({.conninfo = conninfo,
                     .table = kTxnTable,
                     .createTable = false,
                     .truncateFirst = false,
                     .startRowSeq = plan->rows});
    GatedSink<Postgres> gate{.inner = &pgSink};
    ResumableSpanSink<GatedSink<Postgres>> sink{{.inner = &gate,
                                                 .copyGate = &gate.open,
                                                 .ledger = &ledger,
                                                 .manifestId = id,
                                                 .plan = &*plan,
                                                 .conninfo = conninfo,
                                                 .txnTable = kTxnTable}};
    bool threw = false;
    try {
      streamSpans(sink, sched, rows, sched.size(), /*finishSink=*/true);
    } catch (const std::exception &) {
      threw = true;
    }
    assert(threw);
    ledger.markFailed(id);
  }
  std::printf("resume: tampered span digest hard-fails\n");
  std::fflush(stdout);

  // ---- 4. Durable row tamper => lockstep read-back catches it ------
  {
    const long long id = interruptedRun(conninfo, ledger, hash, sched, rows);
    conn->exec(std::string{"UPDATE "} + kTxnTable +
               " SET amount = amount + 1.0 WHERE row_seq = 5");

    auto plan = ledger.findResumable(hash);
    assert(plan.has_value());
    const bool prepared = ledger.prepareResume(*plan, kTxnTable);
    assert(prepared); // counts unchanged; the lie is in a cell

    Postgres pgSink({.conninfo = conninfo,
                     .table = kTxnTable,
                     .createTable = false,
                     .truncateFirst = false,
                     .startRowSeq = plan->rows});
    GatedSink<Postgres> gate{.inner = &pgSink};
    ResumableSpanSink<GatedSink<Postgres>> sink{{.inner = &gate,
                                                 .copyGate = &gate.open,
                                                 .ledger = &ledger,
                                                 .manifestId = id,
                                                 .plan = &*plan,
                                                 .conninfo = conninfo,
                                                 .txnTable = kTxnTable}};
    bool threw = false;
    try {
      streamSpans(sink, sched, rows, sched.size(), /*finishSink=*/true);
    } catch (const std::exception &) {
      threw = true;
    }
    assert(threw);
    ledger.markFailed(id);
  }
  std::printf("resume: tampered durable row caught by lockstep read-back\n");
  std::fflush(stdout);

  // ---- 5. Structural damage => unresumable, never a throw ----------
  {
    const long long id = interruptedRun(conninfo, ledger, hash, sched, rows);
    conn->exec(std::string{"DELETE FROM "} + kTxnTable +
               " WHERE row_seq = 3");

    auto plan = ledger.findResumable(hash);
    assert(plan.has_value());
    const bool prepared = ledger.prepareResume(*plan, kTxnTable);
    assert(!prepared);
    ledger.markFailed(id);
    const auto st = status(*conn, id);
    assert(st == "failed");

    // Journal bounds: failure prunes the run's span rows.
    const auto remaining = spanRows(*conn, id);
    assert(remaining == "0");
  }
  std::printf("resume: structural damage falls back to full rewrite\n");
  std::fflush(stdout);

  // ---- 6. A fresh run supersedes stale crash records ----------------
  {
    const long long stale =
        interruptedRun(conninfo, ledger, hash, sched, rows);
    // Fresh run of the same config: supersede + full rewrite.
    ledger.supersedeRunning();
    const long long id =
        ledger.beginRun(hash, kSeed, kPopulation, kDays, kStart);
    const auto stStale = status(*conn, stale);
    assert(stStale == "superseded");

    // Journal bounds: supersede prunes the stale run's span rows.
    const auto remaining = spanRows(*conn, stale);
    assert(remaining == "0");

    const auto none = ledger.findResumable(hash);
    assert(!none.has_value()); // only this run is 'running', with no spans
    ledger.markFailed(id);     // keep the journal tidy for reruns
  }
  std::printf("resume: fresh runs supersede stale crash records\n");
  std::fflush(stdout);

  conn->exec(std::string{"DROP TABLE "} + kTxnTable);
  conn->exec("DROP TABLE " + kTables.manifest);
  conn->exec("DROP TABLE " + kTables.spans);

  std::puts("resume: all assertions passed");
  return 0;
}
