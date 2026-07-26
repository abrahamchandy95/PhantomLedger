#include "phantomledger/transfers/channels/insurance/claims.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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
               const ClaimPayout &payout, const Key &payer,
               std::int64_t deathEpoch) {
    const double claimP =
        windowClaimProbability(policy.annualClaimP, monthCount_);
    if (!claimRng.coin(claimP)) {
      return;
    }

    post(policy.carrierAcct, payer, sampleAmount(claimRng, payout),
         deathEpoch);
  }

private:
  void post(const Key &carrier, const Key &payer, double amount,
            std::int64_t deathEpoch) {
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

    // H3 part 3c-ii: claim filing is BEHAVIORAL — the dead file no
    // claims (unlike the contractual premiums, which the estate keeps
    // paying until closure). The skip sits AFTER the day/hour/minute
    // draws, so the shared timestamp stream is byte-identical.
    if (time::toEpochSeconds(ts) >= deathEpoch) {
      return;
    }

    // H1 step 2b (class P): the payout draw (with its behavioral
    // floor) is calibration-year dollars; the claim realizes at the
    // claim date's CPI level — scale AFTER the floored draw so the
    // floor scales with the amounts it screens (authority U-6).
    out_.push_back(txf_.make(transactions::Draft{
        .source = carrier,
        .destination = payer,
        .amount = primitives::utils::roundMoney(
            amount * synth::econ::priceScale(time::toCalendarDate(ts).year)),
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

ClaimScheduler::ClaimScheduler(const ClaimRates &rates,
                               random::Rng &timestampRng,
                               const transactions::Factory &txf,
                               const random::RngFactory &claimRngs) noexcept
    : rates_(&rates), timestampRng_(&timestampRng), txf_(&txf),
      claimRngs_(&claimRngs) {}

std::vector<transactions::Transaction>
ClaimScheduler::generate(const time::Window &window,
                         const entity::product::InsuranceLedger &insurance,
                         const Population &population) const {
  std::vector<transactions::Transaction> out;

  ClaimEmitter emitter{window, *timestampRng_, *txf_, out};
  if (!emitter.active()) {
    return out;
  }

  const auto deathEpochOf = [&](PersonId person) -> std::int64_t {
    if (population.timelines.empty() || person == 0 ||
        static_cast<std::size_t>(person) > population.timelines.size()) {
      return std::numeric_limits<std::int64_t>::max();
    }
    return time::toEpochSeconds(population.timelines[person - 1].death);
  };

  insurance.forEach([&](PersonId person,
                        const product::InsuranceHoldings &holdings) {
    const auto acctIt = population.primaryAccounts->find(person);
    if (acctIt == population.primaryAccounts->end()) {
      return;
    }

    const Key payer = acctIt->second;
    const auto deathEpoch = deathEpochOf(person);

    // Per-person sub-RNG isolates claim occurrence and amount sampling.
    auto personRng = claimRngs_->rng(
        {"insurance_claims", std::to_string(static_cast<unsigned>(person))});

    if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
      emitter.tryPost(personRng, *policy, rates_->autoPayout(), payer,
                      deathEpoch);
    }

    if (const auto &policy = holdings.homePolicy(); policy.has_value()) {
      emitter.tryPost(personRng, *policy, rates_->homePayout(), payer,
                      deathEpoch);
    }

    // Life insurance: death-benefit payouts are a REGISTERED upgrade
    // (the H3 estates model the wealth transfer; a policy payout to
    // the estate would double-count until sized together).
  });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
