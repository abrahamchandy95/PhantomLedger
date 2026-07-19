//
// tests/test_pg_readback.cpp
//
// Decode contract for the PostgreSQL read-back path (skips with 77 when
// no server is reachable; honors PL_TEST_PG) — the foundation the
// aml-txn-edges derived pass builds on. PostgreSQL is the corpus store
// for bounded-memory runs; this gate pins that a `row_seq`-ordered scan
// reconstructs the stream LOSSLESSLY for every field derived analytics
// consumes:
//
//   KEYS      encoding::parseKey inverts encoding::format over the same
//             layout table (pinned serverless for all 18 role/bank
//             layouts, including the L/LI prefix family)
//   AMOUNT    bit-exact double round-trip (shortest-round-trip text in,
//             extra_float_digits out) — pinned with adversarial values
//             (0.1+0.2, thirds, nextafter neighbors)
//   SUMS      derived::accumulate applied per row in row_seq order over
//             the read-back stream matches the corpus-side fold
//             BIT-FOR-BIT, including the 30/90-day sim-end windows —
//             the property that lets the read-back pass target byte
//             parity instead of a model-version change
//   TS/FRAUD  timestamps invert formatTimestamp exactly; is_fraud
//             round-trips the flag
//   CHANNEL   channels::parse inverts channels::name (validated unique)
//             to the exact Tag — pinned over a channel-diverse fixture
//             (legit, cash, fraud, rent, credit groups) because the
//             currency-scoped CTR rule (31 CFR 1010.311) reads it
//   LIFECYCLE bounds query, empty tables, and an abandoned scan rolling
//             back cleanly with the connection still usable
//
// HARD-ENFORCED where it runs; serverless machines still enforce the
// parseKey pins before skipping.
//
// Assertion discipline: asserts must stay armed (the harness passes
// -UNDEBUG; the #undef below is belt-and-braces against other build
// paths) and must NEVER carry side effects — under -DNDEBUG a
// side-effectful assert silently skips the call, which is exactly how
// the first version of this test left a scan transaction open.
//

#undef NDEBUG

#include "phantomledger/encoding/parse.hpp"
#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/exporter/sinks/txn_readback.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/constants.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace PhantomLedger;
using exporter::sinks::Postgres;
using pipeline::chunk::Schedule;
using transactions::Transaction;

namespace derived = exporter::aml_txn_edges::derived;

namespace {

constexpr const char *kTable = "pl_readback_txns";

[[nodiscard]] std::uint64_t bits(double v) noexcept {
  return std::bit_cast<std::uint64_t>(v);
}

void checkRoundTrip(entity::Role role, entity::Bank bank,
                    std::uint64_t number) {
  const auto key = entity::makeKey(role, bank, number);
  const auto rendered = encoding::format(key);
  const auto parsed = encoding::parseKey(rendered.view());
  assert(parsed.has_value());
  assert(*parsed == key);
}

// Serverless: parseKey must invert format for every generic layout and
// reject malformed ids. Runs before the connectivity probe so these
// pins hold on machines without a server.
void checkParseKeyPins() {
  checkRoundTrip(entity::Role::customer, entity::Bank::internal, 12);
  checkRoundTrip(entity::Role::account, entity::Bank::internal, 987);
  checkRoundTrip(entity::Role::merchant, entity::Bank::internal, 5);
  checkRoundTrip(entity::Role::merchant, entity::Bank::external, 5);
  checkRoundTrip(entity::Role::employer, entity::Bank::internal, 33);
  checkRoundTrip(entity::Role::employer, entity::Bank::external, 33);
  checkRoundTrip(entity::Role::landlord, entity::Bank::internal, 42);
  checkRoundTrip(entity::Role::landlord, entity::Bank::external, 42);
  checkRoundTrip(entity::Role::client, entity::Bank::internal, 8);
  checkRoundTrip(entity::Role::client, entity::Bank::external, 8);
  checkRoundTrip(entity::Role::platform, entity::Bank::external, 3);
  checkRoundTrip(entity::Role::processor, entity::Bank::external, 3);
  checkRoundTrip(entity::Role::family, entity::Bank::external, 77);
  checkRoundTrip(entity::Role::business, entity::Bank::internal, 9);
  checkRoundTrip(entity::Role::business, entity::Bank::external, 0xFFFF'FF01ULL);
  checkRoundTrip(entity::Role::brokerage, entity::Bank::internal, 4);
  checkRoundTrip(entity::Role::brokerage, entity::Bank::external, 4);
  checkRoundTrip(entity::Role::card, entity::Bank::internal, 987654321);

  // The L/LI prefix family must resolve by the digits-only remainder.
  {
    const auto card =
        entity::makeKey(entity::Role::card, entity::Bank::internal, 12345);
    const auto parsed = encoding::parseKey(encoding::format(card).view());
    assert(parsed.has_value());
    assert(parsed->role == entity::Role::card);
  }
  {
    const auto landlord =
        entity::makeKey(entity::Role::landlord, entity::Bank::internal, 42);
    const auto parsed = encoding::parseKey(encoding::format(landlord).view());
    assert(parsed.has_value());
    assert(parsed->role == entity::Role::landlord);
    assert(parsed->bank == entity::Bank::internal);
  }

  assert(!encoding::parseKey("").has_value());
  assert(!encoding::parseKey("Q123").has_value());
  assert(!encoding::parseKey("L").has_value());
  assert(!encoding::parseKey("LIabc").has_value());
  assert(!encoding::parseKey("A00x1").has_value());
  assert(!encoding::parseKey("A0000000000").has_value()); // zero, !allowZero

  std::printf("pg-readback: parseKey pins hold (18 layouts)\n");
  std::fflush(stdout);
}

// Cross-group channel rotation: the decode must round-trip every tag
// family the writer can emit, cash channels included (the currency-
// scoped CTR rule consumes exactly this field).
inline constexpr std::array<channels::Tag, 5> kFixtureChannels{
    channels::tag(channels::Legit::merchant),
    channels::tag(channels::Legit::atm),
    channels::tag(channels::Fraud::structuring),
    channels::tag(channels::Legit::salary),
    channels::tag(channels::Rent::check),
};

[[nodiscard]] std::vector<Transaction> buildFixture(std::int64_t startEpoch,
                                                    std::size_t count) {
  std::vector<Transaction> rows;
  rows.reserve(count);

  const std::int64_t step = (120 * time::kSecondsPerDay) / 1250;

  std::int64_t prevTs = startEpoch;
  for (std::size_t i = 0; i < count; ++i) {
    Transaction tx;

    // Nondecreasing timestamps with deliberate same-second ties.
    std::int64_t ts =
        startEpoch + static_cast<std::int64_t>(i) * step +
        static_cast<std::int64_t>(i % 5);
    if (i % 13 == 0 && i > 0) {
      ts = prevTs;
    }
    prevTs = ts;
    tx.timestamp = ts;

    const auto acct = entity::makeKey(entity::Role::account,
                                      entity::Bank::internal, 100 + (i % 50));
    switch (i % 6) {
    case 0:
      tx.source = acct;
      tx.target = entity::makeKey(entity::Role::merchant,
                                  entity::Bank::internal, 1 + (i % 20));
      break;
    case 1:
      tx.source = acct;
      tx.target = entity::makeKey(entity::Role::merchant,
                                  entity::Bank::external, 1 + (i % 15));
      break;
    case 2:
      tx.source = acct;
      tx.target = entity::makeKey(entity::Role::card, entity::Bank::internal,
                                  1000 + (i % 30));
      break;
    case 3:
      tx.source = entity::makeKey(entity::Role::family, entity::Bank::external,
                                  1 + (i % 25));
      tx.target = acct;
      break;
    case 4:
      tx.source = acct;
      tx.target = entity::makeKey(entity::Role::business,
                                  entity::Bank::external, 0xFFFF'FF01ULL);
      break;
    default:
      tx.source = entity::makeKey(entity::Role::business,
                                  entity::Bank::internal, 1 + (i % 12));
      tx.target = entity::makeKey(entity::Role::brokerage,
                                  entity::Bank::external, 1 + (i % 6));
      break;
    }

    // Adversarial amounts: values whose shortest text is long, whose
    // binary form is non-terminating decimal, and exact ULP neighbors.
    switch (i % 9) {
    case 0:
      tx.amount = 0.1 + 0.2; // 0.30000000000000004
      break;
    case 1:
      tx.amount = 1000.0 / 3.0;
      break;
    case 2:
      tx.amount = std::nextafter(100.0, 200.0);
      break;
    case 3:
      tx.amount = 12345678.90123;
      break;
    case 4:
      tx.amount = 0.01;
      break;
    case 5:
      tx.amount = 987654321.99;
      break;
    default:
      tx.amount = static_cast<double>((i * 9973) % 100000) / 100.0 +
                  0.007 * static_cast<double>(i % 3);
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
    tx.session.channel = kFixtureChannels[i % kFixtureChannels.size()];

    rows.push_back(tx);
  }

  return rows;
}

} // namespace

int main() {
  checkParseKeyPins();

  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  std::optional<postgres::Connection> conn;
  try {
    conn.emplace(conninfo);
  } catch (const std::exception &) {
    std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
    return 77;
  }

  const time::Window run{time::makeTime({2025, 3, 1}), 120};
  const auto startEpoch = time::toEpochSeconds(run.start);

  constexpr std::size_t kRows = 1200;
  const auto rows = buildFixture(startEpoch, kRows);

  // 1. Stream the fixture through the production sink, one COPY per
  //    settlement span, exactly like a windowed run.
  {
    const auto sched = Schedule::partition(run, {});
    assert(sched.size() >= 3);

    Postgres sink({.conninfo = conninfo, .table = kTable});
    std::size_t streamed = 0;
    for (const auto &span : sched) {
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
    sink.finish();
    assert(streamed == kRows);
    assert(sink.rowsWritten() == kRows);
  }

  // 2. Bounds: the cheap first pass that gives the derived pass its sim
  //    window before the sweep.
  const auto bounds = postgres::queryStreamBounds(*conn, kTable);
  assert(bounds.rows == kRows);
  assert(bounds.minTs == rows.front().timestamp);
  assert(bounds.maxTs == rows.back().timestamp);

  const std::int64_t cut30 = bounds.maxTs - 30 * time::kSecondsPerDay;
  const std::int64_t cut90 = bounds.maxTs - 90 * time::kSecondsPerDay;

  // 3. Full ordered scan: every decoded field bit-exact against the
  //    corpus, and the SHARED per-row accumulator (derived::accumulate)
  //    folded over both sides in the same order must agree bit-for-bit.
  {
    derived::AggregateRow fromPg;
    derived::AggregateRow fromCorpus;

    postgres::TransactionScan scan{*conn, kTable};
    postgres::StreamTxnRow row;
    std::size_t i = 0;
    while (scan.next(row)) {
      assert(i < rows.size());
      const auto &tx = rows[i];

      assert(row.rowSeq == i + 1);
      assert(row.source == tx.source);
      assert(row.target == tx.target);
      assert(bits(row.amount) == bits(tx.amount));
      assert(row.timestamp == tx.timestamp);
      assert(row.fraudFlag == tx.fraud.flag);
      assert(row.channel == tx.session.channel);
      assert(row.sourceRendered ==
             std::string_view{encoding::format(tx.source).view()});
      assert(row.targetRendered ==
             std::string_view{encoding::format(tx.target).view()});

      derived::accumulate(fromPg, row.amount, row.timestamp, cut30, cut90);
      derived::accumulate(fromCorpus, tx.amount, tx.timestamp, cut30, cut90);
      ++i;
    }
    assert(i == kRows);
    assert(scan.rowsRead() == kRows);

    assert(bits(fromPg.totalAmount) == bits(fromCorpus.totalAmount));
    assert(bits(fromPg.amount30d) == bits(fromCorpus.amount30d));
    assert(bits(fromPg.amount90d) == bits(fromCorpus.amount90d));
    assert(fromPg.txnCount == fromCorpus.txnCount);
    assert(fromPg.count30d == fromCorpus.count30d);
    assert(fromPg.count90d == fromCorpus.count90d);
    assert(fromPg.firstTs == fromCorpus.firstTs);
    assert(fromPg.lastTs == fromCorpus.lastTs);

    // The 30/90-day cuts must actually bite (window is 120 days).
    assert(fromPg.count30d > 0);
    assert(fromPg.count30d < fromPg.count90d);
    assert(fromPg.count90d < fromPg.txnCount);

    std::printf("pg-readback: %zu rows bit-exact (channels incl.); 30/90-day "
                "sums match (30d=%u 90d=%u total=%u)\n",
                i, fromPg.count30d, fromPg.count90d, fromPg.txnCount);
    std::fflush(stdout);
  }

  // 4. Abandoned scan: destruction mid-stream rolls back the read-only
  //    transaction and leaves the connection usable. (Side effects
  //    hoisted OUT of the asserts.)
  {
    {
      postgres::TransactionScan partial{*conn, kTable};
      postgres::StreamTxnRow row;
      const bool first = partial.next(row);
      assert(first);
      assert(row.rowSeq == 1);
      const bool second = partial.next(row);
      assert(second);
      assert(row.rowSeq == 2);
    }
    const auto count =
        conn->queryValue(std::string{"SELECT count(*) FROM "} + kTable);
    assert(count == std::to_string(kRows));
  }

  // 5. Empty table: bounds report zero rows; a scan ends immediately.
  //    The scan leaves scope BEFORE the DROP so no transaction can
  //    still be holding the table.
  {
    Postgres sink({.conninfo = conninfo, .table = "pl_readback_empty"});
    sink.finish();

    const auto empty = postgres::queryStreamBounds(*conn, "pl_readback_empty");
    assert(empty.rows == 0);

    {
      postgres::TransactionScan scan{*conn, "pl_readback_empty"};
      postgres::StreamTxnRow row;
      const bool any = scan.next(row);
      assert(!any);
    }

    conn->exec("DROP TABLE pl_readback_empty");
  }

  conn->exec(std::string{"DROP TABLE "} + kTable);

  std::puts("pg-readback: all assertions passed");
  return 0;
}
