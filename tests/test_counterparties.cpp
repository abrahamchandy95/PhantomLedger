#include "phantomledger/counterparties/accounts.hpp"
#include "phantomledger/entities/encoding/external.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <set>
#include <string_view>

using namespace PhantomLedger;

namespace {

void testIdValues() {
  PL_CHECK_EQ(counterparties::id(counterparties::Government::ssa).value,
              std::string_view("XGOV00000001"));
  PL_CHECK_EQ(counterparties::id(counterparties::Government::disability).value,
              std::string_view("XGOV00000002"));

  PL_CHECK_EQ(counterparties::id(counterparties::Insurance::autoCarrier).value,
              std::string_view("XINS00000001"));
  PL_CHECK_EQ(counterparties::id(counterparties::Insurance::homeCarrier).value,
              std::string_view("XINS00000002"));
  PL_CHECK_EQ(counterparties::id(counterparties::Insurance::lifeCarrier).value,
              std::string_view("XINS00000003"));

  PL_CHECK_EQ(counterparties::id(counterparties::Lending::mortgage).value,
              std::string_view("XLND00000001"));
  PL_CHECK_EQ(counterparties::id(counterparties::Lending::autoLoan).value,
              std::string_view("XLND00000002"));
  PL_CHECK_EQ(
      counterparties::id(counterparties::Lending::studentServicer).value,
      std::string_view("XLND00000003"));

  PL_CHECK_EQ(counterparties::id(counterparties::Tax::irsTreasury).value,
              std::string_view("XIRS00000001"));

  std::printf("  PASS: id values\n");
}

void testNames() {
  PL_CHECK_EQ(counterparties::name(counterparties::Government::ssa),
              std::string_view("gov_ssa"));
  PL_CHECK_EQ(counterparties::name(counterparties::Government::disability),
              std::string_view("gov_disability"));

  PL_CHECK_EQ(counterparties::name(counterparties::Insurance::autoCarrier),
              std::string_view("insurance_auto"));
  PL_CHECK_EQ(counterparties::name(counterparties::Insurance::homeCarrier),
              std::string_view("insurance_home"));
  PL_CHECK_EQ(counterparties::name(counterparties::Insurance::lifeCarrier),
              std::string_view("insurance_life"));

  PL_CHECK_EQ(counterparties::name(counterparties::Lending::mortgage),
              std::string_view("lender_mortgage"));
  PL_CHECK_EQ(counterparties::name(counterparties::Lending::autoLoan),
              std::string_view("lender_auto"));
  PL_CHECK_EQ(counterparties::name(counterparties::Lending::studentServicer),
              std::string_view("servicer_student"));

  PL_CHECK_EQ(counterparties::name(counterparties::Tax::irsTreasury),
              std::string_view("irs_treasury"));

  std::printf("  PASS: names\n");
}

void testAllAreExternal() {
  for (auto account : counterparties::kAll) {
    PL_CHECK(encoding::isExternal(account.value));
  }
  std::printf("  PASS: all ids are external\n");
}

void testAllUnique() {
  std::set<std::string_view> unique;
  for (auto account : counterparties::kAll) {
    unique.insert(account.value);
  }

  PL_CHECK_EQ(unique.size(), counterparties::kAll.size());
  std::printf("  PASS: all ids are unique\n");
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
  PL_CHECK_EQ(counterparties::kGovernment[0].id,
              counterparties::id(counterparties::Government::ssa));
  PL_CHECK_EQ(counterparties::kGovernment[0].name,
              counterparties::name(counterparties::Government::ssa));

  PL_CHECK_EQ(counterparties::kGovernment[1].id,
              counterparties::id(counterparties::Government::disability));
  PL_CHECK_EQ(counterparties::kGovernment[1].name,
              counterparties::name(counterparties::Government::disability));

  PL_CHECK_EQ(counterparties::kInsurance[0].id,
              counterparties::id(counterparties::Insurance::autoCarrier));
  PL_CHECK_EQ(counterparties::kInsurance[0].name,
              counterparties::name(counterparties::Insurance::autoCarrier));

  PL_CHECK_EQ(counterparties::kInsurance[1].id,
              counterparties::id(counterparties::Insurance::homeCarrier));
  PL_CHECK_EQ(counterparties::kInsurance[1].name,
              counterparties::name(counterparties::Insurance::homeCarrier));

  PL_CHECK_EQ(counterparties::kInsurance[2].id,
              counterparties::id(counterparties::Insurance::lifeCarrier));
  PL_CHECK_EQ(counterparties::kInsurance[2].name,
              counterparties::name(counterparties::Insurance::lifeCarrier));

  PL_CHECK_EQ(counterparties::kLending[0].id,
              counterparties::id(counterparties::Lending::mortgage));
  PL_CHECK_EQ(counterparties::kLending[0].name,
              counterparties::name(counterparties::Lending::mortgage));

  PL_CHECK_EQ(counterparties::kLending[1].id,
              counterparties::id(counterparties::Lending::autoLoan));
  PL_CHECK_EQ(counterparties::kLending[1].name,
              counterparties::name(counterparties::Lending::autoLoan));

  PL_CHECK_EQ(counterparties::kLending[2].id,
              counterparties::id(counterparties::Lending::studentServicer));
  PL_CHECK_EQ(counterparties::kLending[2].name,
              counterparties::name(counterparties::Lending::studentServicer));

  PL_CHECK_EQ(counterparties::kTax[0].id,
              counterparties::id(counterparties::Tax::irsTreasury));
  PL_CHECK_EQ(counterparties::kTax[0].name,
              counterparties::name(counterparties::Tax::irsTreasury));

  std::printf("  PASS: group contents\n");
}

void testPrefixConventions() {
  for (const auto &entry : counterparties::kGovernment) {
    PL_CHECK(entry.id.value.starts_with("XGOV"));
  }
  for (const auto &entry : counterparties::kInsurance) {
    PL_CHECK(entry.id.value.starts_with("XINS"));
  }
  for (const auto &entry : counterparties::kLending) {
    PL_CHECK(entry.id.value.starts_with("XLND"));
  }
  for (const auto &entry : counterparties::kTax) {
    PL_CHECK(entry.id.value.starts_with("XIRS"));
  }

  std::printf("  PASS: prefix conventions\n");
}

void testIsHelper() {
  auto ssa = counterparties::id(counterparties::Government::ssa);
  auto disability = counterparties::id(counterparties::Government::disability);

  PL_CHECK(counterparties::is(ssa, counterparties::Government::ssa));
  PL_CHECK(!counterparties::is(ssa, counterparties::Government::disability));
  PL_CHECK(
      counterparties::is(disability, counterparties::Government::disability));

  std::printf("  PASS: is helper\n");
}

} // namespace

int main() {
  std::printf("=== Counterparty Tests ===\n");
  testIdValues();
  testNames();
  testAllAreExternal();
  testAllUnique();
  testGroupSizes();
  testGroupContents();
  testPrefixConventions();
  testIsHelper();
  std::printf("All counterparty tests passed.\n\n");
  return 0;
}
