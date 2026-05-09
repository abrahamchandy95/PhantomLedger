#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/government/recipients.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::government {

/// What a monthly benefit deposit looks like at the source side:
/// which counterparty pays, on which channel.
struct BenefitDeposit {
  entity::Key source{};
  channels::Tag channel{};
};

/// Emits monthly benefit deposits across an active window. Holds the
/// window, almanac, rng, and transaction factory as long-lived
/// collaborators so `emit` takes only the per-program inputs.
class MonthlyDepositEmitter {
public:
  MonthlyDepositEmitter(const time::Window &window, random::Rng &rng,
                        const transactions::Factory &txf)
      : window_(window), almanac_(window), rng_(rng), txf_(txf) {}

  /// True when the window contains at least one whole month — i.e.
  /// at least one deposit cycle is reachable.
  [[nodiscard]] bool active() const noexcept {
    return !almanac_.monthAnchors().empty();
  }

  [[nodiscard]] std::vector<transactions::Transaction>
  emit(std::span<const Recipient> recipients, BenefitDeposit deposit);

private:
  [[nodiscard]] time::TimePoint morningJitter(time::TimePoint base) noexcept {
    const auto hour = rng_.uniformInt(6, 10);
    const auto minute = rng_.uniformInt(0, 60);
    return base + time::Hours{hour} + time::Minutes{minute};
  }

  time::Window window_{};
  time::Almanac almanac_;
  random::Rng &rng_;
  const transactions::Factory &txf_;
};

inline std::vector<transactions::Transaction>
MonthlyDepositEmitter::emit(std::span<const Recipient> recipients,
                            BenefitDeposit deposit) {
  std::vector<transactions::Transaction> out;
  if (recipients.empty()) {
    return out;
  }

  const std::span<const time::TimePoint> cohorts[3] = {
      almanac_.ssaPayDates(0),
      almanac_.ssaPayDates(1),
      almanac_.ssaPayDates(2),
  };

  const auto months = cohorts[0].size();
  for (std::size_t m = 0; m < months; ++m) {
    for (const auto &r : recipients) {
      const auto ts = morningJitter(cohorts[r.ssaCohort][m]);
      if (ts < window_.start || ts >= window_.endExcl()) {
        continue;
      }
      out.push_back(txf_.make(transactions::Draft{
          .source = deposit.source,
          .destination = r.account,
          .amount = r.amount,
          .timestamp = time::toEpochSeconds(ts),
          .channel = deposit.channel,
      }));
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::government
