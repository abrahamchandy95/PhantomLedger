#include "phantomledger/synth/personas/timeline.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

// macro-history-v1 H2 step 1: MEANING gates for the persona-timeline
// primitive (synth/personas/timeline.hpp, authority U-7, contract
// docs/h2_persona_timeline.md). The primitive is UNWIRED — these
// gates pin the statutory FRA schedule, the claiming-age mixture, the
// student/business transition shapes, the seed start-consistency
// invariant, monotone irreversibility, and per-person lane isolation.
// The H2 wiring round adds the corpus-level realization gates.

using namespace PhantomLedger;
namespace tlx = synth::personas::timeline;
using PersonaType = ::PhantomLedger::personas::Type;

namespace {

constexpr std::uint64_t kSeed = 0x5EEDBEEFULL;

const time::TimePoint kSimStart =
    time::makeTime(time::CalendarDate{.year = 1991, .month = 1, .day = 1});

// A deterministic dob for "age A at the 1991 sim start", varied by
// person so month/day structure is exercised. Month stays <= August
// so the age at 1991-01-01 is unambiguous (birthday already passed
// logic stays out of the gate's way: we only need band membership).
[[nodiscard]] time::CalendarDate dobForAge(int age, entity::PersonId p) {
  return time::CalendarDate{
      .year = 1990 - age, // strictly older than `age` at 1991-01-01
      .month = 1 + static_cast<unsigned>(p % 8),
      .day = 1 + static_cast<unsigned>(p % 28),
  };
}

[[nodiscard]] tlx::Timeline deriveOne(const random::RngFactory &factory,
                                      entity::PersonId p, PersonaType seed,
                                      int age) {
  return tlx::derive(factory, tlx::Inputs{
                                  .person = p,
                                  .seed = seed,
                                  .dob = dobForAge(age, p),
                                  .simStart = kSimStart,
                              });
}

[[nodiscard]] int ageMonthsAt(time::CalendarDate dob, time::TimePoint at) {
  const auto cd = time::toCalendarDate(at);
  return (cd.year * 12 + static_cast<int>(cd.month)) -
         (dob.year * 12 + static_cast<int>(dob.month));
}

void testDeterminism() {
  const random::RngFactory a{kSeed};
  const random::RngFactory b{kSeed};
  for (entity::PersonId p = 1; p <= 200; ++p) {
    const auto seed = ::PhantomLedger::personas::kTypes[p % 6];
    const int age = 40; // inside every band except student/retiree
    const auto sane =
        (seed == PersonaType::student)   ? 22
        : (seed == PersonaType::retiree) ? 70
                                         : age;
    const auto ta = deriveOne(a, p, seed, sane);
    const auto tb = deriveOne(b, p, seed, sane);
    PL_CHECK(ta.seed == tb.seed);
    PL_CHECK(ta.working == tb.working);
    PL_CHECK_EQ(time::toEpochSeconds(ta.workStart),
                time::toEpochSeconds(tb.workStart));
    PL_CHECK_EQ(time::toEpochSeconds(ta.businessEnd),
                time::toEpochSeconds(tb.businessEnd));
    PL_CHECK_EQ(time::toEpochSeconds(ta.retirement),
                time::toEpochSeconds(tb.retirement));
  }
  std::printf("  PASS: same (worldSeed, person, inputs) -> identical "
              "timelines\n");
}

void testFraSchedule() {
  // Statutory pins (1983 Amendments; MEASUREMENT).
  PL_CHECK_EQ(tlx::fraMonths(1930), 65 * 12);
  PL_CHECK_EQ(tlx::fraMonths(1937), 65 * 12);
  PL_CHECK_EQ(tlx::fraMonths(1938), 65 * 12 + 2);
  PL_CHECK_EQ(tlx::fraMonths(1940), 65 * 12 + 6);
  PL_CHECK_EQ(tlx::fraMonths(1942), 65 * 12 + 10);
  PL_CHECK_EQ(tlx::fraMonths(1943), 66 * 12);
  PL_CHECK_EQ(tlx::fraMonths(1954), 66 * 12);
  PL_CHECK_EQ(tlx::fraMonths(1955), 66 * 12 + 2);
  PL_CHECK_EQ(tlx::fraMonths(1957), 66 * 12 + 6);
  PL_CHECK_EQ(tlx::fraMonths(1959), 66 * 12 + 10);
  PL_CHECK_EQ(tlx::fraMonths(1960), 67 * 12);
  PL_CHECK_EQ(tlx::fraMonths(1980), 67 * 12);
  std::printf("  PASS: statutory FRA schedule pins\n");
}

void testClaimMapping() {
  const int fra = 66 * 12; // the 1943-1954 cohort

  // Exact boundary behavior of the mixture map.
  PL_CHECK_EQ(tlx::claimAgeMonths(0.0, fra), 62 * 12);
  PL_CHECK_EQ(tlx::claimAgeMonths(0.299, fra), 62 * 12);
  PL_CHECK_EQ(tlx::claimAgeMonths(0.40, fra), fra);
  PL_CHECK_EQ(tlx::claimAgeMonths(0.849, fra), fra);
  PL_CHECK_EQ(tlx::claimAgeMonths(0.95, fra), 70 * 12);
  PL_CHECK_EQ(tlx::claimAgeMonths(0.999, fra), 70 * 12);

  // The two uniform bands stay strictly inside their open intervals.
  for (double u = 0.30; u < 0.40; u += 0.005) {
    const int m = tlx::claimAgeMonths(u, fra);
    PL_CHECK(m >= 63 * 12 && m < fra);
  }
  for (double u = 0.85; u < 0.90; u += 0.0025) {
    const int m = tlx::claimAgeMonths(u, fra);
    PL_CHECK(m > fra && m < 70 * 12);
  }

  // Mass sweep: the mixture integrates to its declared weights.
  int at62 = 0, early = 0, atFra = 0, late = 0, at70 = 0;
  const int n = 100000;
  for (int i = 0; i < n; ++i) {
    const double u = (static_cast<double>(i) + 0.5) / n;
    const int m = tlx::claimAgeMonths(u, fra);
    if (m == 62 * 12) {
      ++at62;
    } else if (m < fra) {
      ++early;
    } else if (m == fra) {
      ++atFra;
    } else if (m < 70 * 12) {
      ++late;
    } else {
      ++at70;
    }
  }
  PL_CHECK_EQ(at62, 30000);
  PL_CHECK_EQ(early, 10000);
  PL_CHECK_EQ(atFra, 45000);
  PL_CHECK_EQ(late, 5000);
  PL_CHECK_EQ(at70, 10000);
  std::printf("  PASS: claiming mixture .30/.10/.45/.05/.10 exact\n");
}

void testStartConsistency() {
  const random::RngFactory factory{kSeed};

  struct Band {
    PersonaType seed;
    int minAge;
    int maxAge;
  };
  // The sampleDob bands (synth/pii/samplers.hpp bandFor).
  const Band bands[] = {
      {PersonaType::student, 16, 34},      {PersonaType::retiree, 65, 99},
      {PersonaType::freelancer, 25, 65},   {PersonaType::smallBusiness, 30, 70},
      {PersonaType::highNetWorth, 35, 80}, {PersonaType::salaried, 22, 65},
  };

  entity::PersonId p = 1;
  for (const auto &band : bands) {
    for (int age = band.minAge; age <= band.maxAge; ++age) {
      const auto tl = deriveOne(factory, p, band.seed, age);
      PL_CHECK(tlx::personaAt(tl, kSimStart) == band.seed);
      ++p;
    }
  }
  std::printf("  PASS: personaAt(simStart) == seed across every band age "
              "(%u persons)\n", static_cast<unsigned>(p - 1));
}

void testRetirementDistribution() {
  const random::RngFactory factory{kSeed};

  // Salaried, age ~45 at the 1991 start: dob year 1945 -> FRA 66y.
  // Every drawn claim lies 17+ years in the future, so no settle
  // clamp binds and the realized dates carry the mixture exactly.
  // Claim-age months from the realized retirement date; the 0-60 day
  // birthday jitter widens each spike by at most two months.
  const int n = 20000;
  int at62 = 0, early = 0, atFra = 0, late = 0, at70 = 0;
  for (entity::PersonId p = 1; p <= n; ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::salaried, 45);
    const int m = ageMonthsAt(dobForAge(45, p), tl.retirement);
    PL_CHECK(m >= 62 * 12 && m <= 70 * 12 + 2);
    if (m <= 62 * 12 + 2) {
      ++at62;
    } else if (m < 66 * 12) {
      ++early;
    } else if (m <= 66 * 12 + 2) {
      ++atFra;
    } else if (m < 70 * 12) {
      ++late;
    } else {
      ++at70;
    }
  }
  const auto frac = [](int c) { return static_cast<double>(c) / n; };
  PL_CHECK(frac(at62) > 0.26 && frac(at62) < 0.34);
  PL_CHECK(frac(early) > 0.06 && frac(early) < 0.14);
  PL_CHECK(frac(atFra) > 0.41 && frac(atFra) < 0.49);
  PL_CHECK(frac(late) > 0.02 && frac(late) < 0.09);
  PL_CHECK(frac(at70) > 0.07 && frac(at70) < 0.13);
  std::printf("  PASS: claim ages 62/early/FRA/late/70 = "
              "%.3f/%.3f/%.3f/%.3f/%.3f\n",
              frac(at62), frac(early), frac(atFra), frac(late), frac(at70));
}

void testWorkerSettleClamp() {
  const random::RngFactory factory{kSeed};

  // Salaried already past most claim draws (age 64, dob 1926, FRA
  // 65y): past claims settle 180-1825 days into the window, in-window
  // claims stand as drawn — either way retirement lands strictly
  // after sim start (the seed-consistency invariant) and everyone has
  // claimed by 70 plus jitter, i.e. well inside eight years.
  const int n = 4000;
  const auto eightYears = time::addDays(kSimStart, 8 * 365);
  for (entity::PersonId p = 1; p <= n; ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::salaried, 64);
    PL_CHECK(tl.retirement > kSimStart);
    PL_CHECK(tlx::personaAt(tl, kSimStart) == PersonaType::salaried);
    PL_CHECK(tlx::personaAt(tl, eightYears) == PersonaType::retiree);
  }
  std::printf("  PASS: past claims settle in-window, all claimed by 70\n");
}

void testStudentTransitions() {
  const random::RngFactory factory{kSeed};

  const int n = 10000;
  int salaried = 0;
  const auto midLife = time::addDays(kSimStart, 15 * 365);
  const auto cohortProbe = time::addDays(kSimStart, 12 * 365);
  for (entity::PersonId p = 1; p <= n; ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::student, 20);
    PL_CHECK(tlx::personaAt(tl, kSimStart) == PersonaType::student);

    // THE H2 directive gate: no 29-year student cohort — every seed
    // student has transitioned long before year 12.
    const auto atProbe = tlx::personaAt(tl, cohortProbe);
    PL_CHECK(atProbe != PersonaType::student);

    const auto atMid = tlx::personaAt(tl, midLife);
    PL_CHECK(atMid == PersonaType::salaried ||
             atMid == PersonaType::freelancer);
    if (tl.working == PersonaType::salaried) {
      ++salaried;
    }

    // Work starts strictly in-window and at a plausible age: never
    // past 30 for a 20-year-old (28-year cap + <=540-day settle).
    PL_CHECK(tl.workStart > kSimStart);
    const int m = ageMonthsAt(dobForAge(20, p), tl.workStart);
    PL_CHECK(m <= 30 * 12);
  }
  const double fracSalaried = static_cast<double>(salaried) / n;
  PL_CHECK(fracSalaried > 0.82 && fracSalaried < 0.88);
  std::printf("  PASS: students transition (dest salaried %.3f), none "
              "student at +12y\n", fracSalaried);
}

void testSmallBusinessChurn() {
  const random::RngFactory factory{kSeed};

  const int n = 10000;
  int aliveAtFive = 0, toSalaried = 0, transitioned = 0;
  const auto day29 = time::addDays(kSimStart, 29);
  const auto fiveYears = time::addDays(kSimStart, 1826);
  const auto thirtyYears = time::addDays(kSimStart, 30 * 365);
  for (entity::PersonId p = 1; p <= n; ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::smallBusiness, 40);

    // Residual clamp: every business survives the first 30 days.
    PL_CHECK(tlx::personaAt(tl, day29) == PersonaType::smallBusiness);

    if (tlx::personaAt(tl, fiveYears) == PersonaType::smallBusiness) {
      ++aliveAtFive;
    }

    // By 30 years (age 70) the business has ended one way or the
    // other; destination mix among those who closed before retiring.
    const auto late = tlx::personaAt(tl, thirtyYears);
    PL_CHECK(late != PersonaType::smallBusiness);
    if (tl.businessEnd < tl.retirement) {
      ++transitioned;
      if (tl.working == PersonaType::salaried) {
        ++toSalaried;
      }
    }
  }
  const double survival = static_cast<double>(aliveAtFive) / n;
  PL_CHECK(survival > 0.45 && survival < 0.55);
  const double fracSalaried =
      static_cast<double>(toSalaried) / static_cast<double>(transitioned);
  PL_CHECK(fracSalaried > 0.66 && fracSalaried < 0.74);
  std::printf("  PASS: business 5-yr survival %.3f (BED ~.50), post-close "
              "salaried %.3f\n", survival, fracSalaried);
}

void testRetireeSeed() {
  const random::RngFactory factory{kSeed};
  entity::PersonId p = 1;
  for (int age = 65; age <= 99; ++age, ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::retiree, age);
    PL_CHECK(tl.retirement <= kSimStart);
    PL_CHECK(tlx::personaAt(tl, kSimStart) == PersonaType::retiree);
    PL_CHECK(tlx::personaAt(tl, time::addDays(kSimStart, 20 * 365)) ==
             PersonaType::retiree);
  }
  std::printf("  PASS: retiree seeds backdated (claim <= simStart), "
              "retiree forever\n");
}

void testHnwExemption() {
  const random::RngFactory factory{kSeed};
  entity::PersonId p = 1;
  for (int age = 35; age <= 80; age += 5, ++p) {
    const auto tl = deriveOne(factory, p, PersonaType::highNetWorth, age);
    for (int y = 0; y <= 40; y += 5) {
      PL_CHECK(tlx::personaAt(tl, time::addDays(kSimStart, y * 365)) ==
               PersonaType::highNetWorth);
    }
  }
  std::printf("  PASS: highNetWorth exempt from transitions (declared)\n");
}

// student(0) -> working(1) -> retiree(2); smallBusiness counts as a
// working state. Rank must never decrease along a life.
[[nodiscard]] int rankOf(PersonaType t) {
  switch (t) {
  case PersonaType::student:
    return 0;
  case PersonaType::salaried:
  case PersonaType::freelancer:
  case PersonaType::smallBusiness:
  case PersonaType::highNetWorth:
    return 1;
  case PersonaType::retiree:
    return 2;
  }
  return 1;
}

void testMonotoneIrreversibility() {
  const random::RngFactory factory{kSeed};

  struct Case {
    PersonaType seed;
    int age;
  };
  const Case cases[] = {
      {PersonaType::student, 18},       {PersonaType::student, 30},
      {PersonaType::salaried, 25},      {PersonaType::salaried, 60},
      {PersonaType::freelancer, 40},    {PersonaType::smallBusiness, 35},
      {PersonaType::smallBusiness, 62}, {PersonaType::retiree, 72},
      {PersonaType::highNetWorth, 50},
  };
  entity::PersonId p = 1;
  for (const auto &c : cases) {
    for (int rep = 0; rep < 40; ++rep, ++p) {
      const auto tl = deriveOne(factory, p, c.seed, c.age);
      int prev = -1;
      for (int d = 0; d <= 40 * 365; d += 90) {
        const int r = rankOf(tlx::personaAt(tl, time::addDays(kSimStart, d)));
        PL_CHECK(r >= prev);
        prev = r;
      }
    }
  }
  std::printf("  PASS: persona rank monotone over 40 years (no "
              "reversals)\n");
}

void testLaneIsolation() {
  // Person 7's timeline is identical whether derived alone or after
  // six other derivations from an equal factory: {"persona-era", pid}
  // lanes never share state.
  const random::RngFactory a{kSeed};
  const random::RngFactory b{kSeed};

  const auto alone = deriveOne(a, 7, PersonaType::salaried, 45);
  for (entity::PersonId p = 1; p <= 6; ++p) {
    (void)deriveOne(b, p, PersonaType::student, 22);
  }
  const auto crowded = deriveOne(b, 7, PersonaType::salaried, 45);
  PL_CHECK_EQ(time::toEpochSeconds(alone.retirement),
              time::toEpochSeconds(crowded.retirement));
  PL_CHECK(alone.working == crowded.working);
  std::printf("  PASS: per-person lane isolation\n");
}

} // namespace

int main() {
  std::printf("test_persona_timeline\n");
  testDeterminism();
  testFraSchedule();
  testClaimMapping();
  testStartConsistency();
  testRetirementDistribution();
  testWorkerSettleClamp();
  testStudentTransitions();
  testSmallBusinessChurn();
  testRetireeSeed();
  testHnwExemption();
  testMonotoneIrreversibility();
  testLaneIsolation();
  std::printf("OK\n");
  return 0;
}
