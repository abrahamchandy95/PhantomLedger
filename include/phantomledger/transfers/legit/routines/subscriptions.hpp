#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"
#include "phantomledger/transfers/subscriptions/bundle.hpp"
#include "phantomledger/transfers/subscriptions/schedule.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

using BundleRules = ::PhantomLedger::transfers::subscriptions::BundleRules;
using ScheduleRules = ::PhantomLedger::transfers::subscriptions::ScheduleRules;

inline constexpr BundleRules kDefaultBundleRules{};
inline constexpr ScheduleRules kDefaultScheduleRules{};

class DebitEmitter {
public:
  DebitEmitter(random::Rng &rng, const transactions::Factory &txf,
               ledger::SeededScreen &screen);

  DebitEmitter(random::Rng &rng, const transactions::Factory &txf,
               ledger::SeededScreen &screen, BundleRules rules);

  DebitEmitter(const DebitEmitter &) = delete;
  DebitEmitter &operator=(const DebitEmitter &) = delete;

  DebitEmitter &bundleRules(BundleRules rules);
  DebitEmitter &scheduleRules(ScheduleRules rules);

  [[nodiscard]] std::vector<transactions::Transaction>
  emitDebits(const blueprints::LegitBlueprint &plan,
             const entity::account::Registry &registry);

private:
  random::Rng &rng_;
  const transactions::Factory &txf_;
  ledger::SeededScreen &screen_;
  BundleRules bundleRules_ = kDefaultBundleRules;
  ScheduleRules scheduleRules_ = kDefaultScheduleRules;
};

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
