#include "phantomledger/taxonomies/personas/archetypes.hpp"
#include "phantomledger/taxonomies/personas/names.hpp"
#include "phantomledger/taxonomies/personas/predicates.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>
#include <string_view>

using namespace PhantomLedger;

namespace {

constexpr double eps = 1e-10;

void testKindEnum() {
  PL_CHECK_EQ(static_cast<int>(personas::Type::student), 0);
  PL_CHECK_EQ(static_cast<int>(personas::Type::retiree), 1);
  PL_CHECK_EQ(static_cast<int>(personas::Type::freelancer), 2);
  PL_CHECK_EQ(static_cast<int>(personas::Type::smallBusiness), 3);
  PL_CHECK_EQ(static_cast<int>(personas::Type::highNetWorth), 4);
  PL_CHECK_EQ(static_cast<int>(personas::Type::salaried), 5);

  PL_CHECK_EQ(personas::kKindCount, 6U);
  PL_CHECK(personas::kDefaultType == personas::Type::salaried);

  std::printf("  PASS: Kind enum values\n");
}

void testKindNames() {
  PL_CHECK_EQ(personas::name(personas::Type::student),
              std::string_view("student"));
  PL_CHECK_EQ(personas::name(personas::Type::retiree),
              std::string_view("retired"));
  PL_CHECK_EQ(personas::name(personas::Type::smallBusiness),
              std::string_view("smallbiz"));
  PL_CHECK_EQ(personas::name(personas::Type::highNetWorth),
              std::string_view("hnw"));
  PL_CHECK_EQ(personas::toString(personas::Type::salaried),
              std::string_view("salaried"));

  std::printf("  PASS: Kind names\n");
}

void testParseKind() {
  PL_CHECK(personas::parse("student") == personas::Type::student);
  PL_CHECK(personas::parse("retired") == personas::Type::retiree);
  PL_CHECK(personas::parse("freelancer") == personas::Type::freelancer);
  PL_CHECK(personas::parse("smallbiz") == personas::Type::smallBusiness);
  PL_CHECK(personas::parse("hnw") == personas::Type::highNetWorth);
  PL_CHECK(personas::parse("salaried") == personas::Type::salaried);

  PL_CHECK(!personas::parse("unknown").has_value());
  PL_CHECK(!personas::parse("").has_value());

  PL_CHECK(personas::fromString("unknown") == personas::Type::salaried);
  PL_CHECK(personas::fromString("") == personas::Type::salaried);

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
  PL_CHECK(!personas::hasEarnedIncome(personas::Type::student));
  PL_CHECK(!personas::hasEarnedIncome(personas::Type::retiree));
  PL_CHECK(personas::hasEarnedIncome(personas::Type::freelancer));
  PL_CHECK(personas::hasEarnedIncome(personas::Type::smallBusiness));
  PL_CHECK(personas::hasEarnedIncome(personas::Type::highNetWorth));
  PL_CHECK(personas::hasEarnedIncome(personas::Type::salaried));

  // Compatibility alias
  PL_CHECK(!personas::isEarner(personas::Type::student));
  PL_CHECK(personas::isEarner(personas::Type::salaried));

  std::printf("  PASS: hasEarnedIncome / isEarner\n");
}

void testArchetypeValues() {
  const auto &sal = personas::archetype(personas::Type::salaried);
  PL_CHECK(std::abs(sal.rateMultiplier - 1.0) < eps);
  PL_CHECK(std::abs(sal.amountMultiplier - 1.0) < eps);
  PL_CHECK(sal.timing == personas::Timing::consumer);
  PL_CHECK(std::abs(sal.initialBalance - 1200.0) < eps);
  PL_CHECK(std::abs(sal.cardProb - 0.70) < eps);
  PL_CHECK(std::abs(sal.ccShare - 0.70) < eps);
  PL_CHECK(std::abs(sal.creditLimit - 3000.0) < eps);
  PL_CHECK(std::abs(sal.weight - 1.0) < eps);
  PL_CHECK(std::abs(sal.paycheckSensitivity - 0.40) < eps);

  const auto &stu = personas::archetype(personas::Type::student);
  PL_CHECK(std::abs(stu.rateMultiplier - 0.7) < eps);
  PL_CHECK(std::abs(stu.initialBalance - 200.0) < eps);
  PL_CHECK(std::abs(stu.cardProb - 0.25) < eps);

  const auto &sb = personas::archetype(personas::Type::smallBusiness);
  PL_CHECK(std::abs(sb.rateMultiplier - 2.4) < eps);
  PL_CHECK(std::abs(sb.amountMultiplier - 1.8) < eps);
  PL_CHECK(sb.timing == personas::Timing::business);
  PL_CHECK(std::abs(sb.initialBalance - 8000.0) < eps);

  const auto &hnw = personas::archetype(personas::Type::highNetWorth);
  PL_CHECK(std::abs(hnw.amountMultiplier - 2.8) < eps);
  PL_CHECK(std::abs(hnw.creditLimit - 15000.0) < eps);

  const auto &ret = personas::archetype(personas::Type::retiree);
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

  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::student) - 0.12) <
           eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::retiree) - 0.10) <
           eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::freelancer) -
                    0.10) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::smallBusiness) -
                    0.06) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::highNetWorth) -
                    0.02) < eps);
  PL_CHECK(std::abs(personas::defaultFraction(personas::Type::salaried) -
                    0.60) < eps);

  PL_CHECK(std::abs(total - 1.0) < eps);

  std::printf("  PASS: default fractions sum = %.2f\n", total);
}

void testPaycheckSensitivityBeta() {
  auto student = personas::paycheckSensitivityBeta(personas::Type::student);
  PL_CHECK(std::abs(student.alpha - 4.0) < eps);
  PL_CHECK(std::abs(student.beta - 2.0) < eps);

  auto hnw = personas::paycheckSensitivityBeta(personas::Type::highNetWorth);
  PL_CHECK(std::abs(hnw.alpha - 1.0) < eps);
  PL_CHECK(std::abs(hnw.beta - 8.0) < eps);

  auto sal = personas::paycheckSensitivityBeta(personas::Type::salaried);
  PL_CHECK(std::abs(sal.alpha - 2.0) < eps);
  PL_CHECK(std::abs(sal.beta - 3.0) < eps);

  std::printf("  PASS: paycheckSensitivityBeta\n");
}

void testArchetypeArrayCompleteness() {
  for (std::size_t i = 0; i < personas::kKindCount; ++i) {
    auto kind = static_cast<personas::Type>(i);
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
