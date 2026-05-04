#include "phantomledger/transfers/insurance/claims.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace PhantomLedger::transfers::insurance {
namespace {

using entity::Key;
using entity::PersonId;
using time::TimePoint;

namespace product = entity::product;

[[nodiscard]] double windowClaimProbability(double annualP,
                                            std::size_t months) noexcept {
  if (annualP <= 0.0 || months == 0) {
    return 0.0;
  }

  const double years = static_cast<double>(months) / 12.0;

  return 1.0 - std::pow(1.0 - annualP, years);
}

[[nodiscard]] std::size_t claimMonthCount(const time::Window &window) {
  time::Almanac almanac{window};

  return almanac.monthAnchors().size();
}

[[nodiscard]] double sampleAmount(random::Rng &rng, const ClaimPayout &payout) {
  const auto raw = probability::distributions::lognormalByMedian(
      rng, payout.median, payout.sigma);

  return primitives::utils::floorAndRound(raw, payout.floor);
}

class ClaimEmitter {
public:
  ClaimEmitter(const time::Window &window, random::Rng &timestampRng,
               const transactions::Factory &txf,
               std::vector<transactions::Transaction> &out)
      : start_(window.start), endExcl_(window.endExcl()), days_(window.days),
        monthCount_(claimMonthCount(window)), timestampRng_(timestampRng),
        txf_(txf), out_(out) {}

  [[nodiscard]] bool active() const noexcept { return monthCount_ != 0; }

  void tryPost(random::Rng &claimRng, const product::InsurancePolicy &policy,
               const ClaimPayout &payout, const Key &payer) {
    const double claimP =
        windowClaimProbability(policy.annualClaimP, monthCount_);
    if (!claimRng.coin(claimP)) {
      return;
    }

    post(policy.carrierAcct, payer, sampleAmount(claimRng, payout));
  }

private:
  void post(const Key &carrier, const Key &payer, double amount) {
    if (amount <= 0.0) {
      return;
    }

    const auto dayOff = timestampRng_.uniformInt(0, std::max(1, days_));
    const auto hour = timestampRng_.uniformInt(9, 17);
    const auto minute = timestampRng_.uniformInt(0, 60);
    const auto ts =
        start_ + time::Days{dayOff} + time::Hours{hour} + time::Minutes{minute};

    if (ts >= endExcl_) {
      return;
    }

    out_.push_back(txf_.make(transactions::Draft{
        .source = carrier,
        .destination = payer,
        .amount = amount,
        .timestamp = time::toEpochSeconds(ts),
        .channel = channels::tag(channels::Insurance::claim),
    }));
  }

  TimePoint start_{};
  TimePoint endExcl_{};
  int days_ = 0;
  std::size_t monthCount_ = 0;

  random::Rng &timestampRng_;
  const transactions::Factory &txf_;
  std::vector<transactions::Transaction> &out_;
};

} // namespace

std::vector<transactions::Transaction>
claims(const ClaimRates &rates, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const entity::product::PortfolioRegistry &portfolios,
       const Population &population) {
  std::vector<transactions::Transaction> out;

  ClaimEmitter emitter{window, rng, txf, out};
  if (!emitter.active()) {
    return out;
  }

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;

        // Per-person sub-RNG isolates claim occurrence and amount sampling.
        auto personRng =
            factory.rng({"insurance_claims",
                         std::to_string(static_cast<unsigned>(person))});

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          emitter.tryPost(personRng, *policy, rates.autoPayout(), payer);
        }

        if (const auto &policy = holdings.homePolicy(); policy.has_value()) {
          emitter.tryPost(personRng, *policy, rates.homePayout(), payer);
        }

        // Life insurance: death events are not modeled.
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
