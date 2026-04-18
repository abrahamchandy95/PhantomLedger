#include "phantomledger/personas/archetypes.hpp"
#include "phantomledger/personas/names.hpp"
#include "phantomledger/personas/predicates.hpp"
#include "phantomledger/personas/taxonomy.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>
#include <string_view>

using namespace PhantomLedger;

namespace {

constexpr double eps = 1e-10;

void testKindEnum() {
  PL_CHECK_EQ(static_cast<int>(personas::Kind::student), 0);
  PL_CHECK_EQ(static_cast<int>(personas::Kind::retiree), 1);
  PL_CHECK_EQ(static_cast<int>(personas::Kind::freelancer), 2);
  PL_CHECK_EQ(static_cast<int>(personas::Kind::smallBusiness), 3);
  PL_CHECK_EQ(static_cast<int>(personas::Kind::highNetWorth), 4);
  PL_CHECK_EQ(static_cast<int>(personas::Kind::salaried), 5);

  PL_CHECK_EQ(personas::kKindCount, 6U);
  PL_CHECK(personas::kDefaultKind == personas::Kind::salaried);

  std::printf("  PASS: Kind enum values\n");
}

void testKindNames() {
  PL_CHECK_EQ(personas::name(personas::Kind::student),
              std::string_view("student"));
  PL_CHECK_EQ(personas::name(personas::Kind::retiree),
              std::string_view("retired"));
  PL_CHECK_EQ(personas::name(personas::Kind::smallBusiness),
              std::string_view("smallbiz"));
  PL_CHECK_EQ(personas::name(personas::Kind::highNetWorth),
              std::string_view("hnw"));
  PL_CHECK_EQ(personas::toString(personas::Kind::salaried),
              std::string_view("salaried"));

  std::printf("  PASS: Kind names\n");
}

void testParseKind() {
  PL_CHECK(personas::parse("student") == personas::Kind::student);
  PL_CHECK(personas::parse("retired") == personas::Kind::retiree);
  PL_CHECK(personas::parse("freelancer") == personas::Kind::freelancer);
  PL_CHECK(personas::parse("smallbiz") == personas::Kind::smallBusiness);
  PL_CHECK(personas::parse("hnw") == personas::Kind::highNetWorth);
  PL_CHECK(personas::parse("salaried") == personas::Kind::salaried);

  PL_CHECK(!personas::parse("unknown").has_value());
  PL_CHECK(!personas::parse("").has_value());

  PL_CHECK(personas::fromString("unknown") == personas::Kind::salaried);
  PL_CHECK(personas::fromString("") == personas::Kind::salaried);

  std::printf("  PASS: Kind parsing\n");
}

void testTimingNames() {
  PL_CHECK_EQ(personas::name(personas::Timing::consumer),
              std::string_view("consumer"));
  PL_CHECK_EQ(personas::name(personas::Timing::consumerDay),
              std::string_view("consumer_day"));
  PL_CHECK_EQ(personas::name(personas::Timing::business),
              std::string_view("business"));

  PL_CHECK(personas::parseTiming("consumer") == personas::Timing::consumer);
  PL_CHECK(personas::parseTiming("consumer_day") ==
           personas::Timing::consumerDay);
  PL_CHECK(personas::parseTiming("business") == personas::Timing::business);

  PL_CHECK(!personas::parseTiming("unknown").has_value());
  PL_CHECK(personas::parseTimingOrDefault("unknown") ==
           personas::Timing::consumer);

  std::printf("  PASS: Timing names and parsing\n");
}

void testHasEarnedIncome() {
  PL_CHECK(!personas::hasEarnedIncome(personas::Kind::student));
  PL_CHECK(!personas::hasEarnedIncome(personas::Kind::retiree));
  PL_CHECK(personas::hasEarnedIncome(personas::Kind::freelancer));
  PL_CHECK(personas::hasEarnedIncome(personas::Kind::smallBusiness));
  PL_CHECK(personas::hasEarnedIncome(personas::Kind::highNetWorth));
  PL_CHECK(personas::hasEarnedIncome(personas::Kind::salaried));

  // Compatibility alias
  PL_CHECK(!personas::isEarner(personas::Kind::student));
  PL_CHECK(personas::isEarner(personas::Kind::salaried));

  std::printf("  PASS: hasEarnedIncome / isEarner\n");
}

void testArchetypeValues() {
  const auto &sal = personas::archetype(personas::Kind::salaried);
  PL_CHECK(std::abs(sal.rateMultiplier - 1.0) < eps);
  PL_CHECK(std::abs(sal.amountMultiplier - 1.0) < eps);
  PL_CHECK(sal.timing == personas::Timing::consumer);
  PL_CHECK(std::abs(sal.initialBalance - 1200.0) < eps);
  PL_CHECK(std::abs(sal.cardProb - 0.70) < eps);
  PL_CHECK(std::abs(sal.ccShare - 0.70) < eps);
  PL_CHECK(std::abs(sal.creditLimit - 3000.0) < eps);
  PL_CHECK(std::abs(sal.weight - 1.0) < eps);
  PL_CHECK(std::abs(sal.paycheckSensitivity - 0.40) < eps);

  const auto &stu = personas::archetype(personas::Kind::student);
  PL_CHECK(std::abs(stu.rateMultiplier - 0.7) < eps);
  PL_CHECK(std::abs(stu.initialBalance - 200.0) < eps);
  PL_CHECK(std::abs(stu.cardProb - 0.25) < eps);

  const auto &sb = personas::archetype(personas::Kind::smallBusiness);
  PL_CHECK(std::abs(sb.rateMultiplier - 2.4) < eps);
  PL_CHECK(std::abs(sb.amountMultiplier - 1.8) < eps);
  PL_CHECK(sb.timing == personas::Timing::business);
  PL_CHECK(std::abs(sb.initialBalance - 8000.0) < eps);

  const auto &hnw = personas::archetype(personas::Kind::highNetWorth);
  PL_CHECK(std::abs(hnw.amountMultiplier - 2.8) < eps);
  PL_CHECK(std::abs(hnw.creditLimit - 15000.0) < eps);

  const auto &ret = personas::archetype(personas::Kind::retiree);
  PL_CHECK(ret.timing == personas::Timing::consumerDay);
  PL_CHECK(std::abs(ret.initialBalance - 1500.0) < eps);

  std::printf("  PASS: archetype values\n");
}

void testArchetypeByString() {
  const auto &a = personas::archetype("freelancer");
  PL_CHECK(std::abs(a.rateMultiplier - 1.1) < eps);

  const auto &b = personas::archetype("nonexistent");
  PL_CHECK(std::abs(b.rateMultiplier - 1.0) < eps);
  PL_CHECK(b.timing == personas::Timing::consumer);

  std::printf("  PASS: archetype by string\n");
}

void testDefaultFractions() {
  double total = 0.0;
  for (double fraction : personas::kDefaultFractions) {
    PL_CHECK(fraction >= 0.0);
    PL_CHECK(fraction <= 1.0);
    total += fraction;
  }

  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::student) - 0.12) <
           eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::retiree) - 0.10) <
           eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::freelancer) -
                    0.10) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::smallBusiness) -
                    0.06) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::highNetWorth) -
                    0.02) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Kind::salaried) -
                    0.60) < eps);

  PL_CHECK(std::abs(total - 1.0) < eps);

  std::printf("  PASS: default fractions sum = %.2f\n", total);
}

void testPaycheckSensitivityBeta() {
  auto student = personas::paycheckSensitivityBeta(personas::Kind::student);
  PL_CHECK(std::abs(student.alpha - 4.0) < eps);
  PL_CHECK(std::abs(student.beta - 2.0) < eps);

  auto hnw = personas::paycheckSensitivityBeta(personas::Kind::highNetWorth);
  PL_CHECK(std::abs(hnw.alpha - 1.0) < eps);
  PL_CHECK(std::abs(hnw.beta - 8.0) < eps);

  auto sal = personas::paycheckSensitivityBeta(personas::Kind::salaried);
  PL_CHECK(std::abs(sal.alpha - 2.0) < eps);
  PL_CHECK(std::abs(sal.beta - 3.0) < eps);

  std::printf("  PASS: paycheckSensitivityBeta\n");
}

void testArchetypeArrayCompleteness() {
  for (std::size_t i = 0; i < personas::kKindCount; ++i) {
    auto kind = static_cast<personas::Kind>(i);
    const auto &a = personas::archetype(kind);

    PL_CHECK(a.rateMultiplier > 0.0);
    PL_CHECK(a.amountMultiplier > 0.0);
    PL_CHECK(a.initialBalance > 0.0);
    PL_CHECK(a.cardProb >= 0.0 && a.cardProb <= 1.0);
    PL_CHECK(a.ccShare >= 0.0 && a.ccShare <= 1.0);
    PL_CHECK(a.creditLimit > 0.0);
    PL_CHECK(a.weight > 0.0);
    PL_CHECK(a.paycheckSensitivity >= 0.0);
  }

  std::printf("  PASS: archetype table completeness\n");
}

} // namespace

int main() {
  std::printf("=== Persona Tests ===\n");
  testKindEnum();
  testKindNames();
  testParseKind();
  testTimingNames();
  testHasEarnedIncome();
  testArchetypeValues();
  testArchetypeByString();
  testDefaultFractions();
  testPaycheckSensitivityBeta();
  testArchetypeArrayCompleteness();
  std::printf("All persona tests passed.\n\n");
  return 0;
}
