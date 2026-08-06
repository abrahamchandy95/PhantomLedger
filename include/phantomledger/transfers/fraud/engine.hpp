#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
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

  std::vector<transactions::Transaction> injected;
};

struct Execution {
  transactions::Factory txf;
  random::Rng *rng = nullptr;

  const random::RngFactory *factory = nullptr;
};

/* Pre-materialized account pools used by the camouflage layer. */
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

  /* The MERCHANT ACCEPTANCE catalogue and the geographic axis, forwarded
   * verbatim from InjectorLegitCounterparties — see that header for why the
   * catalogue itself is the right carrier. A null catalogue and an empty span
   * leave every draw and every golden byte in place. */
  const entity::merchant::Catalog *merchants = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};

  /* Attacker infrastructure, forwarded from InjectorServices. Read ONLY to
   * size the campaign-shared cash-out slot; selection itself is a draw-free
   * hash of the campaign index, so this pointer adds no uniforms and no state.
   * It is filled inside Injector::inject where `services_.attackers` is
   * already in scope, so it needs no change at any inject call site and no new
   * harness carrier. */
  const infra::AttackerInfra *attackers = nullptr;

  std::uint32_t nextChainId = 1;

  void seedChainIds(std::uint32_t ringId) noexcept {
    nextChainId = (ringId << 16U) | 1U;
  }

  [[nodiscard]] std::int32_t allocateChainId() noexcept {
    const auto id = nextChainId++;
    return static_cast<std::int32_t>(id);
  }
};

} // namespace PhantomLedger::transfers::fraud
