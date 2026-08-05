// loc-accrual-perf-2026-08 — the gate that should have existed since the LOC
// tracker was written.
//
// `LocAccrualTracker::sweep` ran ONCE PER REPLAYED ROW and scanned EVERY
// enabled slot. Rows and accounts are both linear in population, so the
// generator was O(population^2) — measured 2.19x / 3.09x / 3.50x / 3.76x per
// population doubling, converging on 4x, and 98.9% of process samples at the
// owner's 50,000-person run. Nothing in the 58-test suite could see it: every
// existing check asserted that interest was CORRECT, none asserted what it
// COST. That is the same failure mode as `attacker-infra-2026-07`'s "a count
// of endpoints is not a measurement of the graph" — an assertion about the
// answer is not an assertion about the machine that produced it.
//
// Sub-gate A pins the arithmetic against a closed form.
// Sub-gate B pins the lazy integral against a REFERENCE FULL SCAN — the exact
//   algorithm this round replaced — so the speedup cannot silently change the
//   model.
// Sub-gate C pins the billing queue's lazy deletion across disable/re-enable.
// Sub-gate D is the cost gate. It is the only one that would have caught the
//   defect.

#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/transactions/clearing/loc_accrual.hpp"

#include "test_support.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace PhantomLedger;
using clearing::InterestAccrual;
using clearing::LocAccrualTracker;

namespace {

constexpr std::int64_t kDay = time::kSecondsPerDay;
constexpr std::int64_t kPeriod = LocAccrualTracker::kBillingPeriodSeconds;
constexpr double kYearSeconds = LocAccrualTracker::kYearSeconds;

// A deterministic stream — no dependency on the engine's Rng, so this gate
// cannot be perturbed by an unrelated draw-count change.
class Xorshift {
public:
  explicit Xorshift(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() {
    state_ ^= state_ << 13U;
    state_ ^= state_ >> 7U;
    state_ ^= state_ << 17U;
    return state_;
  }
  std::uint32_t below(std::uint32_t bound) {
    return static_cast<std::uint32_t>(next() % bound);
  }
  double unit() {
    return static_cast<double>(next() >> 11U) / 9007199254740992.0;
  }

private:
  std::uint64_t state_;
};

// ---------------------------------------------------------------------------
// The pre-round algorithm, verbatim in structure: integrate every enabled slot
// at every row, then test that slot for maturity. Sub-gate B holds the shipped
// tracker to this.
// ---------------------------------------------------------------------------
class ReferenceScanTracker {
public:
  void initialize(std::uint32_t count) {
    enabled_.assign(count, 0);
    apr_.assign(count, 0.0);
    integral_.assign(count, 0.0);
    lastUpdateTs_.assign(count, 0);
    lastBillingTs_.assign(count, 0);
    enabledIdx_.clear();
  }

  void enable(std::uint32_t idx, double apr) {
    if (enabled_[idx] == 0) {
      enabledIdx_.push_back(idx);
      std::sort(enabledIdx_.begin(), enabledIdx_.end());
    }
    enabled_[idx] = 1;
    apr_[idx] = apr;
    integral_[idx] = 0.0;
    lastUpdateTs_[idx] = 0;
    lastBillingTs_[idx] = 0;
  }

  void update(std::uint32_t idx, double preCash, std::int64_t ts) {
    if (enabled_[idx] == 0 || ts == 0) {
      return;
    }
    const auto lastTs = lastUpdateTs_[idx];
    if (lastTs != 0 && ts > lastTs && preCash < 0.0) {
      integral_[idx] += (-preCash) * static_cast<double>(ts - lastTs);
    }
    lastUpdateTs_[idx] = ts;
  }

  void sweep(std::int64_t ts, const std::vector<double> &cash,
             std::vector<InterestAccrual> &out) {
    for (const std::uint32_t idx : enabledIdx_) {
      update(idx, cash[idx], ts);

      const auto lastBilling = lastBillingTs_[idx];
      if (lastBilling == 0) {
        lastBillingTs_[idx] = ts;
        continue;
      }
      if (ts - lastBilling < kPeriod) {
        continue;
      }
      const double interest = integral_[idx] * apr_[idx] / kYearSeconds;
      if (interest > 0.0) {
        out.push_back(InterestAccrual{
            .accountIndex = idx, .interest = interest, .timestamp = ts});
      }
      integral_[idx] = 0.0;
      lastBillingTs_[idx] = ts;
    }
  }

private:
  std::vector<std::uint8_t> enabled_;
  std::vector<double> apr_;
  std::vector<double> integral_;
  std::vector<std::int64_t> lastUpdateTs_;
  std::vector<std::int64_t> lastBillingTs_;
  std::vector<std::uint32_t> enabledIdx_;
};

// ---------------------------------------------------------------------------
// Sub-gate A — the arithmetic, against a closed form.
//
// A slot held at exactly -$1,000 for one whole billing period at 18% APR owes
// 1000 * 0.18 * 30 / 365.25. Deriving it here rather than restating the
// implementation is the `bls-citation-2026-07` rule: a derivation in a comment
// is not a derivation until someone evaluates it.
// ---------------------------------------------------------------------------
void testAnalyticInterest() {
  LocAccrualTracker tracker;
  tracker.initialize(1);
  tracker.enable(0, 0.18, 1);

  std::vector<double> cash(1, -1000.0);
  std::vector<InterestAccrual> out;

  const std::int64_t t0 = 1'600'000'000;

  // First sweep starts the billing clock and bills nothing.
  tracker.sweep(t0, [&](std::uint32_t i) { return cash[i]; }, out);
  PL_CHECK(out.empty());

  // Exactly one period later, with the balance untouched throughout.
  tracker.sweep(t0 + kPeriod, [&](std::uint32_t i) { return cash[i]; }, out);
  PL_CHECK_EQ(out.size(), 1U);

  const double expected = 1000.0 * 0.18 * 30.0 / 365.25;
  const double relative = std::abs(out[0].interest - expected) / expected;
  std::printf("  A: one period at -$1000 / 18%% APR -> %.10f "
              "(closed form %.10f, rel %.3e)\n",
              out[0].interest, expected, relative);
  PL_CHECK(relative < 1e-12);
  PL_CHECK_EQ(out[0].accountIndex, 0U);
  PL_CHECK_EQ(out[0].timestamp, t0 + kPeriod);

  // A positive balance accrues nothing, and the reset must have cleared the
  // integral rather than carrying it into the next period.
  cash[0] = 500.0;
  out.clear();
  tracker.sweep(t0 + 2 * kPeriod, [&](std::uint32_t i) { return cash[i]; },
                out);
  PL_CHECK(out.empty());
  std::printf("  A: a period spent in credit bills nothing\n");
  std::printf("  PASS: sub-gate A (closed-form interest)\n");
}

// ---------------------------------------------------------------------------
// Sub-gate B — equivalence to the reference full scan.
//
// Both trackers see the SAME posting stream in the SAME order, mirroring
// `drainPending`: sweep at the row instant, then roll the touched slot forward
// on its pre-posting balance, then move cash. The lazy integral sums FEWER,
// LARGER terms for the identical mathematical quantity, so the two agree to
// rounding and not to the bit — that is the whole reason this round moves
// `golden_run.b2sum` while leaving its row count at 186,144.
// ---------------------------------------------------------------------------
void testMatchesReferenceScan() {
  constexpr std::uint32_t kAccounts = 400;
  constexpr int kRows = 120'000;
  constexpr std::int64_t kRowGap = 300; // 5 minutes

  LocAccrualTracker lazy;
  ReferenceScanTracker reference;
  lazy.initialize(kAccounts);
  reference.initialize(kAccounts);

  Xorshift rng(0xC0FFEEULL);
  for (std::uint32_t i = 0; i < kAccounts; ++i) {
    // Half the slots revolve, half stay in credit — the shipped mix measured
    // 24.4% of enabled slots negative at any instant.
    const double apr = 0.12 + 0.18 * rng.unit();
    lazy.enable(i, apr, 1);
    reference.enable(i, apr);
  }

  std::vector<double> cashLazy(kAccounts, 0.0);
  std::vector<double> cashRef(kAccounts, 0.0);
  std::vector<InterestAccrual> outLazy;
  std::vector<InterestAccrual> outRef;

  std::int64_t ts = 1'700'000'000;
  for (int row = 0; row < kRows; ++row) {
    ts += kRowGap;

    lazy.sweep(ts, [&](std::uint32_t i) { return cashLazy[i]; }, outLazy);
    reference.sweep(ts, cashRef, outRef);

    const auto idx = rng.below(kAccounts);
    // Skewed negative so balances genuinely revolve rather than hovering at 0.
    const double delta = (rng.unit() - 0.62) * 400.0;

    lazy.update(idx, cashLazy[idx], ts);
    reference.update(idx, cashRef[idx], ts);
    cashLazy[idx] += delta;
    cashRef[idx] += delta;
  }

  std::printf("  B: %zu billing events over %d rows / %u slots\n",
              outLazy.size(), kRows, kAccounts);
  PL_CHECK(outLazy.size() == outRef.size());
  PL_CHECK(!outLazy.empty());

  double worstRelative = 0.0;
  double sumLazy = 0.0;
  double sumRef = 0.0;
  for (std::size_t i = 0; i < outLazy.size(); ++i) {
    // The billing INSTANT and the slot are exact — only the amount rounds.
    PL_CHECK_EQ(outLazy[i].accountIndex, outRef[i].accountIndex);
    PL_CHECK_EQ(outLazy[i].timestamp, outRef[i].timestamp);
    const double relative = std::abs(outLazy[i].interest - outRef[i].interest) /
                            std::abs(outRef[i].interest);
    worstRelative = std::max(worstRelative, relative);
    sumLazy += outLazy[i].interest;
    sumRef += outRef[i].interest;
  }

  const double aggregate = std::abs(sumLazy - sumRef) / std::abs(sumRef);
  std::printf("  B: worst per-event rel %.3e, aggregate rel %.3e "
              "(total $%.2f vs $%.2f)\n",
              worstRelative, aggregate, sumLazy, sumRef);

  // Float-summation slack only. A missed posting notification — the real
  // hazard, since the lazy integral advances only where it is told to —
  // shows up as a whole missing interval, orders of magnitude above this.
  PL_CHECK(worstRelative < 1e-9);
  PL_CHECK(aggregate < 1e-12);
  std::printf("  PASS: sub-gate B (matches the reference full scan)\n");
}

// ---------------------------------------------------------------------------
// Sub-gate C — lazy deletion in the billing queue.
//
// `disable` cannot erase from the middle of a std::priority_queue, so it drops
// the validity token and leaves the entry to be discarded on sight. Get that
// wrong and a re-enabled slot either double-bills or inherits the previous
// incarnation's billing clock.
// ---------------------------------------------------------------------------
void testDisableReenable() {
  LocAccrualTracker tracker;
  tracker.initialize(2);
  std::vector<double> cash{-1000.0, -1000.0};
  std::vector<InterestAccrual> out;
  const auto cashFn = [&](std::uint32_t i) { return cash[i]; };

  const std::int64_t t0 = 1'600'000'000;
  tracker.enable(0, 0.18, 1);
  tracker.enable(1, 0.18, 1);
  tracker.sweep(t0, cashFn, out);
  PL_CHECK(out.empty());

  // Slot 0 leaves LOC protection mid-period; its queued entry is now stale.
  tracker.disable(0);

  out.clear();
  tracker.sweep(t0 + kPeriod, cashFn, out);
  PL_CHECK_EQ(out.size(), 1U);
  PL_CHECK_EQ(out[0].accountIndex, 1U);
  std::printf("  C: a disabled slot's stale queue entry does not bill\n");

  // Re-enable: the clock must restart here, NOT resume from t0. Billing one
  // period after the re-enable sweep must produce exactly one period of
  // interest, not two.
  tracker.enable(0, 0.18, 1);
  out.clear();
  tracker.sweep(t0 + kPeriod + 1, cashFn, out);
  PL_CHECK(out.empty());

  out.clear();
  tracker.sweep(t0 + 2 * kPeriod + 1, cashFn, out);
  bool sawZero = false;
  for (const auto &a : out) {
    if (a.accountIndex == 0) {
      sawZero = true;
      const double expected = 1000.0 * 0.18 * 30.0 / 365.25;
      const double relative = std::abs(a.interest - expected) / expected;
      std::printf("  C: re-enabled slot bills %.6f for one period "
                  "(closed form %.6f, rel %.3e)\n",
                  a.interest, expected, relative);
      PL_CHECK(relative < 1e-12);
    }
  }
  PL_CHECK(sawZero);
  std::printf("  PASS: sub-gate C (disable / re-enable)\n");
}

// ---------------------------------------------------------------------------
// Sub-gate D — THE COST GATE.
//
// This is the check whose absence cost the owner a four-day run. It asserts
// that sweeping N rows does not scale with the number of ENABLED SLOTS.
//
// The band is an absolute budget rather than a ratio, deliberately. Billing
// EVENTS scale linearly with slot count and always will — 16x the slots is 16x
// the maturities — so a ratio would confound real work with the defect. The
// pre-round scan cost rows x slots: the 16,000-slot leg below is 4.8e9 slot
// visits, tens of seconds on any machine. The shipped path does 300,000 heap
// probes plus ~112,000 maturities and measures in tens of milliseconds. The
// budget sits between them with roughly two orders of magnitude of headroom on
// each side, so it is not a timing race — it separates two regimes, and the
// measured value is PRINTED so drift is visible before it fails.
// ---------------------------------------------------------------------------
double timeSweeps(std::uint32_t accounts, int rows, std::int64_t rowGap) {
  LocAccrualTracker tracker;
  tracker.initialize(accounts);
  Xorshift rng(0xA11CE5ULL);
  for (std::uint32_t i = 0; i < accounts; ++i) {
    tracker.enable(i, 0.18, 1);
  }

  std::vector<double> cash(accounts, -500.0);
  std::vector<InterestAccrual> out;
  std::int64_t ts = 1'700'000'000;

  const auto start = std::chrono::steady_clock::now();
  for (int row = 0; row < rows; ++row) {
    ts += rowGap;
    tracker.sweep(ts, [&](std::uint32_t i) { return cash[i]; }, out);
    const auto idx = rng.below(accounts);
    tracker.update(idx, cash[idx], ts);
    cash[idx] -= 1.0;
    out.clear();
  }
  const auto stop = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(stop - start).count();
}

void testSweepCostIsIndependentOfSlotCount() {
  constexpr int kRows = 300'000;
  constexpr std::int64_t kRowGap = 60;
  constexpr double kBudgetSeconds = 3.0;

  const double small = timeSweeps(1'000, kRows, kRowGap);
  const double large = timeSweeps(16'000, kRows, kRowGap);

  std::printf("  D: %d rows x  1,000 slots -> %.4f s\n", kRows, small);
  std::printf("  D: %d rows x 16,000 slots -> %.4f s  (ratio %.2fx, "
              "budget %.1f s)\n",
              kRows, large, small > 0.0 ? large / small : 0.0, kBudgetSeconds);
  std::printf("  D: pre-round scan would visit %.2e slots on the large leg\n",
              static_cast<double>(kRows) * 16000.0);

  PL_CHECK(large < kBudgetSeconds);
  std::printf("  PASS: sub-gate D (sweep cost does not scale with slots)\n");
}

} // namespace

int main() {
  std::printf("=== LOC Accrual Tests (loc-accrual-perf-2026-08) ===\n");
  testAnalyticInterest();
  testMatchesReferenceScan();
  testDisableReenable();
  testSweepCostIsIndependentOfSlotCount();
  std::printf("All LOC accrual tests passed.\n\n");
  return 0;
}
