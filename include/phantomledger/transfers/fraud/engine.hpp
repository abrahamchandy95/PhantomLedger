#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
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

    const auto cdf = probability::distributions::buildCdf(
        std::span<const double>(weights.data(), weights.size()));
    const auto idx =
        probability::distributions::sampleIndex(cdf, rng.nextDouble());
    return static_cast<Typology>(idx);
  }
};

struct InjectionOutput {
  std::vector<transactions::Transaction> txns;
  std::size_t injectedCount = 0;
};

struct Execution {
  transactions::Factory txf;
  random::Rng *rng = nullptr;
};

/// Pre-materialized account pools used by the camouflage layer.
struct AccountPools {
  std::vector<entity::Key> allAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
};

struct CamouflageContext {
  Execution execution;
  time::Window window;
  const AccountPools *accounts = nullptr;
};

struct IllicitContext {
  Execution execution;
  time::Window window;
  std::span<const entity::Key> billerAccounts{};

  // Monotonic counter for chain id allocation. Each call to allocateChainId()
  // returns a fresh id and bumps the counter. Shared across all rings within
  // one Inject() call, so chain ids are globally unique within a run.
  std::uint32_t nextChainId = 1;

  /// Allocate a fresh chain id and bump the internal counter.
  /// Returns the id as a signed int so it can be stored on Draft::chainId
  /// (which uses -1 as the "no chain" sentinel).
  [[nodiscard]] std::int32_t allocateChainId() noexcept {
    const auto id = nextChainId++;
    return static_cast<std::int32_t>(id);
  }
};

} // namespace PhantomLedger::transfers::fraud
