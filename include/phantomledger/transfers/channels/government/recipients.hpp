#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transfers/channels/government/cohort.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::government {

struct Population {
  std::uint32_t count = 0;
  const entity::behavior::Assignment *personas = nullptr;
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

struct Recipient {
  entity::PersonId person{};
  entity::Key account{};
  double amount = 0.0;
  int ssaCohort = 0;
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

} // namespace detail

template <class Terms, class PersonaFilter>
[[nodiscard]] std::vector<Recipient>
select(const Population &pop, random::Rng &rng, const Terms &terms,
       PersonaFilter matches) {
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
    if (!rng.coin(terms.eligibleP)) {
      continue;
    }

    out.push_back(Recipient{
        pid,
        detail::primaryAccount(pop, pid),
        detail::sampleAmount(rng, terms.median, terms.sigma, terms.floor),
        time::ssaCohort(cohort::syntheticBirthDay(pid)),
    });
  }

  std::sort(out.begin(), out.end(),
            [](const auto &a, const auto &b) { return a.person < b.person; });
  return out;
}

} // namespace PhantomLedger::transfers::government
