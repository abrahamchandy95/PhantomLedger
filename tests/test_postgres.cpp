#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/postgres/csv_loader.hpp"

#include <libpq-fe.h>

#include <algorithm>
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

  // 1. Stream through Postgres, one COPY per span.
  {
    Postgres sink({.conninfo = conninfo, .table = "transactions"});
    for (const auto &span : sched) {
      sink.beginSpan(span);
      std::vector<Transaction> slice;
      for (const auto &tx : txns) {
        const auto ts = time::fromEpochSeconds(tx.timestamp);
        if (ts >= span.activeWindow.start && ts < span.activeWindow.endExcl()) {
          slice.push_back(tx);
        }
      }
      sink.append(slice);
      sink.endSpan(span);
    }
    sink.finish();
    assert(sink.rowsWritten() == txns.size());
    assert(sink.spansWritten() == 4);
  }

  postgres::Connection conn{conninfo};

  // 2. Server-side checks.
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
  }

  // 3. Equivalence vs the legacy single-file writer: pull rows back
  //    out and compare. Amount (field 2) compares as a value because
  //    the server re-renders float8 on output ('250.0' in, '250' out);
  //    every other column must match byte-for-byte.
  {
    PGresult *r =
        PQexec(conn.raw(), "COPY transactions TO STDOUT WITH (FORMAT csv)");
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
    assert(pgRows.size() == txns.size());

    namespace fs = std::filesystem;
    const fs::path ref = fs::temp_directory_path() / "pl_pg_ref.csv";
    {
      exporter::csv::Writer w{ref};
      w.writeHeader(exporter::schema::kLedger.header);
      exporter::common::writeLedgerRows(w, txns);
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

    std::sort(pgRows.begin(), pgRows.end());
    std::sort(fileRows.begin(), fileRows.end());
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

  // 4. CopyIn abort-on-destruction: a batch abandoned mid-COPY commits
  //    nothing. This is the exception-safety guarantee for spans.
  {
    {
      postgres::CopyIn copy{
          conn, "COPY transactions (src_acct, dst_acct, amount, ts, is_fraud,"
                " ring_id, device_id, ip_address, channel)"
                " FROM STDIN WITH (FORMAT csv)"};
      const std::string row =
          "X1,X2,1.0,2025-01-01 00:00:00,0,0,d,ip,merchant\n";
      copy.put(row.data(), row.size());
      // scope exit without done() -> server-side abort
    }
    PGresult *r = PQexec(conn.raw(), "SELECT count(*) FROM transactions");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "11");
    PQclear(r);
  }

  // 5. Protocol violations fail loudly.
  {
    Postgres sink({.conninfo = conninfo, .table = "transactions"});
    bool threw = false;
    try {
      sink.append(txns);
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw);
  }

  // 6. CSV directory loader: files stream verbatim into all-text
  //    tables; header-derived columns; quoted commas survive; empty
  //    fields become NULL; skip list honored.
  {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "pl_pg_loader";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
      std::ofstream a{dir / "alpha.csv", std::ios::binary};
      a << "id,label,note\r\n"
        << "1,\"Main St, Apt 4\",x\r\n"
        << "2,plain,\r\n"
        << "3,q,z\r\n";
      std::ofstream t{dir / "transactions.csv", std::ios::binary};
      t << "src_acct,dst_acct\r\nA,B\r\n";
    }

    static constexpr std::string_view kSkip[] = {"transactions"};
    const auto reports = postgres::loadCsvDirectory(
        conn, dir, std::span<const std::string_view>{kSkip});
    assert(reports.size() == 1);
    assert(reports[0].table == "alpha");
    assert(reports[0].rows == 3);

    PGresult *r = PQexec(
        conn.raw(), "SELECT count(*), count(*) FILTER (WHERE note IS NULL), "
                    "(SELECT label FROM alpha WHERE id = '1') FROM alpha");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "3");
    assert(std::string{PQgetvalue(r, 0, 1)} == "1");
    assert(std::string{PQgetvalue(r, 0, 2)} == "Main St, Apt 4");
    PQclear(r);
    conn.exec("DROP TABLE alpha");
  }

  // 7. Tree loader: recursive walk into a schema, relative paths
  //    folded into table names, duplicate header columns suffixed,
  //    leaf-stem skip honored.
  {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "pl_pg_tree";
    fs::remove_all(root);
    fs::create_directories(root / "edges");
    {
      std::ofstream v{root / "party.csv", std::ios::binary};
      v << "id,name\r\n1,ann\r\n2,bo\r\n";
      std::ofstream e{root / "edges" / "same_as.csv", std::ios::binary};
      e << "Customer,Customer,score\r\nA,B,0.9\r\n";
      std::ofstream t{root / "edges" / "transactions.csv", std::ios::binary};
      t << "a,b\r\nx,y\r\n";
    }

    conn.exec("DROP SCHEMA IF EXISTS pl_test_tree CASCADE");
    static constexpr std::string_view kSkip[] = {"transactions"};
    const auto reports = postgres::loadCsvTree(
        conn, root, "pl_test_tree", std::span<const std::string_view>{kSkip});
    assert(reports.size() == 2);
    assert(reports[0].table == "edges_same_as");
    assert(reports[1].table == "party");

    PGresult *r = PQexec(
        conn.raw(),
        "SELECT \"Customer_2\", (SELECT count(*) FROM pl_test_tree.party) "
        "FROM pl_test_tree.edges_same_as");
    assert(PQresultStatus(r) == PGRES_TUPLES_OK);
    assert(std::string{PQgetvalue(r, 0, 0)} == "B");
    assert(std::string{PQgetvalue(r, 0, 1)} == "2");
    PQclear(r);
    conn.exec("DROP SCHEMA pl_test_tree CASCADE");
  }

  std::puts("postgres: all assertions passed");
  return 0;
}
