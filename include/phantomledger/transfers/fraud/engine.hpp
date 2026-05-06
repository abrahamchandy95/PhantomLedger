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
// Output of one injection pass.
// ---------------------------------------------------------------------------

struct InjectionOutput {
  std::vector<transactions::Transaction> txns;
  std::size_t injectedCount = 0;
};

// ---------------------------------------------------------------------------
// Execution / window types shared by every typology and by camouflage.
//
// These contexts intentionally do *not* carry per-typology rules.
// Each typology accepts its own rules as an explicit parameter so it
// neither sees nor depends on rules belonging to other typologies.
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

/// Pre-materialized account pools used by the camouflage layer.
struct AccountPools {
  std::vector<entity::Key> allAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
};

struct CamouflageContext {
  Execution execution;
  ActiveWindow window;
  const AccountPools *accounts = nullptr;
};

struct IllicitContext {
  Execution execution;
  ActiveWindow window;
  std::span<const entity::Key> billerAccounts{};
};

} // namespace PhantomLedger::transfers::fraud
