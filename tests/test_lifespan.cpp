//
// tests/test_lifespan.cpp
//
// macro-history-v1 H3 step 1: the LIFESPAN primitive's meaning gates
// (synth/personas/lifespan.hpp; contract docs/h3_mortality_estate.md).
// The primitive is ZERO-GOLDEN at this step — nothing consumes it —
// so these gates pin the MEANING before the wiring rounds pin the
// realization: determinism, per-person lane isolation, the
// alive-at-start invariant, self-consistency of the simulated
// remaining-lifetime mean against a deterministic expectation
// computed INDEPENDENTLY from the same embedded SSA table, the
// male/female differential, cohort monotonicity, the age-120 cap,
// and mass spread.
//

#include "phantomledger/synth/econ/catalog.hpp"
#include "phantomledger/synth/personas/lifespan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace lsx = pl::synth::personas::lifespan;

namespace {

constexpr std::uint64_t kWorldSeed = 0; // the harness convention
int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] pl::time::TimePoint simStart1991() {
  return pl::time::makeTime(
      pl::time::CalendarDate{.year = 1991, .month = 1, .day = 1});
}

// A cohort of n people who are all exactly `ageYears` old at the 1991
// sim start (birthday on the start day, so the fractional age is 0).
[[nodiscard]] std::vector<pl::time::CalendarDate> cohortDobs(std::size_t n,
                                                             int ageYears) {
  return std::vector<pl::time::CalendarDate>(
      n, pl::time::CalendarDate{.year = 1991 - ageYears,
                                .month = 1,
                                .day = 1});
}

[[nodiscard]] double remainingYearsOf(const lsx::Lifespan &ls,
                                      pl::time::TimePoint simStart) {
  return static_cast<double>(pl::time::toEpochSeconds(ls.death) -
                             pl::time::toEpochSeconds(simStart)) /
         (lsx::kDaysPerYear * 86'400.0);
}

// The deterministic expectation of the SAME model, computed directly
// from the embedded table (independent arithmetic — a real oracle for
// the Monte Carlo mean): E[T] = sum_k P(die in year k)*(k+.5) plus the
// capped residual mass.
[[nodiscard]] double expectedRemainingYears(bool male, double a0) {
  const auto &table = pl::synth::econ::mortality();
  double survival = 1.0;
  double expected = 0.0;
  double k = 0.0;
  while (a0 + k < lsx::kMaxAgeYears) {
    const double qx = std::clamp(
        male ? table.qxMale(a0 + k) : table.qxFemale(a0 + k), 0.0, 1.0);
    expected += survival * qx * (k + 0.5);
    survival *= (1.0 - qx);
    k += 1.0;
  }
  expected += survival * (lsx::kMaxAgeYears - a0 + 0.5);
  return expected;
}

} // namespace

int main() {
  const auto simStart = simStart1991();

  // --- Determinism -----------------------------------------------
  {
    const auto dobs = cohortDobs(64, 40);
    const auto a = lsx::deriveAll(kWorldSeed, simStart, dobs);
    const auto b = lsx::deriveAll(kWorldSeed, simStart, dobs);
    bool equal = a.size() == b.size();
    for (std::size_t i = 0; equal && i < a.size(); ++i) {
      equal = a[i].male == b[i].male && a[i].death == b[i].death;
    }
    check(equal, "deriveAll is deterministic");
  }

  // --- Lane isolation: population size cannot move person k -------
  {
    const auto small = lsx::deriveAll(kWorldSeed, simStart, cohortDobs(50, 40));
    const auto large =
        lsx::deriveAll(kWorldSeed, simStart, cohortDobs(200, 40));
    bool same = true;
    for (std::size_t i = 0; i < small.size(); ++i) {
      same = same && small[i].male == large[i].male &&
             small[i].death == large[i].death;
    }
    check(same, "per-person mortality lanes are population-size independent");
  }

  // --- Alive-at-start invariant across ages -----------------------
  {
    for (const int age : {0, 25, 65, 85, 99, 110}) {
      const auto cohort =
          lsx::deriveAll(kWorldSeed, simStart, cohortDobs(400, age));
      bool allAlive = true;
      for (const auto &ls : cohort) {
        allAlive = allAlive && ls.death > simStart;
      }
      check(allAlive, "cohort aged " + std::to_string(age) +
                          " is alive at sim start (death strictly after)");
    }
  }

  // --- Expectation self-consistency + the sex differential --------
  {
    constexpr std::size_t kN = 6000;
    constexpr int kAge = 65;
    const auto cohort =
        lsx::deriveAll(kWorldSeed, simStart, cohortDobs(kN, kAge));

    double maleSum = 0.0, femaleSum = 0.0;
    std::size_t maleN = 0, femaleN = 0;
    for (const auto &ls : cohort) {
      const double t = remainingYearsOf(ls, simStart);
      if (ls.male) {
        maleSum += t;
        ++maleN;
      } else {
        femaleSum += t;
        ++femaleN;
      }
    }

    const double maleShare = static_cast<double>(maleN) / kN;
    check(maleShare > 0.46 && maleShare < 0.54,
          "latent sex draw is ~50/50 (" + std::to_string(maleShare) + ")");

    const double maleMean = maleSum / static_cast<double>(maleN);
    const double femaleMean = femaleSum / static_cast<double>(femaleN);
    const double expMale = expectedRemainingYears(true, kAge);
    const double expFemale = expectedRemainingYears(false, kAge);

    std::printf("[diag] e%d male sim %.2f vs table %.2f; female sim %.2f vs "
                "table %.2f\n",
                kAge, maleMean, expMale, femaleMean, expFemale);

    check(std::abs(maleMean - expMale) < 0.05 * expMale,
          "male e65 matches the table expectation within 5%");
    check(std::abs(femaleMean - expFemale) < 0.05 * expFemale,
          "female e65 matches the table expectation within 5%");
    check(femaleMean > maleMean + 1.0,
          "the male/female longevity differential is realized");
  }

  // --- Cohort monotonicity ----------------------------------------
  {
    constexpr std::size_t kN = 3000;
    double prevMean = 1e9;
    for (const int age : {30, 65, 85}) {
      const auto cohort =
          lsx::deriveAll(kWorldSeed, simStart, cohortDobs(kN, age));
      double sum = 0.0;
      for (const auto &ls : cohort) {
        sum += remainingYearsOf(ls, simStart);
      }
      const double mean = sum / static_cast<double>(kN);
      std::printf("[diag] mean remaining years at age %d: %.2f\n", age, mean);
      check(mean < prevMean,
            "older cohorts have shorter remaining lifetimes (age " +
                std::to_string(age) + ")");
      prevMean = mean;
    }
  }

  // --- The age-120 cap + mass spread ------------------------------
  {
    constexpr std::size_t kN = 4000;
    const auto cohort =
        lsx::deriveAll(kWorldSeed, simStart, cohortDobs(kN, 65));
    std::size_t early = 0, late = 0;
    bool capped = true;
    for (const auto &ls : cohort) {
      const double t = remainingYearsOf(ls, simStart);
      capped = capped && (65.0 + t) < lsx::kMaxAgeYears + 2.0;
      if (t < 5.0) {
        ++early;
      }
      if (t > 25.0) {
        ++late;
      }
    }
    check(capped, "no death beyond the age-120 cap");
    check(early > 0, "some 65-year-olds die within five years");
    check(late > 0, "some 65-year-olds live past 90");
    std::printf("[diag] 65-cohort deaths: <5y %zu, >25y %zu of %zu\n", early,
                late, kN);
  }

  // --- aliveAt semantics ------------------------------------------
  {
    const auto one = lsx::deriveAll(kWorldSeed, simStart, cohortDobs(1, 80));
    const auto &ls = one.front();
    check(lsx::aliveAt(ls, simStart), "alive at sim start");
    check(!lsx::aliveAt(ls, ls.death), "dead at the death instant");
    check(lsx::aliveAt(ls, pl::time::addDays(ls.death, -1)),
          "alive the day before death");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_lifespan: all gates passed\n");
  return 0;
}
