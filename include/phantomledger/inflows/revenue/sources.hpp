#pragma once
/*
 * inflows/revenue/sources.hpp — persona-aware revenue source assignment.
 */

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/inflow/ids.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/revenue/catalog.hpp"
#include "phantomledger/inflows/revenue/draw.hpp"
#include "phantomledger/inflows/revenue/profiles.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::inflows::revenue::source {

using Key = entity::Key;
using PersonId = entity::PersonId;

// ---------------------------------------------------------------
// Accounts
// ---------------------------------------------------------------

struct Accounts {
  Key personal;
  Key revenueDst;
  std::optional<Key> business;
  std::optional<Key> brokerage;
};

// ---------------------------------------------------------------
// Sources
// ---------------------------------------------------------------

struct Sources {
  std::vector<Key> clients;
  std::vector<Key> platforms;
  std::optional<Key> processor;
  std::optional<Key> drawSrc;
  std::optional<Key> investmentSrc;

  [[nodiscard]] bool any() const noexcept {
    return !clients.empty() || !platforms.empty() || processor.has_value() ||
           drawSrc.has_value() || investmentSrc.has_value();
  }
};

// ---------------------------------------------------------------
// Plan
// ---------------------------------------------------------------

struct Plan {
  PersonId person = 0;
  personas::Type persona = personas::kDefaultType;
  const RevenuePersonaProfile *profile = nullptr;
  Accounts accounts{};
  Sources sources{};

  [[nodiscard]] bool valid() const noexcept { return profile != nullptr; }
};

// ---------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------

namespace detail {

[[nodiscard]] inline Accounts accountsFor(const Population &population,
                                          PersonId person,
                                          personas::Type persona,
                                          const Key &personal) {
  const auto businessKey = entities::synth::inflow::businessId(person);
  const auto brokerageKey = entities::synth::inflow::brokerageId(person);

  std::optional<Key> business;
  if (population.owns(person, businessKey)) {
    business = businessKey;
  }

  std::optional<Key> brokerage;
  if (population.owns(person, brokerageKey)) {
    brokerage = brokerageKey;
  }

  const Key revenueDst = ((persona == personas::Type::freelancer ||
                           persona == personas::Type::smallBusiness) &&
                          business.has_value())
                             ? *business
                             : personal;

  return Accounts{
      .personal = personal,
      .revenueDst = revenueDst,
      .business = business,
      .brokerage = brokerage,
  };
}

[[nodiscard]] inline std::vector<Key>
counterpartySources(random::Rng &rng, std::span<const Key> pool,
                    const CounterpartyRevenueProfile &profile) {
  if (profile.activeP <= 0.0 || pool.empty()) {
    return {};
  }

  if (rng.nextDouble() >= profile.activeP) {
    return {};
  }

  return choiceK(rng, pool, profile.counterpartiesMin,
                 profile.counterpartiesMax);
}

[[nodiscard]] inline std::optional<Key>
source(random::Rng &rng, std::span<const Key> pool,
       const SingleSourceRevenueProfile &profile) {
  if (profile.activeP <= 0.0 || pool.empty()) {
    return std::nullopt;
  }

  if (rng.nextDouble() >= profile.activeP) {
    return std::nullopt;
  }

  return pickOne(rng, pool);
}

inline void applyFallback(personas::Type persona, random::Rng &rng,
                          const entity::counterparty::Pool &pools,
                          const Accounts &accounts, Sources &sources) {
  if (sources.any()) {
    return;
  }

  switch (persona) {
  case personas::Type::freelancer:
    if (!pools.clientPayerIds.empty()) {
      sources.clients = choiceK(rng, pools.clientPayerIds, 1, 2);
    }
    break;

  case personas::Type::smallBusiness:
    if (!accounts.business.has_value() && !pools.ownerBusinessIds.empty()) {
      sources.drawSrc = pickOne(rng, pools.ownerBusinessIds);
    }
    break;

  case personas::Type::highNetWorth:
    if (accounts.brokerage.has_value()) {
      sources.investmentSrc = accounts.brokerage;
    } else if (!pools.brokerageIds.empty()) {
      sources.investmentSrc = pickOne(rng, pools.brokerageIds);
    }
    break;

  default:
    break;
  }
}

} // namespace detail

// ---------------------------------------------------------------
// Public API
// ---------------------------------------------------------------

/// Assign revenue sources for one person.
/// Returns nullopt if the person is ineligible.
[[nodiscard]] inline std::optional<Plan> assign(const InflowSnapshot &snapshot,
                                                PersonId person) {
  const auto &population = snapshot.population;
  const auto &counterparties = snapshot.counterparties;

  if (!counterparties.hasPools()) {
    return std::nullopt;
  }

  if (!population.exists(person) || !population.hasAccount(person)) {
    return std::nullopt;
  }

  const auto personal = population.primary(person);
  if (population.hubs.contains(personal)) {
    return std::nullopt;
  }

  const auto persona = population.persona(person);
  const auto &profileOpt = archetypeFor(persona);
  if (!profileOpt.has_value()) {
    return std::nullopt;
  }

  const auto *profile = &*profileOpt;
  const auto accounts =
      detail::accountsFor(population, person, persona, personal);

  const auto personKey = std::to_string(static_cast<unsigned>(person));
  auto rng =
      snapshot.entropy.factory.rng({"legit", "nonpayroll_income", personKey});

  const auto &pools = *counterparties.pools;

  Sources sources{
      .clients = detail::counterpartySources(rng, pools.clientPayerIds,
                                             profile->client),
      .platforms = detail::counterpartySources(rng, pools.platformIds,
                                               profile->platform),
      .processor = detail::source(rng, pools.processorIds, profile->settlement),
      .drawSrc =
          accounts.business.has_value()
              ? accounts.business
              : detail::source(rng, pools.ownerBusinessIds, profile->ownerDraw),
      .investmentSrc =
          accounts.brokerage.has_value()
              ? accounts.brokerage
              : detail::source(rng, pools.brokerageIds, profile->investment),
  };

  detail::applyFallback(persona, rng, pools, accounts, sources);

  return Plan{
      .person = person,
      .persona = persona,
      .profile = profile,
      .accounts = accounts,
      .sources = std::move(sources),
  };
}

} // namespace PhantomLedger::inflows::revenue::source
