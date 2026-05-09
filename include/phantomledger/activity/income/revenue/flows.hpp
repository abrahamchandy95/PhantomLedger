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
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::inflows::revenue::flow {

using Key = entity::Key;

namespace detail {

struct Rule {
  double floor = 0.0;
  BusinessDayWindow window{};
  channels::Tag channel = channels::none;
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

} // namespace detail

// one month flow
class Cycle {
public:
  Cycle(random::Rng &rng, time::Window window, time::TimePoint monthStart,
        const transactions::Factory &factory) noexcept
      : rng_(rng), window_(window), monthStart_(monthStart), factory_(factory) {
  }

  // -------- public verbs (one per flow type) --------

  void clients(const CounterpartyRevenueProfile &profile, const Key &dst,
               std::span<const Key> sources) {
    counterparty(profile, dst, sources, detail::kClients);
  }

  void platforms(const CounterpartyRevenueProfile &profile, const Key &dst,
                 std::span<const Key> sources) {
    counterparty(profile, dst, sources, detail::kPlatforms);
  }

  void settlements(const SingleSourceRevenueProfile &profile, const Key &dst,
                   std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kSettlements);
  }

  void draws(const SingleSourceRevenueProfile &profile, const Key &dst,
             std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kDraws);
  }

  void investments(const SingleSourceRevenueProfile &profile, const Key &dst,
                   std::optional<Key> src) {
    singleSource(profile, dst, src, detail::kInvestments);
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
  void counterparty(const CounterpartyRevenueProfile &profile, const Key &dst,
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

  void singleSource(const SingleSourceRevenueProfile &profile, const Key &dst,
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
  std::vector<transactions::Transaction> txns_;
};

} // namespace PhantomLedger::inflows::revenue::flow
