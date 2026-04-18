#include "phantomledger/clearing/ledger.hpp"
#include "phantomledger/entities/accounts.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace PhantomLedger;

namespace {

constexpr double eps = 1e-10;

bool almostEqual(double lhs, double rhs) { return std::abs(lhs - rhs) < eps; }

clearing::Ledger makeTestLedger() {
  clearing::Ledger ledger;
  ledger.initialize(5);

  ledger.addAccount(entities::accounts::account(1), 0);
  ledger.cash(0) = 1000.0;
  ledger.overdraft(0) = 200.0;
  ledger.linked(0) = 100.0;
  ledger.courtesy(0) = 50.0;

  ledger.addAccount(entities::accounts::account(2), 1);
  ledger.cash(1) = 50.0;

  ledger.addAccount(entities::accounts::account(3), 2);
  ledger.createHub(2);

  ledger.addAccount(entities::accounts::account(4), 4);
  ledger.cash(4) = 100.0;
  ledger.overdraft(4) = 500.0;
  ledger.linked(4) = 200.0;

  return ledger;
}

void testTransferDecisionHelpers() {
  const auto accepted = clearing::TransferDecision::accept();
  PL_CHECK(accepted.accepted());
  PL_CHECK(!accepted.rejected());
  PL_CHECK(!accepted.rejectReason().has_value());

  bool threw = false;
  try {
    (void)accepted.reason();
  } catch (const std::logic_error &) {
    threw = true;
  }
  PL_CHECK(threw);

  const auto rejected =
      clearing::TransferDecision::reject(clearing::RejectReason::unfunded);
  PL_CHECK(!rejected.accepted());
  PL_CHECK(rejected.rejected());
  PL_CHECK_EQ(rejected.rejectReason(), clearing::RejectReason::unfunded);
  PL_CHECK_EQ(rejected.reason(), clearing::RejectReason::unfunded);
  std::printf("  PASS: TransferDecision helpers\n");
}

void testInvalidAmounts() {
  auto ledger = makeTestLedger();

  const auto zero = ledger.transfer(entities::accounts::account(1),
                                    entities::accounts::account(2), 0.0);
  PL_CHECK(zero.rejected());
  PL_CHECK_EQ(zero.reason(), clearing::RejectReason::invalid);

  const auto negative = ledger.transfer(entities::accounts::account(1),
                                        entities::accounts::account(2), -5.0);
  PL_CHECK(negative.rejected());
  PL_CHECK_EQ(negative.reason(), clearing::RejectReason::invalid);

  const auto nan = ledger.transfer(entities::accounts::account(1),
                                   entities::accounts::account(2),
                                   std::numeric_limits<double>::quiet_NaN());
  PL_CHECK(nan.rejected());
  PL_CHECK_EQ(nan.reason(), clearing::RejectReason::invalid);

  const auto inf = ledger.transfer(entities::accounts::account(1),
                                   entities::accounts::account(2),
                                   std::numeric_limits<double>::infinity());
  PL_CHECK(inf.rejected());
  PL_CHECK_EQ(inf.reason(), clearing::RejectReason::invalid);
  std::printf("  PASS: invalid amounts rejected\n");
}

void testUnbookedCases() {
  auto ledger = makeTestLedger();

  const auto unknownSrc = ledger.transfer(entities::accounts::account(99),
                                          entities::accounts::account(2), 10.0);
  PL_CHECK(unknownSrc.rejected());
  PL_CHECK_EQ(unknownSrc.reason(), clearing::RejectReason::unbooked);

  const auto unknownDst = ledger.transfer(
      entities::accounts::account(1), entities::accounts::account(99), 10.0);
  PL_CHECK(unknownDst.rejected());
  PL_CHECK_EQ(unknownDst.reason(), clearing::RejectReason::unbooked);

  const auto externalToExternal = ledger.transfer(
      entities::accounts::employer(1),
      entities::accounts::merchant(99, entities::BankAccount::external), 100.0);
  PL_CHECK(externalToExternal.rejected());
  PL_CHECK_EQ(externalToExternal.reason(), clearing::RejectReason::unbooked);
  std::printf("  PASS: unbooked and external-to-external cases rejected\n");
}

void testExternalToInternal() {
  auto ledger = makeTestLedger();
  const double before = ledger.cash(1);

  const auto decision = ledger.transfer(entities::accounts::employer(1),
                                        entities::accounts::account(2), 200.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(almostEqual(ledger.cash(1), before + 200.0));
  std::printf("  PASS: external-to-internal credits destination\n");
}

void testInternalToExternal() {
  auto ledger = makeTestLedger();

  const auto decision = ledger.transfer(entities::accounts::account(1),
                                        entities::accounts::employer(1), 300.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(almostEqual(ledger.cash(0), 700.0));
  std::printf("  PASS: internal-to-external debits source cash\n");
}

void testInternalToInternal() {
  auto ledger = makeTestLedger();

  const auto decision = ledger.transfer(entities::accounts::account(1),
                                        entities::accounts::account(2), 150.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(almostEqual(ledger.cash(0), 850.0));
  PL_CHECK(almostEqual(ledger.cash(1), 200.0));
  std::printf("  PASS: internal-to-internal moves funds correctly\n");
}

void testHubUnlimited() {
  auto ledger = makeTestLedger();

  const auto decision = ledger.transfer(
      entities::accounts::account(3), entities::accounts::account(2), 999999.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(std::isinf(ledger.liquidity(2)));
  PL_CHECK(std::isinf(ledger.availableCash(2)));
  PL_CHECK(almostEqual(ledger.cash(1), 50.0 + 999999.0));
  std::printf("  PASS: hub sends unlimited without debit\n");
}

void testInsufficientFunds() {
  auto ledger = makeTestLedger();

  const auto rejected = ledger.transfer(entities::accounts::account(2),
                                        entities::accounts::account(1), 51.0);
  PL_CHECK(rejected.rejected());
  PL_CHECK_EQ(rejected.reason(), clearing::RejectReason::unfunded);

  const auto accepted = ledger.transfer(entities::accounts::account(2),
                                        entities::accounts::account(1), 50.0);
  PL_CHECK(accepted.accepted());
  std::printf("  PASS: insufficient funds rejected, exact cash accepted\n");
}

void testProtectionLiquidity() {
  auto ledger = makeTestLedger();

  const auto decision = ledger.transfer(
      entities::accounts::account(1), entities::accounts::employer(1), 1350.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(almostEqual(ledger.cash(0), -350.0));
  std::printf("  PASS: overdraft/linked/courtesy count toward liquidity\n");
}

void testSelfTransferUsesCashOnly() {
  auto ledger = makeTestLedger();

  const auto rejected = ledger.transfer(entities::accounts::account(4),
                                        entities::accounts::account(1), 150.0,
                                        clearing::self_transfer_channel);
  PL_CHECK(rejected.rejected());
  PL_CHECK_EQ(rejected.reason(), clearing::RejectReason::unfunded);

  const auto accepted = ledger.transfer(entities::accounts::account(4),
                                        entities::accounts::account(1), 100.0,
                                        clearing::self_transfer_channel);
  PL_CHECK(accepted.accepted());
  std::printf("  PASS: self_transfer uses cash only\n");
}

void testOverdraftOnlySetup() {
  auto ledger = makeTestLedger();
  ledger.setOverdraftOnly(1, 5000.0);

  PL_CHECK(almostEqual(ledger.cash(1), 0.0));
  PL_CHECK(almostEqual(ledger.overdraft(1), 5000.0));
  PL_CHECK(almostEqual(ledger.linked(1), 0.0));
  PL_CHECK(almostEqual(ledger.courtesy(1), 0.0));

  const auto decision = ledger.transfer(
      entities::accounts::account(2), entities::accounts::employer(1), 4000.0);
  PL_CHECK(decision.accepted());
  std::printf("  PASS: overdraft-only setup works\n");
}

void testCloneRestore() {
  auto ledger = makeTestLedger();
  const auto snapshot = ledger.clone();

  const auto decision = ledger.transfer(entities::accounts::account(1),
                                        entities::accounts::account(2), 500.0);
  PL_CHECK(decision.accepted());
  PL_CHECK(almostEqual(ledger.cash(0), 500.0));

  ledger.restore(snapshot);
  PL_CHECK(almostEqual(ledger.cash(0), 1000.0));
  PL_CHECK(almostEqual(ledger.cash(1), 50.0));
  PL_CHECK(almostEqual(ledger.overdraft(0), 200.0));
  PL_CHECK(almostEqual(ledger.linked(0), 100.0));
  PL_CHECK(almostEqual(ledger.courtesy(0), 50.0));
  std::printf("  PASS: clone/restore preserves balances and buffers\n");
}

void testLiquidityQueries() {
  auto ledger = makeTestLedger();

  PL_CHECK_EQ(ledger.size(), 5U);
  PL_CHECK_EQ(ledger.findAccount(entities::accounts::account(1)), 0U);
  PL_CHECK_EQ(ledger.findAccount(entities::accounts::account(99)),
              clearing::Ledger::invalid);

  PL_CHECK(
      almostEqual(ledger.liquidity(entities::accounts::account(1)), 1350.0));
  PL_CHECK(almostEqual(ledger.availableCash(entities::accounts::account(1)),
                       1000.0));
  PL_CHECK(std::isinf(ledger.liquidity(entities::accounts::account(3))));
  PL_CHECK(std::isinf(ledger.availableCash(entities::accounts::account(3))));
  PL_CHECK(almostEqual(ledger.liquidity(entities::accounts::account(99)), 0.0));
  PL_CHECK(
      almostEqual(ledger.availableCash(entities::accounts::account(99)), 0.0));
  std::printf("  PASS: liquidity / availableCash queries\n");
}

} // namespace

int main() {
  std::printf("=== Ledger Tests (updated for clearing::Ledger) ===\n");
  testTransferDecisionHelpers();
  testInvalidAmounts();
  testUnbookedCases();
  testExternalToInternal();
  testInternalToExternal();
  testInternalToInternal();
  testHubUnlimited();
  testInsufficientFunds();
  testProtectionLiquidity();
  testSelfTransferUsesCashOnly();
  testOverdraftOnlySetup();
  testCloneRestore();
  testLiquidityQueries();
  std::printf("All ledger tests passed.\n\n");
  return 0;
}
