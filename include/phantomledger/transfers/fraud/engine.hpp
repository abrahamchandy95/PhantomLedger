#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::fraud {

// ---------------------------------------------------------------------------
// Typology selection
// ---------------------------------------------------------------------------

enum class Typology : std::uint8_t {
  classic = 0,
  layering = 1,
  funnel = 2,
  structuring = 3,
  invoice = 4,
  mule = 5,
};

struct TypologyWeights {
  double classic = 0.30;
  double layering = 0.15;
  double funnel = 0.10;
  double structuring = 0.10;
  double invoice = 0.05;
  double mule = 0.30;

  void validate() const {
    const std::array<double, 6> ws{classic,     layering, funnel,
                                   structuring, invoice,  mule};
    for (const auto w : ws) {
      if (w < 0.0) {
        throw std::invalid_argument("Fraud typology weights must be >= 0");
      }
    }
  }

  [[nodiscard]] Typology choose(random::Rng &rng) const {
    const std::array<double, 6> weights{classic,     layering, funnel,
                                        structuring, invoice,  mule};

    double total = 0.0;
    for (const auto w : weights) {
      total += w;
    }
    if (total <= 0.0) {
      return Typology::classic;
    }

    const auto cdf = distributions::buildCdf(
        std::span<const double>(weights.data(), weights.size()));
    const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
    return static_cast<Typology>(idx);
  }
};

// ---------------------------------------------------------------------------
// Per-typology rules
// ---------------------------------------------------------------------------

struct LayeringRules {
  std::int32_t minHops = 3;
  std::int32_t maxHops = 8;

  void validate() const {
    if (minHops < 1) {
      throw std::invalid_argument("LayeringRules.minHops must be >= 1");
    }
    if (maxHops < minHops) {
      throw std::invalid_argument("LayeringRules.maxHops must be >= minHops");
    }
  }
};

struct StructuringRules {
  double threshold = 10'000.0;
  double epsilonMin = 50.0;
  double epsilonMax = 400.0;
  std::int32_t splitsMin = 3;
  std::int32_t splitsMax = 12;

  void validate() const {
    if (threshold <= 0.0) {
      throw std::invalid_argument("StructuringRules.threshold must be > 0");
    }
    if (epsilonMax < epsilonMin) {
      throw std::invalid_argument(
          "StructuringRules.epsilonMax must be >= epsilonMin");
    }
    if (splitsMin < 1) {
      throw std::invalid_argument("StructuringRules.splitsMin must be >= 1");
    }
    if (splitsMax < splitsMin) {
      throw std::invalid_argument(
          "StructuringRules.splitsMax must be >= splitsMin");
    }
  }
};

struct CamouflageRates {
  double smallP2pPerDayP = 0.03;
  double billMonthlyP = 0.35;
  double salaryInboundP = 0.12;

  void validate() const {
    auto inUnit = [](double v) { return v >= 0.0 && v <= 1.0; };
    if (!inUnit(smallP2pPerDayP) || !inUnit(billMonthlyP) ||
        !inUnit(salaryInboundP)) {
      throw std::invalid_argument(
          "CamouflageRates probabilities must lie in [0, 1]");
    }
  }
};

struct Parameters {
  TypologyWeights typology{};
  LayeringRules layering{};
  StructuringRules structuring{};
  CamouflageRates camouflage{};

  void validate() const {
    typology.validate();
    layering.validate();
    structuring.validate();
    camouflage.validate();
  }
};

// ---------------------------------------------------------------------------
// Inputs from the pipeline
// ---------------------------------------------------------------------------

struct Scenario {
  const entities::synth::people::Fraud *fraudCfg = nullptr;
  time::Window window{};
  const entity::person::Roster *people = nullptr;
  const entity::person::Topology *topology = nullptr;
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::account::Ownership *ownership = nullptr;
  std::span<const transactions::Transaction> baseTxns{};
};

struct Runtime {
  random::Rng *rng = nullptr;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;
};

struct Counterparties {
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
};

struct InjectionInput {
  Scenario scenario{};
  Runtime runtime{};
  Counterparties counterparties{};
  Parameters params{};
};

struct InjectionOutput {
  std::vector<transactions::Transaction> txns;
  std::size_t injectedCount = 0;
};

// ---------------------------------------------------------------------------
// Execution contexts (built by the injector, consumed by sub-generators)
// ---------------------------------------------------------------------------

struct Execution {
  transactions::Factory txf;
  random::Rng *rng = nullptr;
};

struct ActiveWindow {
  time::TimePoint startDate{};
  std::int32_t days = 0;

  [[nodiscard]] time::TimePoint endExcl() const noexcept {
    return startDate + time::Days{days};
  }
};

/// Pre-materialized account pools. Keeping these as vectors of Keys avoids
/// re-walking the registry on every typology call.
struct AccountPools {
  std::vector<entity::Key> allAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
};

struct CamouflageContext {
  Execution execution;
  ActiveWindow window;
  const AccountPools *accounts = nullptr;
  CamouflageRates rates{};
};

struct IllicitContext {
  Execution execution;
  ActiveWindow window;
  std::span<const entity::Key> billerAccounts{};
  LayeringRules layeringRules{};
  StructuringRules structuringRules{};
};

} // namespace PhantomLedger::transfers::fraud
