#include "phantomledger/synth/personas/dob.hpp"

#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

// macro-history-v1 H2 step 2a: MEANING gates for the single-age-axis
// birth-date carrier (synth/personas/dob.hpp -> Pack::birthDates,
// authority U-7). The carrier is drawn on isolated {"dob", personId}
// lanes with the retired in-stream draw's exact shape (persona age
// band at SIM START + 0-364 day offset); PII renders from it and the
// SSA deposit cohort derives from its REAL birth day-of-month. The
// golden digests pin the realized bytes; these gates pin the MEANING.

using namespace PhantomLedger;
namespace personas = ::PhantomLedger::personas;
namespace spersonas = synth::personas;

namespace {

constexpr std::uint64_t kSeed = 0xD0BCA11EULL;

const time::TimePoint kSimStart =
    time::makeTime(time::CalendarDate{.year = 1991, .month = 1, .day = 1});

[[nodiscard]] entity::behavior::Assignment uniformAssignment(personas::Type t,
                                                             std::size_t n) {
  entity::behavior::Assignment out;
  out.byPerson.assign(n, t);
  return out;
}

[[nodiscard]] long daysBefore(time::CalendarDate dob) {
  return static_cast<long>(
      (time::toEpochSeconds(kSimStart) - time::toEpochSeconds(time::makeTime(dob))) /
      86400);
}

void testDeterminism() {
  const auto assignment = uniformAssignment(personas::Type::salaried, 500);
  const auto a = spersonas::birthDates(kSeed, kSimStart, assignment);
  const auto b = spersonas::birthDates(kSeed, kSimStart, assignment);
  PL_CHECK_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    PL_CHECK_EQ(a[i].year, b[i].year);
    PL_CHECK_EQ(a[i].month, b[i].month);
    PL_CHECK_EQ(a[i].day, b[i].day);
  }
  std::printf("  PASS: same (worldSeed, simStart, assignment) -> identical "
              "carrier\n");
}

void testBands() {
  // The pii::sampleDob persona age bands (synth/pii/samplers.hpp
  // bandFor): age uniform in [min, max] years at sim start plus a
  // 0-364 day offset, so days-before-start lies in
  // [min*365, max*365 + 364].
  struct Band {
    personas::Type type;
    long minAge;
    long maxAge;
  };
  const Band bands[] = {
      {personas::Type::student, 16, 34},      {personas::Type::retiree, 65, 99},
      {personas::Type::freelancer, 25, 65},   {personas::Type::smallBusiness, 30, 70},
      {personas::Type::highNetWorth, 35, 80}, {personas::Type::salaried, 22, 65},
  };

  for (const auto &band : bands) {
    const auto assignment = uniformAssignment(band.type, 2000);
    const auto carrier = spersonas::birthDates(kSeed, kSimStart, assignment);
    for (const auto &dob : carrier) {
      const long days = daysBefore(dob);
      PL_CHECK(days >= band.minAge * 365);
      PL_CHECK(days <= band.maxAge * 365 + 364);
      PL_CHECK(dob.month >= 1 && dob.month <= 12);
      PL_CHECK(dob.day >= 1 && dob.day <= 31);
    }
  }
  std::printf("  PASS: persona age bands at sim start (all six types)\n");
}

void testLaneIsolation() {
  // Person k's birth date is a pure function of (worldSeed, k, type,
  // simStart) — a bigger population never moves an earlier person.
  const auto small =
      spersonas::birthDates(kSeed, kSimStart,
                            uniformAssignment(personas::Type::salaried, 50));
  const auto large =
      spersonas::birthDates(kSeed, kSimStart,
                            uniformAssignment(personas::Type::salaried, 400));
  for (std::size_t i = 0; i < small.size(); ++i) {
    PL_CHECK_EQ(small[i].year, large[i].year);
    PL_CHECK_EQ(small[i].month, large[i].month);
    PL_CHECK_EQ(small[i].day, large[i].day);
  }
  std::printf("  PASS: per-person lane isolation (population-size "
              "independent)\n");
}

void testSsaCohorts() {
  // The statutory mapping (MEASUREMENT: SSA payment schedule).
  PL_CHECK_EQ(time::ssaCohort(1), 0);
  PL_CHECK_EQ(time::ssaCohort(10), 0);
  PL_CHECK_EQ(time::ssaCohort(11), 1);
  PL_CHECK_EQ(time::ssaCohort(20), 1);
  PL_CHECK_EQ(time::ssaCohort(21), 2);
  PL_CHECK_EQ(time::ssaCohort(31), 2);

  // Real birth days populate ALL THREE deposit cohorts, with the
  // 21-31 band carrying the largest share (11 of 31 days) — the
  // retired blake2b syntheticBirthDay only reached days 1-28.
  const auto carrier =
      spersonas::birthDates(kSeed, kSimStart,
                            uniformAssignment(personas::Type::retiree, 5000));
  int byCohort[3] = {0, 0, 0};
  for (const auto &dob : carrier) {
    ++byCohort[time::ssaCohort(static_cast<int>(dob.day))];
  }
  PL_CHECK(byCohort[0] > 0 && byCohort[1] > 0 && byCohort[2] > 0);
  const double frac2 = static_cast<double>(byCohort[2]) / 5000.0;
  PL_CHECK(frac2 > 0.25 && frac2 < 0.45); // ~11/31 with month-length skew
  std::printf("  PASS: SSA cohorts 0/1/2 = %d/%d/%d from real birth days\n",
              byCohort[0], byCohort[1], byCohort[2]);
}

} // namespace

int main() {
  std::printf("test_dob_carrier\n");
  testDeterminism();
  testBands();
  testLaneIsolation();
  testSsaCohorts();
  std::printf("OK\n");
  return 0;
}
