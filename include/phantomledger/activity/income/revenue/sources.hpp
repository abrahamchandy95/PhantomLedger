#pragma once

#include "phantomledger/activity/income/revenue/catalog.hpp"
#include "phantomledger/activity/income/revenue/draw.hpp"
#include "phantomledger/activity/income/revenue/profiles.hpp"
#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/ids.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::income::revenue {

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

struct Accounts {
  Key personal;
  Key revenueDst;
  std::optional<Key> business;
  std::optional<Key> brokerage;
};

struct Sources {
  std::vector<Key> clients;
  std::vector<Key> platforms;
  std::optional<Key> processor;
  std::optional<Key> drawSrc;
  std::optional<Key> investmentSrc;
  std::optional<Key> cashSrc;

  [[nodiscard]] bool any() const noexcept {
    return !clients.empty() || !platforms.empty() || processor.has_value() ||
           drawSrc.has_value() || investmentSrc.has_value() ||
           cashSrc.has_value();
  }

  void applyFallback(personas::Type persona, random::Rng &rng,
                     const RevenueCounterparties &counterparties,
                     const Accounts &accounts);
};

struct Plan {
  PersonId person = 0;
  personas::Type persona = personas::kDefaultType;
  const PersonaRevenue *profile = nullptr;
  Accounts accounts{};
  Sources sources{};

  [[nodiscard]] bool valid() const noexcept { return profile != nullptr; }
};

namespace detail {

[[nodiscard]] inline Accounts accountsFor(const Population &population,
                                          PersonId person,
                                          personas::Type persona,
                                          const Key &personal) {
  const auto businessKey = ::PhantomLedger::synth::businessId(person);
  const auto brokerageKey = ::PhantomLedger::synth::brokerageId(person);

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
                    const MultiSource &profile) {
  if (profile.activeP <= 0.0 || pool.empty()) {
    return {};
  }

  if (rng.nextDouble() >= profile.activeP) {
    return {};
  }

  // Note: assumes counterpartiesMin/Max have been correctly renamed in
  // MultiSource
  return choiceK(rng, pool, profile.minCount, profile.maxCount);
}

[[nodiscard]] inline std::optional<Key> source(random::Rng &rng,
                                               std::span<const Key> pool,
                                               const SingleSource &profile) {
  if (profile.activeP <= 0.0 || pool.empty()) {
    return std::nullopt;
  }

  if (rng.nextDouble() >= profile.activeP) {
    return std::nullopt;
  }

  return pickOne(rng, pool);
}

} // namespace detail

inline void Sources::applyFallback(personas::Type persona, random::Rng &rng,
                                   const RevenueCounterparties &counterparties,
                                   const Accounts &accounts) {
  if (any()) {
    return;
  }

  switch (persona) {
  case personas::Type::freelancer: {
    const auto clientPool = counterparties.clients();
    if (!clientPool.empty()) {
      clients = choiceK(rng, clientPool, 1, 2);
    }
    break;
  }

  case personas::Type::smallBusiness: {
    const auto ownerBusinesses = counterparties.ownerBusinesses();
    if (!accounts.business.has_value() && !ownerBusinesses.empty()) {
      drawSrc = pickOne(rng, ownerBusinesses);
    }
    break;
  }

  case personas::Type::highNetWorth: {
    if (accounts.brokerage.has_value()) {
      investmentSrc = accounts.brokerage;
      break;
    }

    const auto brokerages = counterparties.brokerages();
    if (!brokerages.empty()) {
      investmentSrc = pickOne(rng, brokerages);
    }
    break;
  }

  default:
    break;
  }
}

[[nodiscard]] inline std::optional<Plan> assign(const Book &book,
                                                PersonId person) {
  const auto &population = book.population;
  const auto &counterparties = book.counterparties;

  // Unrelated checks are now strictly separated for readability
  if (!counterparties.available()) {
    return std::nullopt;
  }

  if (!population.exists(person)) {
    return std::nullopt;
  }

  if (!population.hasAccount(person)) {
    return std::nullopt;
  }

  const auto personal = population.primary(person);
  if (population.hubs().contains(personal)) {
    return std::nullopt;
  }

  const auto persona = population.persona(person);

  // C++23 monadic transform: perfectly flat pipeline
  return lookupProfile(persona).transform([&](const PersonaRevenue &profile) {
    const auto accounts =
        detail::accountsFor(population, person, persona, personal);
    const auto personKey = std::to_string(static_cast<unsigned>(person));
    auto rng =
        book.entropy.factory.rng({"legit", "nonpayroll_income", personKey});

    Sources sources{
        .clients = detail::counterpartySources(rng, counterparties.clients(),
                                               profile.client),
        .platforms = detail::counterpartySources(
            rng, counterparties.platforms(), profile.platform),
        .processor = detail::source(rng, counterparties.processors(),
                                    profile.settlement),

        // C++23 or_else: lazily defers the RNG draw unless the
        // business/brokerage accounts don't exist
        .drawSrc = accounts.business.or_else([&] {
          return detail::source(rng, counterparties.ownerBusinesses(),
                                profile.ownerDraw);
        }),
        .investmentSrc = accounts.brokerage.or_else([&] {
          return detail::source(rng, counterparties.brokerages(),
                                profile.investment);
        }),
    };

    // Cash-takings source draws LAST so its addition leaves every
    // pre-existing per-person source draw unchanged
    if (const auto hub = counterparties.cashHub();
        hub.has_value() && profile.cashTakings.activeP > 0.0 &&
        rng.nextDouble() < profile.cashTakings.activeP) {
      sources.cashSrc = hub;
    }

    sources.applyFallback(persona, rng, counterparties, accounts);

    return Plan{
        .person = person,
        .persona = persona,
        .profile = &profile, // Safe: profile points to the static kCatalog
        .accounts = accounts,
        .sources = std::move(sources),
    };
  });
}

} // namespace source
} // namespace PhantomLedger::activity::income::revenue
