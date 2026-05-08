#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/government/cohort.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::government {

struct Population {
  std::uint32_t count = 0;
  const entity::behavior::Assignment *personas = nullptr;
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

/// One eligible person with their resolved primary account, sampled
/// monthly amount, and SSA cohort.
struct Recipient {
  entity::PersonId person{};
  entity::Key account{};
  double amount = 0.0;
  int ssaCohort = 0;
};

struct BenefitDeposit {
  entity::Key source{};
  channels::Tag channel{};
};

namespace detail {

[[nodiscard]] inline bool hasAccount(const Population &pop,
                                     entity::PersonId pid) noexcept {
  const auto &own = *pop.ownership;
  return own.byPersonOffset[pid - 1] != own.byPersonOffset[pid];
}

[[nodiscard]] inline entity::Key primaryAccount(const Population &pop,
                                                entity::PersonId pid) noexcept {
  const auto idx = pop.ownership->primaryIndex(pid);
  return pop.accounts->records[idx].id;
}

[[nodiscard]] inline double sampleAmount(random::Rng &rng, double median,
                                         double sigma, double floor) {
  const auto raw =
      probability::distributions::lognormalByMedian(rng, median, sigma);
  return primitives::utils::floorAndRound(raw, floor);
}

[[nodiscard]] inline time::TimePoint
morningJitter(random::Rng &rng, time::TimePoint base) noexcept {
  const auto hour = rng.uniformInt(6, 10);
  const auto minute = rng.uniformInt(0, 60);
  return base + time::Hours{hour} + time::Minutes{minute};
}

} // namespace detail

template <class PersonaFilter>
[[nodiscard]] std::vector<Recipient>
select(const Population &pop, random::Rng &rng, double eligibleP, double median,
       double sigma, double floor, PersonaFilter matches) {
  std::vector<Recipient> out;
  out.reserve(pop.count / 4);

  for (entity::PersonId pid = 1; pid <= pop.count; ++pid) {
    if (!detail::hasAccount(pop, pid)) {
      continue;
    }
    const auto persona = pop.personas->byPerson[pid - 1];
    if (!matches(persona)) {
      continue;
    }
    if (!rng.coin(eligibleP)) {
      continue;
    }

    out.push_back(Recipient{
        pid,
        detail::primaryAccount(pop, pid),
        detail::sampleAmount(rng, median, sigma, floor),
        time::ssaCohort(cohort::syntheticBirthDay(pid)),
    });
  }

  std::sort(out.begin(), out.end(),
            [](const auto &a, const auto &b) { return a.person < b.person; });
  return out;
}

class MonthlyDepositEmitter {
public:
  MonthlyDepositEmitter(const time::Window &window, random::Rng &rng,
                        const transactions::Factory &txf)
      : window_(window), almanac_(window), rng_(rng), txf_(txf) {}

  [[nodiscard]] bool active() const noexcept {
    return !almanac_.monthAnchors().empty();
  }

  [[nodiscard]] std::vector<transactions::Transaction>
  emit(std::span<const Recipient> recipients, BenefitDeposit deposit) {
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
        const auto ts = detail::morningJitter(rng_, cohorts[r.ssaCohort][m]);
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

private:
  time::Window window_{};
  time::Almanac almanac_;
  random::Rng &rng_;
  const transactions::Factory &txf_;
};

} // namespace PhantomLedger::transfers::government
