#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/primitives/crypto/blake2b.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace PhantomLedger;
using exporter::sinks::Golden;
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

std::vector<Transaction> fixture() {
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
  return txns;
}

template <class Feed> std::string digestOf(Feed &&feed) {
  Golden g;
  feed(g);
  g.finish();
  return g.digest();
}

} // namespace

int main() {
  const auto txns = fixture();
  time::Window run{time::makeTime({2025, 1, 15}), 90};
  const auto whole = Schedule::unpartitioned(run);
  assert(whole.size() == 1);
  const auto parts = Schedule::partition(run, {});
  assert(parts.size() == 4);

  // A. Baseline: everything in one span, one append.
  const auto base = digestOf([&](Golden &g) {
    g.beginSpan((*whole.begin()));
    g.append(txns);
    g.endSpan((*whole.begin()));
  });
  assert(base.size() == 64);

  // B. Batching invariance: row-by-row appends, same span.
  const auto rowByRow = digestOf([&](Golden &g) {
    g.beginSpan((*whole.begin()));
    for (const auto &tx : txns) {
      g.append(std::span<const Transaction>{&tx, 1});
    }
    g.endSpan((*whole.begin()));
  });
  assert(rowByRow == base);

  // C. Span invariance: the same stream fed through 4 monthly spans.
  //    This equality is the acceptance test the chunked pipeline will
  //    be held to: chunked digest == unpartitioned digest.
  const auto chunked = digestOf([&](Golden &g) {
    for (const auto &span : parts) {
      g.beginSpan(span);
      for (const auto &tx : txns) {
        const auto ts = time::fromEpochSeconds(tx.timestamp);
        if (ts >= span.activeWindow.start && ts < span.activeWindow.endExcl()) {
          g.append(std::span<const Transaction>{&tx, 1});
        }
      }
      g.endSpan(span);
    }
  });
  assert(chunked == base);

  // D. Sensitivity: one cent, or one swap, changes everything.
  {
    auto bent = txns;
    bent[4].amount += 0.01;
    const auto d = digestOf([&](Golden &g) {
      g.beginSpan((*whole.begin()));
      g.append(bent);
      g.endSpan((*whole.begin()));
    });
    assert(d != base);

    auto swapped = txns;
    std::swap(swapped[2], swapped[3]);
    const auto s = digestOf([&](Golden &g) {
      g.beginSpan((*whole.begin()));
      g.append(swapped);
      g.endSpan((*whole.begin()));
    });
    assert(s != base);
  }

  // E. Canonical-bytes cross-check: the digest equals a one-shot
  //    blake2b over the exact bytes the legacy Writer puts in a file
  //    (rows only, CRLF included), proving Golden hashes the
  //    renderer's bytes and nothing else.
  {
    namespace fs = std::filesystem;
    const fs::path ref = fs::temp_directory_path() / "pl_golden_ref.csv";
    {
      exporter::csv::Writer w{ref};
      exporter::common::writeLedgerRows(w, txns);
      w.flush();
    }
    std::ifstream in{ref, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
    std::uint8_t oneShot[Golden::kDigestBytes];
    assert(crypto::blake2b::digest(bytes.data(), bytes.size(), oneShot,
                                   sizeof(oneShot)));
    const auto viaGolden = digestOf([&](Golden &g) {
      g.beginSpan((*whole.begin()));
      g.append(txns);
      g.endSpan((*whole.begin()));
    });
    static constexpr char kHex[] = "0123456789abcdef";
    std::string oneShotHex;
    for (auto b : oneShot) {
      oneShotHex.push_back(kHex[b >> 4U]);
      oneShotHex.push_back(kHex[b & 0x0FU]);
    }
    assert(oneShotHex == viaGolden);
  }

  // F. Misuse fails loudly.
  {
    Golden g;
    bool threw = false;
    try {
      (void)g.digest();
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw);

    threw = false;
    try {
      g.append(txns);
    } catch (const std::logic_error &) {
      threw = true;
    }
    assert(threw);
  }

  std::puts("golden: all assertions passed");
  return 0;
}
