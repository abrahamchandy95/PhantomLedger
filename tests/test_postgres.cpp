//
// tests/test_postgres.cpp
//
// Live-PostgreSQL pinning for the transaction stream (skips with 77 when
// no server is reachable; honors PL_TEST_PG for a custom conninfo).
//
// Pinned rules (roadmap: PostgreSQL end-game, identity/ordering step):
//
//   IDENTITY   every streamed row lands exactly once, carrying a 1-based
//              global ordinal (row_seq) and its settlement span
//              (span_index); row_seq is contiguous 1..N
//   ORDERING   `ORDER BY row_seq` reconstructs the exact stream — the
//              read-back contract derived-analytics passes depend on;
//              rows are compared POSITIONALLY against the legacy ledger
//              writer, byte-for-byte (amount as a value: the server
//              re-renders float8 on output)
//   SPANS      each row's span_index matches the span whose COPY
//              committed it
//   RESTART    an abandoned mid-COPY span commits nothing (crash loses
//              only the open span); constructing a new sink on the same
//              table is a full rewrite (drop + recreate)
//
// The csv_loader directory/tree sections retired with the loader itself
// (CSV retirement, step 5c): direct-table parity is structural — the
// TableMirror COPYs the writer's own rendered bytes.
//

#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"

#include <libpq-fe.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace PhantomLedger;
using exporter::sinks::Postgres;
using pipeline::chunk::Schedule;
using transactions::Transaction;

namespace {

time::TimePoint at(int y, unsigned m, unsigned d, int h = 10) {
  return time::makeTime({y, m, d}, {h, 0, 0});
}

Transaction makeTx(std::uint64_t src, std::uint64_t dst, double amount,
                   time::TimePoint ts, bool fraud) {
  Transaction tx;
  tx.source =
      entity::makeKey(entity::Role::account, entity::Bank::internal, src);
  tx.target =
      entity::makeKey(entity::Role::merchant, entity::Bank::internal, dst);
  tx.amount = amount;
  tx.timestamp = time::toEpochSeconds(ts);
  tx.fraud.flag = fraud ? 1 : 0;
  if (fraud) {
    tx.fraud.ringId = 7;
    tx.fraud.chainId = 3;
  }
  tx.session.deviceId = devices::Identity::person(src, 1);
  tx.session.ipAddress =
      network::Ipv4::pack(10, 0, 0, static_cast<std::uint8_t>(src));
  tx.session.channel = channels::tag(channels::Legit::merchant);
  return tx;
}

std::vector<std::string> splitCsv(const std::string &line) {
  // Fixture contains no embedded commas or quotes; naive split is safe.
  std::vector<std::string> out;
  std::stringstream ss{line};
  std::string field;
  while (std::getline(ss, field, ',')) {
    out.push_back(field);
  }
  return out;
}

} // namespace

int main() {
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  { // connectivity probe; skip cleanly when no server is available
    PGconn *probe = PQconnectdb(conninfo.c_str());
    const bool ok = probe != nullptr && PQstatus(probe) == CONNECTION_OK;
    PQfinish(probe);
    if (!ok) {
      std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
      return 77;
    }
  }

  std::vector<Transaction> txns;
  txns.push_back(makeTx(101, 501, 12.34, at(2025, 1, 16), false));
  txns.push_back(makeTx(102, 502, 250.00, at(2025, 1, 28), true));
  txns.push_back(makeTx(103, 503, 9.99, at(2025, 2, 2), false));
  txns.push_back(makeTx(104, 504, 1200.50, at(2025, 2, 14), false));
  txns.push_back(makeTx(105, 505, 77.10, at(2025, 2, 27), true));
  txns.push_back(makeTx(106, 506, 3.15, at(2025, 3, 1), false));
  txns.push_back(makeTx(107, 507, 480.00, at(2025, 3, 18), false));
  txns.push_back(makeTx(108, 508, 66.60, at(2025, 3, 31, 23), true));
  txns.push_back(makeTx(109, 509, 15.25, at(2025, 4, 2), false));
  txns.push_back(makeTx(110, 510, 890.00, at(2025, 4, 9), false));
  { // real streams contain device-less rows; empty CSV field -> SQL NULL
    auto bare = makeTx(111, 511, 5.00, at(2025, 4, 10), false);
    bare.session.deviceId = devices::Identity{};
    txns.push_back(bare);
  }

  time::Window run{time::makeTime({2025, 1, 15}), 90};
  const auto sched = Schedule::partition(run, {});
  assert(sched.size() == 4);

  // 1. Stream through Postgres, one COPY per span, recording the stream
  //    order and the span that committed each row.
  std::vector<Transaction> streamedOrder;
  std::vector<unsigned> expectedSpans;
  {
    Postgres sink({.conninfo = conninfo, .table = "transactions"});
    for (const auto &span : sched) {
      sink.beginSpan(span);
      std::vector<Transaction> slice;
      for (const auto &tx : txns) {
        const auto ts = time::fromEpochSeconds(tx.timestamp);
        if (ts >= span.activeWindow.start && ts < span.activeWindow.endExcl()) {
          slice.push_back(tx);
          streamedOrder.push_back(tx);
          expectedSpans.push_back(span.index);
        }
      }
      sink.append(slice);
      sink.endSpan(span);
    }
    sink.finish();
    assert(sink.rowsWritten() == txns.size());
    assert(sink.spansWritten() == 4);
  }
  assert(streamedOrder.size() == txns.size());

  postgres::Connection conn{conninfo};

  // 2. Server-side identity checks: counts, and the row_seq ordinal is
  //    contiguous 1..N with all four spans attributed.
  {
    PGresult *r = PQexec(
        conn.raw(), "SELECT count(*), sum(is_fraud), count(DISTINCT channel) "
                    "FROM transactions");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "11");
    assert(std::string{PQgetvalue(r, 0, 1)} == "3");
    assert(std::string{PQgetvalue(r, 0, 2)} == "1");
    PQclear(r);

    r = PQexec(conn.raw(),
               "SELECT count(*) FROM transactions WHERE device_id IS NULL");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "1");
    PQclear(r);

    r = PQexec(conn.raw(),
               "SELECT min(row_seq), max(row_seq), count(DISTINCT row_seq), "
               "count(DISTINCT span_index) FROM transactions");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "1");
    assert(std::string{PQgetvalue(r, 0, 1)} == "11");
    assert(std::string{PQgetvalue(r, 0, 2)} == "11");
    assert(std::string{PQgetvalue(r, 0, 3)} == "4");
    PQclear(r);
  }

  // 3. Span attribution: each row's span_index is the span whose COPY
  //    committed it, in stream order.
  {
    PGresult *r = PQexec(conn.raw(),
                         "SELECT span_index FROM transactions "
                         "ORDER BY row_seq");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(static_cast<std::size_t>(PQntuples(r)) == expectedSpans.size());
    for (int i = 0; i < PQntuples(r); ++i) {
      assert(std::string{PQgetvalue(r, i, 0)} ==
             std::to_string(expectedSpans[static_cast<std::size_t>(i)]));
    }
    PQclear(r);
  }

  // 4. Ordered read-back identity vs the legacy single-file writer:
  //    `ORDER BY row_seq` must reconstruct the exact stream, compared
  //    POSITIONALLY (no sorting). Amount (field 2) compares as a value
  //    because the server re-renders float8 on output ('250.0' in,
  //    '250' out); every other column must match byte-for-byte.
  {
    PGresult *r = PQexec(
        conn.raw(),
        "COPY (SELECT src_acct, dst_acct, amount, ts, is_fraud, ring_id, "
        "fraud_type, device_id, ip_address, channel FROM transactions "
        "ORDER BY row_seq) TO STDOUT WITH (FORMAT csv)");
    assert(PQresultStatus(r) == PGRES_COPY_OUT);
    PQclear(r);
    std::vector<std::string> pgRows;
    char *buf = nullptr;
    int n = 0;
    while ((n = PQgetCopyData(conn.raw(), &buf, 0)) > 0) {
      std::string line{buf, static_cast<std::size_t>(n)};
      if (!line.empty() && line.back() == '\n') {
        line.pop_back();
      }
      pgRows.push_back(std::move(line));
      PQfreemem(buf);
    }
    while (PGresult *tail = PQgetResult(conn.raw())) {
      PQclear(tail);
    }
    assert(pgRows.size() == streamedOrder.size());

    namespace fs = std::filesystem;
    const fs::path ref = fs::temp_directory_path() / "pl_pg_ref.csv";
    {
      exporter::csv::Writer w{ref};
      w.writeHeader(exporter::schema::kLedger.header);
      exporter::common::writeLedgerRows(w, streamedOrder);
    }
    std::vector<std::string> fileRows;
    std::ifstream in{ref};
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back(); // legacy Writer emits RFC 4180 CRLF rows
      }
      fileRows.push_back(line);
    }
    assert(fileRows.size() == pgRows.size());

    for (std::size_t i = 0; i < pgRows.size(); ++i) {
      const auto a = splitCsv(fileRows[i]);
      const auto b = splitCsv(pgRows[i]);
      assert(a.size() == 10 && b.size() == 10);
      for (std::size_t f = 0; f < 10; ++f) {
        if (f == 2) {
          assert(std::stod(a[f]) == std::stod(b[f]));
        } else {
          assert(a[f] == b[f]);
        }
      }
    }
  }

  // 5. CopyIn abort-on-destruction: a batch abandoned mid-COPY commits
  //    nothing. This is the exception-safety guarantee for spans: a
  //    crash loses only the open span, never committed ones.
  {
    {
      postgres::CopyIn copy{
          conn,
          "COPY transactions (row_seq, span_index, src_acct, dst_acct, "
          "amount, ts, is_fraud, ring_id, device_id, ip_address, channel)"
          " FROM STDIN WITH (FORMAT csv)"};
      const std::string row =
          "999,0,X1,X2,1.0,2025-01-01 00:00:00,0,0,d,ip,merchant\n";
      copy.put(row.data(), row.size());
      // scope exit without done() -> server-side abort
    }
    PGresult *r = PQexec(conn.raw(), "SELECT count(*) FROM transactions");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "11");
    PQclear(r);
  }

  // 6. Protocol violations fail loudly, and constructing a sink on the
  //    same table is a FULL REWRITE (drop + recreate): the restart
  //    semantic for a rerun.
  {
    Postgres sink({.conninfo = conninfo, .table = "transactions"});
    bool threw = false;
    try {
      sink.append(txns);
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw);

    PGresult *r = PQexec(conn.raw(), "SELECT count(*) FROM transactions");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "0");
    PQclear(r);
  }

  std::puts("postgres: all assertions passed");
  return 0;
}
