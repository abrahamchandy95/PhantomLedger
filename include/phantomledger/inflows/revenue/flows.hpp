#pragma once
/*
 * per-month revenue flow drafters.
 *
 * Five flow drafters, each producing transactions for one
 * revenue channel:
 *   - clients      → CLIENT_ACH_CREDIT
 *   - platforms    → PLATFORM_PAYOUT
 *   - settlements  → CARD_SETTLEMENT
 *   - draws        → OWNER_DRAW
 *   - investments  → INVESTMENT_INFLOW
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/inflows/revenue/clock.hpp"
#include "phantomledger/inflows/revenue/draw.hpp"
#include "phantomledger/inflows/revenue/profiles.hpp"
#include "phantomledger/math/sampling.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/time/calendar.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::inflows::revenue::flow {

using Key = entities::identifier::Key;

// ---------------------------------------------------------------
// Cycle — one month of flow drafting
// ---------------------------------------------------------------

class Cycle {
public:
  Cycle(random::Rng &rng, time::TimePoint monthStart,
        time::TimePoint windowStart, time::TimePoint endExcl,
        const transactions::Factory &txf,
        std::vector<transactions::Transaction> &txns)
      : rng_(rng), monthStart_(monthStart), windowStart_(windowStart),
        endExcl_(endExcl), txf_(txf), txns_(txns) {}

  [[nodiscard]] random::Rng &rng() noexcept { return rng_; }

  [[nodiscard]] time::TimePoint ts(BusinessDayWindow window) {
    return businessDayTs(monthStart_, rng_, window);
  }

  void append(const Key &src, const Key &dst, double amount,
              time::TimePoint timestamp, channels::Tag channel) {
    if (amount <= 0.0) {
      return;
    }

    if (timestamp < windowStart_ || timestamp >= endExcl_) {
      return;
    }

    txns_.push_back(txf_.make(transactions::Draft{
        .source = src,
        .destination = dst,
        .amount = math::roundMoney(amount),
        .timestamp = time::toEpochSeconds(timestamp),
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

private:
  random::Rng &rng_;
  time::TimePoint monthStart_;
  time::TimePoint windowStart_;
  time::TimePoint endExcl_;
  const transactions::Factory &txf_;
  std::vector<transactions::Transaction> &txns_;
};

// ---------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------

namespace detail {

struct Rule {
  double floor = 0.0;
  BusinessDayWindow window{};
  channels::Tag channel = channels::none;
};

[[nodiscard]] inline double amount(random::Rng &rng, double median,
                                   double sigma, double floor) {
  return std::max(floor, math::lognormalByMedian(rng, median, sigma));
}

inline void counterparty(Cycle &cycle,
                         const CounterpartyRevenueProfile &profile,
                         const Key &dst, std::span<const Key> sources,
                         const Rule &rule) {
  if (sources.empty()) {
    return;
  }

  const int n =
      paymentCount(cycle.rng(), profile.paymentsMin, profile.paymentsMax);

  for (int i = 0; i < n; ++i) {
    const auto src = pickOne(cycle.rng(), sources);
    if (!src.has_value()) {
      continue;
    }

    cycle.append(*src, dst,
                 amount(cycle.rng(), profile.median, profile.sigma, rule.floor),
                 cycle.ts(rule.window), rule.channel);
  }
}

inline void singleSource(Cycle &cycle,
                         const SingleSourceRevenueProfile &profile,
                         const Key &dst, std::optional<Key> src,
                         const Rule &rule) {
  if (!src.has_value()) {
    return;
  }

  const int n =
      paymentCount(cycle.rng(), profile.paymentsMin, profile.paymentsMax);

  for (int i = 0; i < n; ++i) {
    cycle.append(*src, dst,
                 amount(cycle.rng(), profile.median, profile.sigma, rule.floor),
                 cycle.ts(rule.window), rule.channel);
  }
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

// ---------------------------------------------------------------
// Public flow drafters
// ---------------------------------------------------------------

inline void clients(Cycle &cycle, const CounterpartyRevenueProfile &profile,
                    const Key &dst, std::span<const Key> sources) {
  detail::counterparty(cycle, profile, dst, sources, detail::kClients);
}

inline void platforms(Cycle &cycle, const CounterpartyRevenueProfile &profile,
                      const Key &dst, std::span<const Key> sources) {
  detail::counterparty(cycle, profile, dst, sources, detail::kPlatforms);
}

inline void settlements(Cycle &cycle, const SingleSourceRevenueProfile &profile,
                        const Key &dst, std::optional<Key> src) {
  detail::singleSource(cycle, profile, dst, src, detail::kSettlements);
}

inline void draws(Cycle &cycle, const SingleSourceRevenueProfile &profile,
                  const Key &dst, std::optional<Key> src) {
  detail::singleSource(cycle, profile, dst, src, detail::kDraws);
}

inline void investments(Cycle &cycle, const SingleSourceRevenueProfile &profile,
                        const Key &dst, std::optional<Key> src) {
  detail::singleSource(cycle, profile, dst, src, detail::kInvestments);
}

} // namespace PhantomLedger::inflows::revenue::flow
