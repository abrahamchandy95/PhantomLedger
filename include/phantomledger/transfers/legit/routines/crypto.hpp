#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::crypto {

/* This is deliberately a BANK-VISIBLE USD boundary model, not a blockchain
 * asset ledger. The modern adopter share is a cohort ceiling; the much smaller
 * monthly probabilities determine how often that cohort actually touches its
 * deposit account. */
struct Config {
  double modernAdopterP = 0.09;
  int firstActivityYear = 2013;
  int modernAnchorYear = 2024;

  double monthlyRampOutP = 0.10;
  double monthlyRampInP = 0.035;

  double rampOutMedian = 150.0;
  double rampOutSigma = 0.95;
  double rampOutMin = 15.0;
  double rampOutMax = 5'000.0;

  double rampInFractionMin = 0.20;
  double rampInFractionMax = 0.80;
  double rampInMin = 10.0;
  double rampInMax = 7'500.0;

  void validate() const;
};

inline constexpr Config kDefaultConfig{};

class Generator {
public:
  Generator(const transactions::Factory &txf,
            ::PhantomLedger::transfers::legit::ledger::SeededScreen &screen,
            Config cfg = kDefaultConfig);

  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const blueprints::LegitBlueprint &plan,
           const entity::account::Registry &registry);

private:
  const transactions::Factory &txf_;
  ::PhantomLedger::transfers::legit::ledger::SeededScreen &screen_;
  Config cfg_;
};

} // namespace PhantomLedger::transfers::legit::routines::crypto
