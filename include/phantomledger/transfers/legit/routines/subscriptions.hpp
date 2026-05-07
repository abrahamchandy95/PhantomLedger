#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"
#include "phantomledger/transfers/subscriptions/bundle.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

using BundleRules = ::PhantomLedger::transfers::subscriptions::BundleRules;

inline constexpr BundleRules kDefaultBundleRules{};

class DebitEmitter {
public:
  DebitEmitter(random::Rng &rng, const transactions::Factory &txf,
               ledger::SeededScreen &screen,
               BundleRules rules = kDefaultBundleRules);

  DebitEmitter(const DebitEmitter &) = delete;
  DebitEmitter &operator=(const DebitEmitter &) = delete;

  [[nodiscard]] std::vector<transactions::Transaction>
  emitDebits(const blueprints::LegitBlueprint &plan,
             const entity::account::Registry &registry);

private:
  random::Rng &rng_;
  const transactions::Factory &txf_;
  ledger::SeededScreen &screen_;
  BundleRules rules_;
};

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
