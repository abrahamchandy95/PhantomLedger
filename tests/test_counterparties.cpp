#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <cstdio>
#include <set>

using namespace PhantomLedger;

namespace {

constexpr std::uint64_t kInstitutionalBaseSerial = 1'000'000'000ULL;

constexpr entity::Key institutional(std::uint64_t offset) noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         kInstitutionalBaseSerial + offset);
}

constexpr entity::Key governmentEmployer(std::uint64_t number) noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         number);
}

void testKeyValues() {
  PL_CHECK_EQ(counterparties::key(counterparties::Government::ssa),
              governmentEmployer(9'000'001ULL));
  PL_CHECK_EQ(counterparties::key(counterparties::Government::disability),
              governmentEmployer(9'000'002ULL));

  PL_CHECK_EQ(counterparties::key(counterparties::Lending::mortgage),
              institutional(1));
  PL_CHECK_EQ(counterparties::key(counterparties::Lending::autoLoan),
              institutional(2));
  PL_CHECK_EQ(counterparties::key(counterparties::Lending::studentServicer),
              institutional(3));

  PL_CHECK_EQ(counterparties::key(counterparties::Tax::irsTreasury),
              institutional(4));

  PL_CHECK_EQ(counterparties::key(counterparties::Insurance::autoCarrier),
              institutional(5));
  PL_CHECK_EQ(counterparties::key(counterparties::Insurance::homeCarrier),
              institutional(6));
  PL_CHECK_EQ(counterparties::key(counterparties::Insurance::lifeCarrier),
              institutional(7));

  std::printf("  PASS: key values\n");
}

void testAllAreExternal() {
  for (const auto &account : counterparties::kAll) {
    PL_CHECK(encoding::isExternal(account));
  }
  std::printf("  PASS: all keys are external\n");
}

void testAllUnique() {
  std::set<entity::Key> unique;
  for (const auto &account : counterparties::kAll) {
    unique.insert(account);
  }

  PL_CHECK_EQ(unique.size(), counterparties::kAll.size());
  std::printf("  PASS: all keys are unique\n");
}

void testGroupSizes() {
  PL_CHECK_EQ(counterparties::kGovernment.size(), 2U);
  PL_CHECK_EQ(counterparties::kInsurance.size(), 3U);
  PL_CHECK_EQ(counterparties::kLending.size(), 3U);
  PL_CHECK_EQ(counterparties::kTax.size(), 1U);
  PL_CHECK_EQ(counterparties::kAll.size(), 9U);

  PL_CHECK_EQ(
      counterparties::kAll.size(),
      counterparties::kGovernment.size() + counterparties::kInsurance.size() +
          counterparties::kLending.size() + counterparties::kTax.size());

  std::printf("  PASS: group sizes\n");
}

void testGroupContents() {
  PL_CHECK_EQ(counterparties::kGovernment[0],
              counterparties::key(counterparties::Government::ssa));
  PL_CHECK_EQ(counterparties::kGovernment[1],
              counterparties::key(counterparties::Government::disability));

  PL_CHECK_EQ(counterparties::kInsurance[0],
              counterparties::key(counterparties::Insurance::autoCarrier));
  PL_CHECK_EQ(counterparties::kInsurance[1],
              counterparties::key(counterparties::Insurance::homeCarrier));
  PL_CHECK_EQ(counterparties::kInsurance[2],
              counterparties::key(counterparties::Insurance::lifeCarrier));

  PL_CHECK_EQ(counterparties::kLending[0],
              counterparties::key(counterparties::Lending::mortgage));
  PL_CHECK_EQ(counterparties::kLending[1],
              counterparties::key(counterparties::Lending::autoLoan));
  PL_CHECK_EQ(counterparties::kLending[2],
              counterparties::key(counterparties::Lending::studentServicer));

  PL_CHECK_EQ(counterparties::kTax[0],
              counterparties::key(counterparties::Tax::irsTreasury));

  std::printf("  PASS: group contents\n");
}

void testRoleConventions() {
  // Government counterparties are modeled as external employers so that
  // SSA / disability deposits show up as payroll-shaped inbound flows.
  for (const auto &account : counterparties::kGovernment) {
    PL_CHECK(account.role == entity::Role::employer);
    PL_CHECK(account.bank == entity::Bank::external);
  }

  // Insurance, lending, and tax counterparties are external businesses.
  for (const auto &account : counterparties::kInsurance) {
    PL_CHECK(account.role == entity::Role::business);
    PL_CHECK(account.bank == entity::Bank::external);
  }
  for (const auto &account : counterparties::kLending) {
    PL_CHECK(account.role == entity::Role::business);
    PL_CHECK(account.bank == entity::Bank::external);
  }
  for (const auto &account : counterparties::kTax) {
    PL_CHECK(account.role == entity::Role::business);
    PL_CHECK(account.bank == entity::Bank::external);
  }

  std::printf("  PASS: role conventions\n");
}

void testIsHelper() {
  const auto ssa = counterparties::key(counterparties::Government::ssa);
  const auto disability =
      counterparties::key(counterparties::Government::disability);

  PL_CHECK(counterparties::is(ssa, counterparties::Government::ssa));
  PL_CHECK(!counterparties::is(ssa, counterparties::Government::disability));
  PL_CHECK(
      counterparties::is(disability, counterparties::Government::disability));

  std::printf("  PASS: is helper\n");
}

} // namespace

int main() {
  std::printf("=== Counterparty Tests ===\n");
  testKeyValues();
  testAllAreExternal();
  testAllUnique();
  testGroupSizes();
  testGroupContents();
  testRoleConventions();
  testIsHelper();
  std::printf("All counterparty tests passed.\n\n");
  return 0;
}
