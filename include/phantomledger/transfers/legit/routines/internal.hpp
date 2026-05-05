#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::internal_xfer {

struct Config {
  double activeP = 0.55;
  std::int32_t transfersPerMonthMin = 1;
  std::int32_t transfersPerMonthMax = 3;
  double amountMedian = 120.0;
  double amountSigma = 0.75;
  double roundAmountP = 0.25;

  void validate() const;
};

inline constexpr Config kDefaultConfig{};

class Generator {
public:
  Generator(random::Rng &rng, const transactions::Factory &txf,
            ledger::SeededScreen &screen) noexcept
      : rng_(rng), txf_(txf), screen_(screen) {}

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const blueprints::LegitBlueprint &plan, Config cfg = kDefaultConfig);

private:
  random::Rng &rng_;
  const transactions::Factory &txf_;
  ledger::SeededScreen &screen_;
};

} // namespace PhantomLedger::transfers::legit::routines::internal_xfer
