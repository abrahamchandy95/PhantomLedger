#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/inflow/ids.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/revenue/catalog.hpp"
#include "phantomledger/inflows/revenue/draw.hpp"
#include "phantomledger/inflows/revenue/profiles.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::inflows::revenue {

struct Book {
  Timeframe timeframe;
  Entropy entropy;
  Population population;
  RevenueCounterparties counterparties;

  void validate(primitives::validate::Report &r) const {
    timeframe.validate(r);
  }
};

namespace source {

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
                          const RevenueCounterparties &counterparties,
                          const Accounts &accounts, Sources &sources) {
  if (sources.any()) {
    return;
  }

  switch (persona) {
  case personas::Type::freelancer: {
    const auto clients = counterparties.clients();
    if (!clients.empty()) {
      sources.clients = choiceK(rng, clients, 1, 2);
    }
    break;
  }

  case personas::Type::smallBusiness: {
    const auto ownerBusinesses = counterparties.ownerBusinesses();
    if (!accounts.business.has_value() && !ownerBusinesses.empty()) {
      sources.drawSrc = pickOne(rng, ownerBusinesses);
    }
    break;
  }

  case personas::Type::highNetWorth: {
    if (accounts.brokerage.has_value()) {
      sources.investmentSrc = accounts.brokerage;
      break;
    }

    const auto brokerages = counterparties.brokerages();
    if (!brokerages.empty()) {
      sources.investmentSrc = pickOne(rng, brokerages);
    }
    break;
  }

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
[[nodiscard]] inline std::optional<Plan> assign(const Book &book,
                                                PersonId person) {
  const auto &population = book.population;
  const auto &counterparties = book.counterparties;

  if (!counterparties.available()) {
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
      book.entropy.factory.rng({"legit", "nonpayroll_income", personKey});

  Sources sources{
      .clients = detail::counterpartySources(rng, counterparties.clients(),
                                             profile->client),
      .platforms = detail::counterpartySources(rng, counterparties.platforms(),
                                               profile->platform),
      .processor =
          detail::source(rng, counterparties.processors(), profile->settlement),
      .drawSrc = accounts.business.has_value()
                     ? accounts.business
                     : detail::source(rng, counterparties.ownerBusinesses(),
                                      profile->ownerDraw),
      .investmentSrc = accounts.brokerage.has_value()
                           ? accounts.brokerage
                           : detail::source(rng, counterparties.brokerages(),
                                            profile->investment),
  };

  detail::applyFallback(persona, rng, counterparties, accounts, sources);

  return Plan{
      .person = person,
      .persona = persona,
      .profile = profile,
      .accounts = accounts,
      .sources = std::move(sources),
  };
}

} // namespace source
} // namespace PhantomLedger::inflows::revenue
