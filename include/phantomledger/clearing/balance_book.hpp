#pragma once
/*
 * Balance book initialization.
 *
 * Bootstraps the Ledger with per-persona randomized balances and
 * liquidity protection buffers (overdraft, linked-account sweep,
 * courtesy cushion).
 * The Ledger itself handles transfer mechanics. This module handles
 * the one-time setup that precedes the simulation loop.
 */

#include "phantomledger/clearing/ledger.hpp"
#include "phantomledger/entities/accounts/ownership.hpp"
#include "phantomledger/entities/accounts/registry.hpp"
#include "phantomledger/entities/behavior/table.hpp"
#include "phantomledger/math/sampling.hpp"
#include "phantomledger/personas/taxonomy.hpp"
#include "phantomledger/random/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::clearing {

/// Per-persona parameters for buffer sampling.
///
/// These model what fraction of a persona's accounts receive each
/// protection type and the median dollar amount for those that do.
struct PersonaBufferProfile {
  // Initial cash balance.
  double balanceMedian = 1200.0;

  // Overdraft line.
  double overdraftFraction = 0.42;
  double overdraftMedian = 900.0;

  // Linked-account protection / sweep buffer.
  double linkedFraction = 0.32;
  double linkedMedian = 700.0;

  // Courtesy cushion.
  double courtesyFraction = 0.18;
  double courtesyMedian = 140.0;
};

/// Look up the buffer profile for a persona kind.
[[nodiscard]] constexpr PersonaBufferProfile
bufferProfile(personas::Kind kind) noexcept {
  switch (kind) {
  case personas::Kind::student:
    return {200.0, 0.20, 350.0, 0.16, 225.0, 0.12, 65.0};
  case personas::Kind::retiree:
    return {1500.0, 0.28, 600.0, 0.30, 500.0, 0.16, 100.0};
  case personas::Kind::freelancer:
    return {900.0, 0.34, 800.0, 0.26, 600.0, 0.16, 120.0};
  case personas::Kind::smallBusiness:
    return {8000.0, 0.46, 2200.0, 0.34, 2200.0, 0.20, 180.0};
  case personas::Kind::highNetWorth:
    return {25000.0, 0.55, 5000.0, 0.60, 10000.0, 0.22, 250.0};
  case personas::Kind::salaried:
    return {1200.0, 0.42, 900.0, 0.32, 700.0, 0.18, 140.0};
  }

  return bufferProfile(personas::Kind::salaried);
}

/// Configuration knobs for the balance sampling distributions.
struct BalanceRules {
  bool enableConstraints = true;
  double initialBalanceSigma = 1.00;
  double overdraftSigma = 0.60;
  double linkedSigma = 0.90;
  double courtesySigma = 0.45;
};

namespace detail {

inline constexpr double kHubCash = 1e18;

[[nodiscard]] inline double clampProbability(double p,
                                             bool enableConstraints) noexcept {
  return enableConstraints ? std::clamp(p, 0.0, 1.0) : p;
}

[[nodiscard]] inline double clampSigma(double sigma,
                                       bool enableConstraints) noexcept {
  return enableConstraints ? std::max(0.0, sigma) : sigma;
}

[[nodiscard]] inline double sampleMoney(random::Rng &rng, double median,
                                        double sigma, double floor,
                                        bool enableConstraints) {
  const double safeMedian = enableConstraints ? std::max(median, 0.01) : median;
  const double safeSigma = clampSigma(sigma, enableConstraints);
  return math::floorAndRound(
      math::lognormalByMedian(rng, safeMedian, safeSigma), floor);
}

[[nodiscard]] inline const entities::behavior::Persona &
personaFor(const entities::behavior::Table &personas,
           entities::identifier::PersonId owner) {
  if (owner == entities::identifier::invalidPerson) {
    throw std::invalid_argument("personaFor: invalid owner id");
  }

  const auto index = static_cast<std::size_t>(owner - 1);
  if (index >= personas.byPerson.size()) {
    throw std::out_of_range("personaFor: owner id exceeds personas table");
  }

  return personas.byPerson[index];
}

[[nodiscard]] inline bool
hasPrimaryAccount(const entities::accounts::Ownership &ownership,
                  entities::identifier::PersonId person) noexcept {
  if (person == entities::identifier::invalidPerson) {
    return false;
  }

  const auto personIndex = static_cast<std::size_t>(person - 1);
  if (personIndex + 1 >= ownership.byPersonOffset.size()) {
    return false;
  }

  const auto begin = ownership.byPersonOffset[personIndex];
  const auto end = ownership.byPersonOffset[personIndex + 1];
  return begin < end && begin < ownership.byPersonIndex.size();
}

} // namespace detail

/// Bootstrap a Ledger with randomized balances and protection buffers.
///
/// For each account, looks up the owner's persona and samples:
///   - initial cash balance (lognormal around persona median)
///   - overdraft limit (lognormal, assigned to a fraction of accounts)
///   - linked-account buffer (lognormal, assigned to a fraction)
///   - courtesy cushion (lognormal, assigned to a fraction)
///
/// Hub accounts get effectively infinite cash. External accounts
/// are not booked into the ledger.
///
/// After initial balances are set, each person with financial-product
/// obligations gets a 35% monthly-burden buffer added to cash to
/// prevent immediate overdraft from the first mortgage/loan payment.
inline void
bootstrap(Ledger &ledger, random::Rng &rng,
          const entities::accounts::Registry &registry,
          [[maybe_unused]] const entities::accounts::Ownership &ownership,
          const entities::behavior::Table &personas,
          const std::unordered_set<Ledger::Index> &hubIndices,
          const BalanceRules &rules = {}) {
  const auto count = static_cast<Ledger::Index>(registry.records.size());
  if (count == 0) {
    return;
  }

  if (ledger.size() < count) {
    throw std::invalid_argument(
        "bootstrap: ledger has fewer slots than registry records");
  }

  for (Ledger::Index idx = 0; idx < count; ++idx) {
    const auto &record = registry.records[idx];

    if (record.owner == entities::identifier::invalidPerson) {
      continue; // unowned / external
    }

    if (hubIndices.contains(idx)) {
      ledger.cash(idx) = detail::kHubCash;
      ledger.overdraft(idx) = 0.0;
      ledger.linked(idx) = 0.0;
      ledger.courtesy(idx) = 0.0;
      continue;
    }

    const auto &persona = detail::personaFor(personas, record.owner);
    const auto profile = bufferProfile(persona.archetype.kind);

    ledger.cash(idx) = detail::sampleMoney(rng, profile.balanceMedian,
                                           rules.initialBalanceSigma, 1.0,
                                           rules.enableConstraints);

    ledger.overdraft(idx) = 0.0;
    ledger.linked(idx) = 0.0;
    ledger.courtesy(idx) = 0.0;

    if (rng.coin(detail::clampProbability(profile.overdraftFraction,
                                          rules.enableConstraints))) {
      ledger.overdraft(idx) = detail::sampleMoney(rng, profile.overdraftMedian,
                                                  rules.overdraftSigma, 0.0,
                                                  rules.enableConstraints);
    }

    if (rng.coin(detail::clampProbability(profile.linkedFraction,
                                          rules.enableConstraints))) {
      ledger.linked(idx) =
          detail::sampleMoney(rng, profile.linkedMedian, rules.linkedSigma, 0.0,
                              rules.enableConstraints);
    }

    if (rng.coin(detail::clampProbability(profile.courtesyFraction,
                                          rules.enableConstraints))) {
      ledger.courtesy(idx) =
          detail::sampleMoney(rng, profile.courtesyMedian, rules.courtesySigma,
                              0.0, rules.enableConstraints);
    }
  }
}

/// Add a burden buffer to each person's primary account.
///
/// Adds 35% of the person's estimated monthly fixed obligation burden
/// (mortgage + auto loan + student loan + insurance + tax) to their
/// primary account's cash balance. This prevents the first month's
/// obligations from immediately overdrawing a newly bootstrapped ledger.
inline void addBurdenBuffer(Ledger &ledger,
                            const entities::accounts::Ownership &ownership,
                            const std::vector<double> &monthlyBurdens,
                            std::uint32_t people, double fraction = 0.35) {
  if (fraction <= 0.0) {
    return;
  }

  for (entities::identifier::PersonId person = 1; person <= people; ++person) {
    const auto burdenIndex = static_cast<std::size_t>(person - 1);
    if (burdenIndex >= monthlyBurdens.size()) {
      continue;
    }

    if (!detail::hasPrimaryAccount(ownership, person)) {
      continue;
    }

    const auto idx = ownership.primaryIndex(person);
    if (idx >= ledger.size()) {
      throw std::out_of_range(
          "addBurdenBuffer: primary account index exceeds ledger size");
    }

    const double burden = monthlyBurdens[burdenIndex];
    if (burden > 0.0) {
      ledger.cash(idx) = math::roundMoney(ledger.cash(idx) + fraction * burden);
    }
  }
}

} // namespace PhantomLedger::clearing
