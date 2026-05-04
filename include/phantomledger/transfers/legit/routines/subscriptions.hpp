#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

struct Config {
  std::int32_t minPerPerson = 4;
  std::int32_t maxPerPerson = 8;
  double debitP = 0.55;
  std::int32_t dayJitter = 1;

  void validate() const;
};

inline constexpr Config kDefaultConfig{};

class Generator {
public:
  Generator(random::Rng &rng, const transactions::Factory &txf,
            ledger::SeededScreen &screen, Config cfg = kDefaultConfig);

  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const blueprints::LegitBuildPlan &plan,
           const entity::account::Registry &registry);

private:
  random::Rng &rng_;
  const transactions::Factory &txf_;
  ledger::SeededScreen &screen_;
  Config cfg_;
};

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
