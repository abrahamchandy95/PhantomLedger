#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"

#include <vector>

namespace PhantomLedger::transfers::legit::routines::deposits {

/* Conservative household-deposit behavior. Business takings remain owned by
 * activity::income::revenue; this routine covers occasional personal cash and
 * paper-check deposits only. Each probability is evaluated on a content-keyed
 * RngFactory lane derived from LegitBlueprint::seed(), never on the shared
 * routines stream. */
struct FlowConfig {
  double userP = 0.0;
  double monthlyP = 0.0;
  double median = 0.0;
  double sigma = 0.0;
  double floor = 0.0;
  double cap = 0.0;
  double roundTo = 0.0;

  void validate(const char *prefix) const;
};

struct Config {
  FlowConfig cash{
      .userP = 0.18,
      .monthlyP = 0.10,
      .median = 180.0,
      .sigma = 0.75,
      .floor = 20.0,
      .cap = 2'000.0,
      .roundTo = 10.0,
  };
  FlowConfig check{
      .userP = 0.30,
      .monthlyP = 0.15,
      .median = 425.0,
      .sigma = 0.90,
      .floor = 25.0,
      .cap = 5'000.0,
      .roundTo = 0.01,
  };

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

} // namespace PhantomLedger::transfers::legit::routines::deposits
