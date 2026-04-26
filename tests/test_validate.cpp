#include "phantomledger/primitives/validate/checks.hpp"
#include "test_support.hpp"

#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

// `validate::*` lives at `PhantomLedger::primitives::validate::*` — the
// older test assumed a top-level `validate::` namespace which doesn't
// exist. Pull in the right one.
namespace validate = PhantomLedger::primitives::validate;

namespace {

void testGt() {
  validate::gt("x", 5, 4);
  validate::gt("x", 0.01, 0.0);

  PL_CHECK_THROWS(validate::gt("x", 4, 4));
  PL_CHECK_THROWS(validate::gt("x", 3, 4));
  PL_CHECK_THROWS(validate::gt("x", -1.0, 0.0));
  std::printf("  PASS: gt\n");
}

void testGe() {
  validate::ge("x", 4, 4);
  validate::ge("x", 5, 4);
  validate::ge("x", 0.0, 0.0);

  PL_CHECK_THROWS(validate::ge("x", 3, 4));
  PL_CHECK_THROWS(validate::ge("x", -0.01, 0.0));
  std::printf("  PASS: ge\n");
}

void testBetween() {
  validate::between("x", 5, 0, 10);
  validate::between("x", 0, 0, 10);
  validate::between("x", 10, 0, 10);
  validate::between("x", 0.5, 0.0, 1.0);

  PL_CHECK_THROWS(validate::between("x", -1, 0, 10));
  PL_CHECK_THROWS(validate::between("x", 11, 0, 10));
  PL_CHECK_THROWS(validate::between("x", 1.01, 0.0, 1.0));
  std::printf("  PASS: between\n");
}

void testFinite() {
  validate::finite("x", 0.0);
  validate::finite("x", -1e300);
  validate::finite("x", 1e300);

  PL_CHECK_THROWS(
      validate::finite("x", std::numeric_limits<double>::infinity()));
  PL_CHECK_THROWS(
      validate::finite("x", -std::numeric_limits<double>::infinity()));
  PL_CHECK_THROWS(
      validate::finite("x", std::numeric_limits<double>::quiet_NaN()));
  std::printf("  PASS: finite\n");
}

void testPositive() {
  validate::positive("x", 1);
  validate::positive("x", 0.001);

  PL_CHECK_THROWS(validate::positive("x", 0));
  PL_CHECK_THROWS(validate::positive("x", -1));
  PL_CHECK_THROWS(validate::positive("x", 0.0));
  std::printf("  PASS: positive\n");
}

void testNonNegative() {
  validate::nonNegative("x", 0);
  validate::nonNegative("x", 1);
  validate::nonNegative("x", 0.0);

  PL_CHECK_THROWS(validate::nonNegative("x", -1));
  PL_CHECK_THROWS(validate::nonNegative("x", -0.001));
  std::printf("  PASS: nonNegative\n");
}

void testErrorMessage() {
  bool caught = false;
  try {
    validate::gt("my_field", 3, 5);
  } catch (const std::invalid_argument &e) {
    caught = true;
    std::string msg = e.what();
    PL_CHECK(msg.find("my_field") != std::string::npos);
    PL_CHECK(msg.find("5") != std::string::npos);
    PL_CHECK(msg.find("3") != std::string::npos);
  }
  PL_CHECK(caught);
  std::printf("  PASS: error messages include field name and values\n");
}

} // namespace

int main() {
  std::printf("=== Validate Tests ===\n");
  testGt();
  testGe();
  testBetween();
  testFinite();
  testPositive();
  testNonNegative();
  testErrorMessage();
  std::printf("All validate tests passed.\n\n");
  return 0;
}
