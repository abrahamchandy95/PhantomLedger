#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud {

// ---------------------------------------------------------------------------
// Typology selection
// ---------------------------------------------------------------------------
using ::PhantomLedger::fraud::Typology;

struct TypologyWeights {
  double classic = 0.30;
  double layering = 0.15;
  double funnel = 0.10;
  double structuring = 0.10;
  double invoice = 0.05;
  double mule = 0.30;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::nonNegative("classic", classic); });
    r.check([&] { v::nonNegative("layering", layering); });
    r.check([&] { v::nonNegative("funnel", funnel); });
    r.check([&] { v::nonNegative("structuring", structuring); });
    r.check([&] { v::nonNegative("invoice", invoice); });
    r.check([&] { v::nonNegative("mule", mule); });
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
// Per-typology and camouflage rules
// ---------------------------------------------------------------------------

struct LayeringRules {
  std::int32_t minHops = 3;
  std::int32_t maxHops = 8;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::ge("minHops", minHops, 1); });
    r.check([&] { v::ge("maxHops", maxHops, minHops); });
  }
};

struct StructuringRules {
  double threshold = 10'000.0;
  double epsilonMin = 50.0;
  double epsilonMax = 400.0;
  std::int32_t splitsMin = 3;
  std::int32_t splitsMax = 12;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::positive("threshold", threshold); });
    r.check([&] { v::ge("epsilonMax", epsilonMax, epsilonMin); });
    r.check([&] { v::ge("splitsMin", splitsMin, 1); });
    r.check([&] { v::ge("splitsMax", splitsMax, splitsMin); });
  }
};

struct CamouflageRates {
  double smallP2pPerDayP = 0.03;
  double billMonthlyP = 0.35;
  double salaryInboundP = 0.12;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::unit("smallP2pPerDayP", smallP2pPerDayP); });
    r.check([&] { v::unit("billMonthlyP", billMonthlyP); });
    r.check([&] { v::unit("salaryInboundP", salaryInboundP); });
  }
};

struct InjectionOutput {
  std::vector<transactions::Transaction> txns;
  std::size_t injectedCount = 0;
};

// ---------------------------------------------------------------------------
// Execution contexts built by the injector and consumed by sub-generators.
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
