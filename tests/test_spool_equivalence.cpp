//
// tests/test_spool_equivalence.cpp
//
// Bounded binary candidate spool acceptance (roadmap: first scale
// deliverable). Three layers, cheapest first, so a failure localizes
// before the expensive legs run:
//
//   1. CODEC ROUND-TRIP: hand-built rows with edge-case field values
//      (funds-key ties, uint64-max account numbers, negative zero and
//      subnormal amounts, present/absent optionals, every session field)
//      must decode bit-identical — amounts compared as raw IEEE-754 bits.
//
//   2. CURSOR CONTRACT: emitUntil is strictly-less-than and hold-back
//      correct across repeated bounds; backward bounds throw; a cursor
//      before finish() throws; a second cursor throws; unsorted spooled
//      rows are rejected; the named-path spool removes its file.
//
//   3. PHASE-BOUNDARY EQUIVALENCE: two complete-model legs (base
//      routines, family, products, fraud; 3-month generation windows)
//      differing ONLY in the candidate spool — in-memory vector vs
//      file-backed binary — must produce identical RunFingerprints:
//      Golden digest, row/candidate/fraud counts, drop maps, book hash.
//
// HARD-ENFORCED: the binary spool is a drop-in replacement or it does
// not ship.
//

#include "window_leg_support.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using pltest::Txn;
namespace pl = pltest::pl;
namespace xfer = pltest::xfer;

constexpr std::int64_t kMaxBound = std::numeric_limits<std::int64_t>::max();

[[nodiscard]] Txn makeRow(std::int64_t ts, std::uint64_t src, std::uint64_t dst,
                          double amount) {
  Txn txn;
  txn.source =
      pl::entity::makeKey(pl::entity::Role::account, pl::entity::Bank::internal,
                          src);
  txn.target =
      pl::entity::makeKey(pl::entity::Role::merchant, pl::entity::Bank::external,
                          dst);
  txn.amount = amount;
  txn.timestamp = ts;
  return txn;
}

// Replay-sorted (timestamp, source, target, amount) by construction; the
// first two rows tie on everything but amount to pin tie preservation.
[[nodiscard]] std::vector<Txn> edgeCaseRows() {
  std::vector<Txn> rows;

  auto r0 = makeRow(100, 1, 2, 12.34);
  r0.fraud.flag = 1;
  r0.fraud.ringId = 7;
  r0.fraud.type = pl::fraud::FraudType::launderRing;
  r0.session.deviceId = pl::devices::Identity::person(42, 3);
  r0.session.ipAddress = pl::network::Ipv4::pack(10, 0, 0, 7);
  r0.session.channel = pl::channels::tag(pl::channels::Fraud::classic);
  rows.push_back(r0);

  auto r1 = makeRow(100, 1, 2, 56.0);
  r1.fraud.chainId = 3;
  r1.session.deviceId = pl::devices::Identity::ring(9, 1);
  r1.session.ipAddress = pl::network::Ipv4::pack(192, 168, 1, 1);
  r1.session.channel = pl::channels::tag(pl::channels::Legit::p2p);
  rows.push_back(r1);

  auto r2 =
      makeRow(250, std::numeric_limits<std::uint64_t>::max(), 9, -0.0);
  r2.fraud.ringId = 0xFFFFFFFFU;
  r2.fraud.chainId = 0;
  r2.fraud.type = pl::fraud::FraudType::txnFraudRing;
  rows.push_back(r2);

  auto r3 = makeRow(300, 5, 6, 1e-300);
  r3.session.deviceId = pl::devices::legitShared(5, 2);
  r3.session.channel = pl::channels::tag(pl::channels::Insurance::premium);
  rows.push_back(r3);

  auto r4 = makeRow(4'102'444'800, 7, 8, 9.75e12);
  r4.session.channel = pl::channels::tag(pl::channels::Liquidity::locInterest);
  rows.push_back(r4);

  return rows;
}

void checkRowsBitIdentical(const std::vector<Txn> &expected,
                           const std::vector<Txn> &actual) {
  PL_CHECK(expected.size() == actual.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    PL_CHECK(pl::transactions::detail::auditKey(expected[i]) ==
             pl::transactions::detail::auditKey(actual[i]));
    PL_CHECK(std::bit_cast<std::uint64_t>(expected[i].amount) ==
             std::bit_cast<std::uint64_t>(actual[i].amount));
  }
}

void codecAndCursorUnits() {
  std::printf("  unit: codec round-trip + cursor contract ...\n");
  std::fflush(stdout);

  const auto rows = edgeCaseRows();

  xfer::BinaryCandidateSpool spool;
  spool.append(std::span<const Txn>(rows.data(), 2));
  spool.append(std::span<const Txn>(rows.data() + 2, rows.size() - 2));
  PL_CHECK(spool.rowsWritten() == rows.size());

  // A cursor before finish() is a composition bug.
  bool threw = false;
  try {
    auto premature = spool.openCursor();
    (void)premature;
  } catch (const std::logic_error &) {
    threw = true;
  }
  PL_CHECK(threw);

  spool.finish();
  PL_CHECK(spool.bytesSpooled() ==
           rows.size() * xfer::BinaryCandidateSpool::kRecordBytes);

  const auto cursor = spool.openCursor();
  PL_CHECK(cursor->remaining() == rows.size());

  std::vector<Txn> out;

  // Strictly-less-than: a bound equal to the first timestamp emits nothing.
  cursor->emitUntil(100, out);
  PL_CHECK(out.empty());

  cursor->emitUntil(101, out);
  PL_CHECK(out.size() == 2);

  // Repeated bound: nothing more.
  cursor->emitUntil(101, out);
  PL_CHECK(out.size() == 2);
  PL_CHECK(cursor->emittedTotal() == 2);
  PL_CHECK(cursor->remaining() == rows.size() - 2);

  cursor->emitUntil(251, out);
  PL_CHECK(out.size() == 3);

  // Backward bounds violate the settlement-schedule contract.
  threw = false;
  try {
    cursor->emitUntil(200, out);
  } catch (const std::logic_error &) {
    threw = true;
  }
  PL_CHECK(threw);

  cursor->emitUntil(kMaxBound, out);
  PL_CHECK(out.size() == rows.size());
  PL_CHECK(cursor->remaining() == 0);
  PL_CHECK(cursor->emittedTotal() == rows.size());

  checkRowsBitIdentical(rows, out);

  // The spool is single-cursor by contract.
  threw = false;
  try {
    auto second = spool.openCursor();
    (void)second;
  } catch (const std::logic_error &) {
    threw = true;
  }
  PL_CHECK(threw);

  // Rows that are not replay-sorted must be rejected, mirroring
  // PrecomputedCursorSource's precondition.
  {
    const std::vector<Txn> unsorted{makeRow(500, 1, 1, 1.0),
                                    makeRow(400, 1, 1, 1.0)};
    xfer::BinaryCandidateSpool badSpool;
    badSpool.append(std::span<const Txn>(unsorted.data(), unsorted.size()));
    badSpool.finish();
    const auto badCursor = badSpool.openCursor();
    std::vector<Txn> drain;
    bool rejected = false;
    try {
      badCursor->emitUntil(kMaxBound, drain);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    PL_CHECK(rejected);
  }

  // The named-path spool removes its file on destruction.
  const auto namedPath =
      std::filesystem::temp_directory_path() / "pl_test_spool_named.bin";
  {
    xfer::BinaryCandidateSpool named(namedPath);
    named.append(std::span<const Txn>(rows.data(), rows.size()));
    named.finish();
    PL_CHECK(std::filesystem::exists(namedPath));

    const auto namedCursor = named.openCursor();
    std::vector<Txn> drained;
    namedCursor->emitUntil(kMaxBound, drained);
    checkRowsBitIdentical(rows, drained);
  }
  PL_CHECK(!std::filesystem::exists(namedPath));

  std::printf("  unit: PASS\n");
  std::fflush(stdout);
}

} // namespace

int main() {
  std::printf("=== Binary candidate spool: codec + phase-boundary "
              "equivalence ===\n");

  codecAndCursorUnits();

  constexpr std::uint64_t seed = 20260721;

  pl::time::Window window;
  window.start = pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 3;
  options.settlementLookaheadDays = 35;
  options.threadCount = 1;
  options.withBaseRoutines = true;
  options.withFamily = true;

  pltest::announceLeg("vector spool (reference)");
  const auto reference = pltest::runLeg(poolSet, options);
  pltest::printLeg("vector-spool", reference);

  PL_CHECK(reference.fingerprint.rows > 0);
  PL_CHECK(reference.fingerprint.fraudRows > 0);

  options.useBinarySpool = true;

  pltest::announceLeg("binary file spool");
  const auto spooled = pltest::runLeg(poolSet, options);
  pltest::printLeg("binary-spool", spooled);
  std::printf("  spool file: rows=%llu bytes=%llu (%.1f MiB)\n",
              static_cast<unsigned long long>(spooled.spoolRows),
              static_cast<unsigned long long>(spooled.spoolBytes),
              static_cast<double>(spooled.spoolBytes) / (1024.0 * 1024.0));
  std::fflush(stdout);

  // Every accepted Phase A candidate must have crossed through the file.
  PL_CHECK(spooled.spoolRows == spooled.fingerprint.candidateRows);
  PL_CHECK(spooled.spoolRows > 0);

  pltest::checkLegMatches("binary spool leg", reference, spooled);

  std::printf("BINARY SPOOL EQUIVALENCE HOLDS: the file-backed candidate "
              "spool reproduces the in-memory spool byte-for-byte "
              "(digest %s).\n",
              reference.fingerprint.digest.c_str());

  return 0;
}
