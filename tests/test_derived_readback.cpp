//
// tests/test_derived_readback.cpp
//
// aml-txn-edges derived bundle: PostgreSQL read-back parity (skips with
// 77 when no server is reachable; honors PL_TEST_PG).
//
// Builds ONE corpus with a real small-world simulation (scaled fraud
// profile so rings, solo fraudsters and fraud rows exist), appends
// handcrafted rows that deterministically exercise every alert rule
// (below-CTR band, currency CTR, velocity burst) AND the two statutory
// CTR pins of conformance-statutory (2026-07-18, 31 CFR 1010.311):
//
//   boundary  a currency row of exactly $10,000.00 must NOT file a CTR
//             (the statute says strictly MORE THAN $10,000); it lands
//             in the sev-2 band instead (inclusive upper edge)
//   scope     a NON-currency row above $10,000 (merchant channel) must
//             file nothing — CTRs cover transactions in currency only
//             (channels::isCurrency: atm_withdrawal, cash_deposit,
//             fraud_structuring)
//
// Since cash-deposits-2026-07 the sim world itself can file legitimate
// CTRs (business cash takings on the cash_deposit channel occasionally
// exceed $10,000), so CTR counts are pinned STRUCTURALLY, not exactly:
// every CTR must sit strictly above $10,000, CTR records must pair 1:1
// with CTR alerts, and the scope marker uses cents ($15,000.77) that
// bill-rounded cash deposits can never produce.
//
// then constructs the derived Bundle twice:
//
//   corpus  derived::buildBundle over the retained vector
//   pg      readback::buildBundle over the streamed transactions table
//           (queryStreamBounds + one TransactionScan in row_seq order —
//           the scan's channel decode is what keeps the currency rule
//           identical on both paths)
//
// and requires the two bundles to be IDENTICAL, field by field — IDs
// (hash outputs), timestamps, index vectors, and every float bit
// pattern including the 30/90-day aggregate windows. Since the
// aml-txn-edges CSV writers are pure functions of the Bundle, bundle
// identity implies byte-identical rendered tables.
//
// Both engines produce the same `transactions` table content up to
// span_index (row_seq is the cross-engine identity), so this parity
// covers windowed runs too.
//
// SPAN SEMANTICS: the posted corpus is half-open on the requested window.
// Future source rows may participate in final-span cure discovery, but
// settlement retries and generated rows at/after window.endExcl are not
// active corpus rows. The streaming loop below therefore slices every
// span, including the last, at activeWindow.endExcl.
//
// HARD-ENFORCED where it runs.
//

#undef NDEBUG

#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/aml_txn_edges/readback.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace derived = pl::exporter::aml_txn_edges::derived;
namespace readback = pl::exporter::aml_txn_edges::readback;

using pl::exporter::sinks::Postgres;
using pl::pipeline::chunk::Schedule;
using pl::transactions::Transaction;

namespace {

constexpr const char *kTable = "pl_derived_readback";

int failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

[[nodiscard]] std::uint64_t bits(double v) noexcept {
  return std::bit_cast<std::uint64_t>(v);
}

[[nodiscard]] std::int64_t epoch(pl::time::TimePoint tp) {
  return pl::time::toEpochSeconds(tp);
}

// ------------------------------------------------ small world (with fraud)

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;

  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));

  return poolSet;
}

// Same scaling as the window-gate harness: with participation ceiling
// 0.10 * population >= size.min, the first sampleRing() call cannot
// fail, so rings (with mules and victims) exist at any seed.
[[nodiscard]] pl::synth::people::Fraud scaledFraudProfile() {
  pl::synth::people::Fraud profile{};
  profile.rings.perTenKMean = 200.0;
  profile.rings.perTenKSigma = 0.0;
  profile.solos.perTenK = 100.0;
  profile.limits.maxParticipationP = 0.10;
  return profile;
}

[[nodiscard]] pl::pipeline::SimulationResult
runSmallSim(const pl::synth::pii::PoolSet &poolSet, pl::time::Window window,
            const pl::synth::people::Fraud &fraudProfile, std::uint64_t seed) {
  const auto entities = pl::pipeline::stages::entities::EntitySynthesis{
      .population = 120,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

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

// ------------------------------------------------------- bundle compare

template <class Id>
[[nodiscard]] bool idEq(const Id &a, const Id &b) noexcept {
  return a.view() == b.view();
}

void compareBundles(const derived::Bundle &corpus, const derived::Bundle &pg) {
  check(corpus.simStartEpoch == pg.simStartEpoch, "simStartEpoch equal");
  check(corpus.simEndEpoch == pg.simEndEpoch, "simEndEpoch equal");
  check(corpus.simStart == pg.simStart, "simStart equal");
  check(corpus.simEnd == pg.simEnd, "simEnd equal");
  check(idEq(corpus.derivationRunId, pg.derivationRunId),
        "derivationRunId equal");

  check(corpus.alerts.size() == pg.alerts.size(),
        "alert count: corpus " + std::to_string(corpus.alerts.size()) +
            " vs pg " + std::to_string(pg.alerts.size()));
  if (corpus.alerts.size() == pg.alerts.size()) {
    for (std::size_t i = 0; i < corpus.alerts.size(); ++i) {
      const auto &a = corpus.alerts[i];
      const auto &b = pg.alerts[i];
      const bool same = idEq(a.id, b.id) && a.onAccount == b.onAccount &&
                        a.rule == b.rule && a.createdDate == b.createdDate &&
                        a.status == b.status;
      if (!same) {
        check(false, "alert[" + std::to_string(i) + "] diverges (corpus id " +
                         std::string{a.id.view()} + ", pg id " +
                         std::string{b.id.view()} + ")");
        break;
      }
    }
  }

  check(corpus.dispositions.size() == pg.dispositions.size(),
        "disposition count equal");
  if (corpus.dispositions.size() == pg.dispositions.size()) {
    for (std::size_t i = 0; i < corpus.dispositions.size(); ++i) {
      const auto &a = corpus.dispositions[i];
      const auto &b = pg.dispositions[i];
      const bool same =
          idEq(a.id, b.id) && a.alertIndex == b.alertIndex &&
          a.outcome == b.outcome && a.date == b.date &&
          a.investigatorNum == b.investigatorNum &&
          idEq(a.notesHash, b.notesHash) &&
          bits(a.confidence) == bits(b.confidence);
      if (!same) {
        check(false, "disposition[" + std::to_string(i) + "] diverges");
        break;
      }
    }
  }

  check(corpus.ctrs.size() == pg.ctrs.size(), "CTR count equal");
  if (corpus.ctrs.size() == pg.ctrs.size()) {
    for (std::size_t i = 0; i < corpus.ctrs.size(); ++i) {
      const auto &a = corpus.ctrs[i];
      const auto &b = pg.ctrs[i];
      const bool same = idEq(a.id, b.id) && a.onAccount == b.onAccount &&
                        a.filingDate == b.filingDate &&
                        bits(a.amount) == bits(b.amount) &&
                        a.branchBucket == b.branchBucket &&
                        a.tellerNum == b.tellerNum;
      if (!same) {
        check(false, "ctr[" + std::to_string(i) + "] diverges");
        break;
      }
    }
  }

  check(corpus.cases.size() == pg.cases.size(), "case count equal");
  if (corpus.cases.size() == pg.cases.size()) {
    for (std::size_t i = 0; i < corpus.cases.size(); ++i) {
      const auto &a = corpus.cases[i];
      const auto &b = pg.cases[i];
      const bool same =
          idEq(a.id, b.id) && a.kind == b.kind &&
          a.ringOrPerson == b.ringOrPerson && a.openedDate == b.openedDate &&
          a.caseSystem == b.caseSystem &&
          a.subjectPersons == b.subjectPersons &&
          a.alertIndices == b.alertIndices && a.sarIndices == b.sarIndices &&
          a.evidenceIndices == b.evidenceIndices &&
          a.promotedTxnIndices == b.promotedTxnIndices;
      if (!same) {
        check(false, "case[" + std::to_string(i) + "] diverges (id " +
                         std::string{a.id.view()} + ")");
        break;
      }
    }
  }

  check(corpus.evidence.size() == pg.evidence.size(), "evidence count equal");
  if (corpus.evidence.size() == pg.evidence.size()) {
    for (std::size_t i = 0; i < corpus.evidence.size(); ++i) {
      const auto &a = corpus.evidence[i];
      const auto &b = pg.evidence[i];
      const bool same = idEq(a.id, b.id) && a.caseIndex == b.caseIndex &&
                        a.artifactType == b.artifactType &&
                        a.sourceSystem == b.sourceSystem &&
                        idEq(a.contentHash, b.contentHash) &&
                        a.createdAt == b.createdAt;
      if (!same) {
        check(false, "evidence[" + std::to_string(i) + "] diverges");
        break;
      }
    }
  }

  check(corpus.promotedTxns.size() == pg.promotedTxns.size(),
        "promoted-txn count: corpus " +
            std::to_string(corpus.promotedTxns.size()) + " vs pg " +
            std::to_string(pg.promotedTxns.size()));
  if (corpus.promotedTxns.size() == pg.promotedTxns.size()) {
    for (std::size_t i = 0; i < corpus.promotedTxns.size(); ++i) {
      const auto &a = corpus.promotedTxns[i];
      const auto &b = pg.promotedTxns[i];
      const bool same = idEq(a.id, b.id) && a.caseIndex == b.caseIndex &&
                        a.txnIndex == b.txnIndex &&
                        a.promotedAt == b.promotedAt && a.ttlDate == b.ttlDate;
      if (!same) {
        check(false, "promotedTxn[" + std::to_string(i) + "] diverges");
        break;
      }
    }
  }

  check(corpus.businesses.size() == pg.businesses.size(),
        "business count equal");
  if (corpus.businesses.size() == pg.businesses.size()) {
    for (std::size_t i = 0; i < corpus.businesses.size(); ++i) {
      const auto &a = corpus.businesses[i];
      const auto &b = pg.businesses[i];
      const bool same = idEq(a.id, b.id) && a.accountKey == b.accountKey &&
                        a.owner == b.owner && a.stemIdx == b.stemIdx &&
                        a.numberSuffix == b.numberSuffix &&
                        a.entityType == b.entityType &&
                        a.effectiveDate == b.effectiveDate;
      if (!same) {
        check(false, "business[" + std::to_string(i) + "] diverges");
        break;
      }
    }
  }

  const auto compareAgg = [&](const std::vector<derived::AggregateBucket> &a,
                              const std::vector<derived::AggregateBucket> &b,
                              const char *what) {
    check(a.size() == b.size(), std::string{what} + " bucket count: corpus " +
                                    std::to_string(a.size()) + " vs pg " +
                                    std::to_string(b.size()));
    if (a.size() != b.size()) {
      return;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
      const auto &x = a[i];
      const auto &y = b[i];
      const bool same =
          x.pair == y.pair &&
          bits(x.row.totalAmount) == bits(y.row.totalAmount) &&
          x.row.txnCount == y.row.txnCount && x.row.firstTs == y.row.firstTs &&
          x.row.lastTs == y.row.lastTs &&
          bits(x.row.amount30d) == bits(y.row.amount30d) &&
          bits(x.row.amount90d) == bits(y.row.amount90d) &&
          x.row.count30d == y.row.count30d && x.row.count90d == y.row.count90d;
      if (!same) {
        check(false, std::string{what} + "[" + std::to_string(i) +
                         "] diverges (float bits or window counts)");
        break;
      }
    }
  };
  compareAgg(corpus.flowAgg, pg.flowAgg, "flowAgg");
  compareAgg(corpus.linkComm, pg.linkComm, "linkComm");
}

[[nodiscard]] std::size_t countRule(const derived::Bundle &b,
                                    derived::Rule rule) {
  std::size_t n = 0;
  for (const auto &a : b.alerts) {
    if (a.rule == rule) {
      ++n;
    }
  }
  return n;
}

} // namespace

int main() {
  // Probe first: skip cheaply before paying for the simulation.
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  std::optional<pl::postgres::Connection> conn;
  try {
    conn.emplace(conninfo);
  } catch (const std::exception &) {
    std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
    return 77;
  }

  constexpr std::uint64_t seed = 20260717;

  pl::time::Window window;
  window.start = pl::time::makeTime({2025, 1, 1});
  window.days = 30;

  std::printf("derived-readback: building small world (pop 120, 30d, "
              "scaled fraud) ...\n");
  std::fflush(stdout);

  const auto poolSet = buildPoolSet(seed);
  const auto fraudProfile = scaledFraudProfile();
  const auto result = runSmallSim(poolSet, window, fraudProfile, seed);

  const auto &posted = result.transfers.ledger.posted.txns;
  check(!posted.empty(), "world produced transactions");

  std::size_t fraudRows = 0;
  for (const auto &tx : posted) {
    if (tx.fraud.flag != 0) {
      ++fraudRows;
    }
  }
  check(fraudRows > 0, "world contains fraud rows at the scaled profile");

  // The fixture corpus: the posted stream plus handcrafted rows that
  // deterministically exercise below-CTR, currency-CTR and velocity
  // rules plus the two statutory pins (all on one internal account,
  // same second as the last posted row, so replay-nondecreasing
  // timestamps hold).
  std::vector<Transaction> corpus(posted.begin(), posted.end());
  {
    pl::entity::Key probeAcct{};
    for (const auto &tx : posted) {
      if (tx.source.bank == pl::entity::Bank::internal &&
          tx.source.role == pl::entity::Role::account) {
        probeAcct = tx.source;
        break;
      }
    }
    check(probeAcct.number != 0, "found an internal source account");

    const auto merchant = pl::channels::tag(pl::channels::Legit::merchant);
    const auto atm = pl::channels::tag(pl::channels::Legit::atm);

    const auto lastTs = posted.back().timestamp;
    const auto extra = [&](double amount, pl::channels::Tag channel) {
      Transaction t;
      t.source = probeAcct;
      t.target = pl::entity::makeKey(pl::entity::Role::merchant,
                                     pl::entity::Bank::internal, 3);
      t.amount = amount;
      t.timestamp = lastTs;
      t.session.channel = channel;
      return t;
    };

    corpus.push_back(extra(9500.55, merchant));  // band (all-channel CHOICE)
    corpus.push_back(extra(10000.00, atm));      // BOUNDARY: band, NO CTR
    corpus.push_back(extra(12000.01, atm));      // currency CTR + alert
    corpus.push_back(extra(15000.00, atm));      // currency CTR + alert
    corpus.push_back(extra(15000.77, merchant)); // SCOPE: non-currency, silent
    for (int i = 0; i < 6; ++i) { // VELOCITY_BURST (>= 5 same acct/day)
      corpus.push_back(extra(42.42, merchant));
    }
  }

  // Corpus-side bundle. SARs enter both builders as the same span (they
  // come from the aml seams, not the corpus), so an empty span keeps
  // the comparison honest without weakening it.
  const auto corpusBundle = derived::buildBundle(
      result.people, result.holdings,
      std::span<const Transaction>{corpus.data(), corpus.size()}, {});

  std::printf("derived-readback: corpus bundle: alerts=%zu (fraud=%zu "
              "belowCtr=%zu ctr=%zu velocity=%zu) ctrs=%zu cases=%zu "
              "promoted=%zu flow=%zu link=%zu\n",
              corpusBundle.alerts.size(),
              countRule(corpusBundle, derived::Rule::fraudMlFlag),
              countRule(corpusBundle, derived::Rule::highAmountBelowCtr),
              countRule(corpusBundle, derived::Rule::cashCtrThreshold),
              countRule(corpusBundle, derived::Rule::velocityBurst),
              corpusBundle.ctrs.size(), corpusBundle.cases.size(),
              corpusBundle.promotedTxns.size(), corpusBundle.flowAgg.size(),
              corpusBundle.linkComm.size());
  std::fflush(stdout);

  // Every rule must actually fire, or the parity below is vacuous. CTR
  // counts are pinned STRUCTURALLY (the world's own cash deposits may
  // legitimately file since cash-deposits-2026-07): at least the two
  // handcrafted currency rows file; every CTR sits strictly above
  // $10,000 (the exactly-$10,000.00 atm row must not appear — strict
  // statutory boundary); CTR records pair 1:1 with CTR alerts; and the
  // $15,000.77 merchant marker never files (currency scope — its cents
  // are unreachable by bill-rounded cash deposits).
  check(countRule(corpusBundle, derived::Rule::fraudMlFlag) > 0,
        "fraud-ml alerts present");
  check(countRule(corpusBundle, derived::Rule::highAmountBelowCtr) >= 2,
        "below-CTR alerts present (incl. the exactly-$10,000.00 edge)");
  check(countRule(corpusBundle, derived::Rule::cashCtrThreshold) >= 2,
        "the two handcrafted currency rows above $10,000 raise CTR alerts");
  check(corpusBundle.ctrs.size() ==
            countRule(corpusBundle, derived::Rule::cashCtrThreshold),
        "one CTR record per CTR alert");
  check(countRule(corpusBundle, derived::Rule::velocityBurst) >= 1,
        "velocity alerts present");
  for (const auto &c : corpusBundle.ctrs) {
    check(c.amount > 10000.0,
          "every CTR amount strictly above $10,000 (31 CFR 1010.311)");
    check(bits(c.amount) != bits(15000.77),
          "the $15,000.77 merchant row never files (currency scope)");
  }
  check(!corpusBundle.cases.empty(), "cases present");
  check(!corpusBundle.promotedTxns.empty(), "promoted fraud txns present");
  check(!corpusBundle.flowAgg.empty() && !corpusBundle.linkComm.empty(),
        "aggregates present");

  // Stream the same corpus into PostgreSQL, one COPY per span. The
  // corpus is replay-sorted (nondecreasing ts), so spans are contiguous
  // half-open active-window slices.
  {
    const auto sched = Schedule::partition(window, {});
    Postgres sink({.conninfo = conninfo, .table = kTable});
    std::size_t begin = 0;
    for (std::size_t k = 0; k < sched.size(); ++k) {
      const auto &span = sched[k];
      const auto boundExcl = span.activeWindow.endExcl();
      std::size_t end = begin;
      while (end < corpus.size() &&
             pl::time::fromEpochSeconds(corpus[end].timestamp) < boundExcl) {
        ++end;
      }

      sink.beginSpan(span);
      sink.append(
          std::span<const Transaction>{corpus.data() + begin, end - begin});
      sink.endSpan(span);
      begin = end;
    }
    sink.finish();
    check(begin == corpus.size(), "every corpus row streamed exactly once");
    check(sink.rowsWritten() == corpus.size(),
          "sink row count matches the corpus");
  }

  // Read-back bundle over the streamed table — same world, same sars,
  // rows from PostgreSQL.
  const auto pgBundle =
      readback::buildBundle(result.people, result.holdings, {}, *conn, kTable);

  compareBundles(corpusBundle, pgBundle);

  conn->exec(std::string{"DROP TABLE "} + kTable);

  if (failures > 0) {
    std::fprintf(stderr,
                 "\nDERIVED READ-BACK DIVERGES: %d check(s) failed.\n",
                 failures);
    return 1;
  }

  std::printf("derived-readback: corpus and PostgreSQL bundles are "
              "identical (%zu alerts, %zu cases, %zu promoted, %zu+%zu "
              "aggregate buckets; sim end %lld)\n",
              corpusBundle.alerts.size(), corpusBundle.cases.size(),
              corpusBundle.promotedTxns.size(), corpusBundle.flowAgg.size(),
              corpusBundle.linkComm.size(),
              static_cast<long long>(epoch(corpusBundle.simEnd)));
  return 0;
}
