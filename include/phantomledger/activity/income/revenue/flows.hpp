#pragma once

#include "phantomledger/activity/income/revenue/clock.hpp"
#include "phantomledger/activity/income/revenue/draw.hpp"
#include "phantomledger/activity/income/revenue/profiles.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::activity::income::revenue::flow {

using Key = entity::Key;

namespace detail {

struct Rule {
  double floor = 0.0;
  BusinessDayWindow window{};
  channels::Tag channel = channels::none;
  // Snap amounts to whole multiples (0 = none). Cash takings deposit
  // in bills, so they land on a $10 lattice — which also makes exactly
  // $10,000.00 reachable in production data, exercising the strict CTR
  // boundary (31 CFR 1010.311: exactly $10,000 files nothing).
  double roundTo = 0.0;
};

[[nodiscard]] inline double amount(random::Rng &rng, double median,
                                   double sigma, double floor) {
  return std::max(
      floor, probability::distributions::lognormalByMedian(rng, median, sigma));
}

inline constexpr Rule kClients{
    .floor = 75.0,
    .window =
        {
            .earliestHour = 8,
            .latestHour = 17,
            .startDay = 0,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::clientAchCredit),
};

inline constexpr Rule kPlatforms{
    .floor = 25.0,
    .window =
        {
            .earliestHour = 6,
            .latestHour = 11,
            .startDay = 0,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::platformPayout),
};

inline constexpr Rule kSettlements{
    .floor = 20.0,
    .window =
        {
            .earliestHour = 5,
            .latestHour = 9,
            .startDay = 0,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::cardSettlement),
};

inline constexpr Rule kDraws{
    .floor = 100.0,
    .window =
        {
            .earliestHour = 10,
            .latestHour = 17,
            .startDay = 8,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::ownerDraw),
};

inline constexpr Rule kInvestments{
    .floor = 250.0,
    .window =
        {
            .earliestHour = 7,
            .latestHour = 15,
            .startDay = 0,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::investmentInflow),
};

// Branch hours: cash is deposited over the counter, weekdays 9–17
// (businessDayTs already rolls off weekends).
inline constexpr Rule kCashTakings{
    .floor = 100.0,
    .window =
        {
            .earliestHour = 9,
            .latestHour = 17,
            .startDay = 0,
            .endDayExclusive = 28,
        },
    .channel = channels::tag(channels::Legit::cashDeposit),
    .roundTo = 10.0,
};

} // namespace detail

// one month flow
class Cycle {
public:
  Cycle(random::Rng &rng, time::Window window, time::TimePoint monthStart,
        const transactions::Factory &factory) noexcept
      : rng_(rng), window_(window), monthStart_(monthStart), factory_(factory),
        // H1 step 2b (class W): freelancer/business revenue rides the
        // labor-income axis; the month's wage index turns the
        // calibration-year draws into era-correct nominal amounts.
        wageScale_(
            synth::econ::wageScale(time::toCalendarDate(monthStart).year)) {}

  // -------- public verbs (one per flow type) --------

  void clients(const MultiSource &profile, const Key &dst,
               std::span<const Key> sources) {
    counterparty(profile, dst, sources, detail::kClients);
  }

  void platforms(const MultiSource &profile, const Key &dst,
                 std::span<const Key> sources) {
    counterparty(profile, dst, sources, detail::kPlatforms);
  }

  void settlements(const SingleSource &profile, const Key &dst,
                   std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kSettlements);
  }

  void draws(const SingleSource &profile, const Key &dst,
             std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kDraws);
  }

  void investments(const SingleSource &profile, const Key &dst,
                   std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kInvestments);
  }

  void cashTakings(const SingleSource &profile, const Key &dst,
                   std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kCashTakings);
  }

  /// Move all generated transactions into the caller's sink.
  /// Rvalue-qualified to express one-shot consumption: after the
  /// move, this Cycle is empty and should be discarded.
  void drainInto(std::vector<transactions::Transaction> &sink) && {
    sink.insert(sink.end(), std::make_move_iterator(txns_.begin()),
                std::make_move_iterator(txns_.end()));
    txns_.clear();
  }

private:
  void counterparty(const MultiSource &profile, const Key &dst,
                    std::span<const Key> sources, const detail::Rule &rule) {
    if (sources.empty()) {
      return;
    }

    const int n = paymentCount(rng_, profile.paymentsMin, profile.paymentsMax);

    for (int i = 0; i < n; ++i) {
      const auto src = pickOne(rng_, sources);
      if (!src.has_value()) {
        continue;
      }

      append(*src, dst,
             detail::amount(rng_, profile.median, profile.sigma, rule.floor),
             rule);
    }
  }

  void singleSource(const SingleSource &profile, const Key &dst,
                    std::optional<Key> src, const detail::Rule &rule) {
    if (!src.has_value()) {
      return;
    }

    const int n = paymentCount(rng_, profile.paymentsMin, profile.paymentsMax);

    for (int i = 0; i < n; ++i) {
      append(*src, dst,
             detail::amount(rng_, profile.median, profile.sigma, rule.floor),
             rule);
    }
  }

  void append(const Key &src, const Key &dst, double amount,
              const detail::Rule &rule) {
    // H1 step 2b: scale AFTER the draw (the floored draw carries the
    // behavioral floor, so the floor scales with the amount), BEFORE
    // the bill lattice — the $10 denomination itself stays nominal
    // (class S), so a 1991 deposit is fewer bills, not scaled bills.
    amount *= wageScale_;
    if (rule.roundTo > 0.0) {
      amount = std::max(rule.floor * wageScale_,
                        std::round(amount / rule.roundTo) * rule.roundTo);
    }
    if (amount <= 0.0) {
      return;
    }

    const auto timestamp = businessDayTs(monthStart_, rng_, rule.window);

    if (timestamp < window_.start || timestamp >= window_.endExcl()) {
      return;
    }

    txns_.push_back(factory_.make(transactions::Draft{
        .source = src,
        .destination = dst,
        .amount = primitives::utils::roundMoney(amount),
        .timestamp = time::toEpochSeconds(timestamp),
        .isFraud = 0,
        .ringId = -1,
        .channel = rule.channel,
    }));
  }

  random::Rng &rng_;
  time::Window window_;
  time::TimePoint monthStart_;
  const transactions::Factory &factory_;
  double wageScale_ = 1.0;
  std::vector<transactions::Transaction> txns_;
};

} // namespace PhantomLedger::activity::income::revenue::flow
