// TEMPORARY AUDIT PROBE — measures how many settled credit-channel rows
// (Credit::refund / Credit::chargeback) exist against the card view, and how
// many of them are merchant->card reversals of a row that IS in the card view.
// Not a gate. Delete after measuring.

#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <cstdio>
#include <map>
#include <set>
#include <utility>

namespace {

constexpr std::uint64_t kSeed = 0xC0FFEEULL;

using Txn = pltest::Txn;
namespace pl = ::PhantomLedger;
namespace ch = ::PhantomLedger::channels;

[[nodiscard]] bool inCardView(const Txn &t) noexcept {
  const auto c = t.session.channel;
  return c.value == ch::tag(ch::Legit::cardPurchase).value ||
         c.value == ch::tag(ch::Legit::merchant).value;
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  pltest::LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  opt.window.days = 1461;
  opt.population = 900;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  const auto leg = pltest::runLeg(pools, opt);

  std::size_t total = 0;
  std::size_t viewRows = 0;
  std::size_t viewFraud = 0;
  std::size_t refunds = 0;
  std::size_t chargebacks = 0;
  std::size_t cbFromViewSource = 0;
  std::size_t cbOnFraudPair = 0;
  std::size_t cbOnLegitPair = 0;
  std::set<pl::entity::Key> creditedCards;
  double refundAmt = 0.0;
  double cbAmt = 0.0;

  // Every (source,target) pair that appears in the card view, so we can ask
  // whether a credit reverses one of them.
  std::set<std::pair<pl::entity::Key, pl::entity::Key>> viewPairs;
  // (target,source) pairs of FRAUD card-view rows, and of LEGIT ones.
  std::set<std::pair<pl::entity::Key, pl::entity::Key>> fraudPairs;
  std::set<std::pair<pl::entity::Key, pl::entity::Key>> legitPairs;
  std::set<pl::entity::Key> fraudCards;
  std::set<pl::entity::Key> viewCards;

  for (const auto &t : leg.rows) {
    ++total;
    if (inCardView(t)) {
      ++viewRows;
      if (t.fraud.flag != 0) {
        ++viewFraud;
      }
      viewPairs.emplace(t.source, t.target);
      viewCards.insert(t.source);
      if (t.fraud.flag != 0) {
        fraudPairs.emplace(t.source, t.target);
        fraudCards.insert(t.source);
      } else {
        legitPairs.emplace(t.source, t.target);
      }
    }
  }

  for (const auto &t : leg.rows) {
    const auto c = t.session.channel;
    const bool isRefund = c.value == ch::tag(ch::Credit::refund).value;
    const bool isCb = c.value == ch::tag(ch::Credit::chargeback).value;
    if (!isRefund && !isCb) {
      continue;
    }
    if (isRefund) {
      ++refunds;
      refundAmt += t.amount;
    } else {
      ++chargebacks;
      cbAmt += t.amount;
    }
    // reversal: credit.source == purchase.target and credit.target ==
    // purchase.source for some card-view row.
    if (viewPairs.contains({t.target, t.source})) {
      ++cbFromViewSource;
    }
    if (isCb) {
      if (fraudPairs.contains({t.target, t.source})) {
        ++cbOnFraudPair;
      } else if (legitPairs.contains({t.target, t.source})) {
        ++cbOnLegitPair;
      }
      creditedCards.insert(t.target);
    }
  }

  std::printf("total settled rows        %zu\n", total);
  std::printf("card view rows            %zu  (fraud %zu)\n", viewRows,
              viewFraud);
  std::printf("Credit::refund rows       %zu  ($%.2f)\n", refunds, refundAmt);
  std::printf("Credit::chargeback rows   %zu  ($%.2f)\n", chargebacks, cbAmt);
  std::printf("credits reversing a CARD-VIEW (src,dst) pair  %zu\n",
              cbFromViewSource);
  std::printf("credit rows as %% of card view rows            %.4f%%\n",
              viewRows == 0 ? 0.0
                            : 100.0 * static_cast<double>(refunds + chargebacks)
                                  / static_cast<double>(viewRows));
  std::printf("chargebacks as %% of card-view FRAUD rows      %.4f%%\n",
              viewFraud == 0 ? 0.0
                             : 100.0 * static_cast<double>(chargebacks)
                                   / static_cast<double>(viewFraud));
  std::printf("chargebacks reversing a FRAUD card-view pair  %zu\n",
              cbOnFraudPair);
  std::printf("chargebacks reversing a LEGIT card-view pair  %zu\n",
              cbOnLegitPair);
  std::size_t creditedAndFraud = 0;
  for (const auto &k : creditedCards) {
    if (fraudCards.contains(k)) {
      ++creditedAndFraud;
    }
  }
  std::printf("cards receiving >=1 chargeback               %zu\n",
              creditedCards.size());
  std::printf("  of which ever-fraud in the view            %zu\n",
              creditedAndFraud);
  std::printf("cards in view %zu, ever-fraud %zu\n", viewCards.size(),
              fraudCards.size());
  const double prec = creditedCards.empty()
                          ? 0.0
                          : static_cast<double>(creditedAndFraud)
                                / static_cast<double>(creditedCards.size());
  const double base = viewCards.empty()
                          ? 0.0
                          : static_cast<double>(fraudCards.size())
                                / static_cast<double>(viewCards.size());
  std::printf("\n\"card got a chargeback => ever-fraud\": precision %.4f, "
              "base %.4f, lift %.2fx\n",
              prec, base, base > 0.0 ? prec / base : 0.0);
  return 0;
}
