#pragma once
/*
 * Balance book initialization.
 *
 * Bootstraps the Ledger with per-persona randomized balances,
 * protection type (NONE / COURTESY / LINKED / LOC), bank tier
 * (ZERO_FEE / REDUCED_FEE / STANDARD_FEE), overdraft fee amount, and
 * LOC parameters (APR, billing day).
 *
 * The previous revision sampled overdraft / linked / courtesy buffers
 * with three independent coin flips, which could produce an account
 * that held all three simultaneously. The Python reference treats
 * protection types as mutually exclusive; this revision enforces that
 * invariant via a single 4-way categorical draw over
 * {courtesy, linked, loc, none}.
 *
 * Bank tier is an independent draw from the 15/10/75 tier weights.
 * The overdraft fee amount is drawn once per account from the
 * tier-specific lognormal, stored on the ledger, and used later when
 * transferAt() fires an OVERDRAFT_FEE liquidity event.
 */

#include "phantomledger/entities/accounts/ownership.hpp"
#include "phantomledger/entities/accounts/registry.hpp"
#include "phantomledger/entities/behavior/table.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/clearing/protection.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::clearing {

/// Per-persona sampling parameters. Replaces the previous per-buffer
/// fraction set. Each of courtesy / linked / loc is the unconditional
/// probability an account receives that protection type. The remainder
/// (1 - sum) is NONE. Medians are sampled from lognormal around them.
struct PersonaBufferProfile {
  double balanceMedian;

  // Protection shares (sum must be <= 1.0).
  PersonaProtectionShares shares;

  // Conditional buffer medians.
  double courtesyMedian;
  double linkedMedian;
  double locCreditLimitMedian;
};

/// Persona-indexed table. Values approximately preserve the character
/// of the previous balance_book (low-end students, mid salaried,
/// high-end HNW) while respecting mutual exclusivity.
[[nodiscard]] constexpr PersonaBufferProfile
bufferProfile(personas::Type type) noexcept {
  switch (type) {
  case personas::Type::student:
    return {200.0, {0.12, 0.08, 0.02}, 65.0, 225.0, 800.0};
  case personas::Type::retiree:
    return {1500.0, {0.16, 0.22, 0.04}, 100.0, 500.0, 2500.0};
  case personas::Type::freelancer:
    return {900.0, {0.16, 0.18, 0.12}, 120.0, 600.0, 4000.0};
  case personas::Type::smallBusiness:
    return {8000.0, {0.20, 0.24, 0.20}, 180.0, 2200.0, 7000.0};
  case personas::Type::highNetWorth:
    return {25000.0, {0.22, 0.30, 0.28}, 250.0, 10000.0, 15000.0};
  case personas::Type::salaried:
    return {1200.0, {0.18, 0.24, 0.12}, 140.0, 700.0, 3000.0};
  }

  return bufferProfile(personas::Type::salaried);
}

/// Configuration knobs for the balance sampling distributions.
struct BalanceRules {
  bool enableConstraints = true;
  double initialBalanceSigma = 1.00;
  double courtesySigma = 0.45;
  double linkedSigma = 0.90;
  double locCreditLimitSigma = 0.60;

  TierWeights tierWeights{};
  LocDefaults locDefaults{};
};

namespace detail {

inline constexpr double kHubCash = 1e18;

[[nodiscard]] inline double clampSigma(double sigma,
                                       bool enableConstraints) noexcept {
  return enableConstraints ? std::max(0.0, sigma) : sigma;
}

[[nodiscard]] inline double sampleMoney(random::Rng &rng, double median,
                                        double sigma, double floor,
                                        bool enableConstraints) {
  const double safeMedian = enableConstraints ? std::max(median, 0.01) : median;
  const double safeSigma = clampSigma(sigma, enableConstraints);
  return primitives::utils::floorAndRound(
      probability::distributions::lognormalByMedian(rng, safeMedian, safeSigma),
      floor);
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

[[nodiscard]] inline BankTier sampleTier(random::Rng &rng,
                                         const TierWeights &weights) {
  const std::array<double, 3> w = {
      weights.zeroFee,
      weights.reducedFee,
      weights.standardFee,
  };
  const auto cdf = distributions::buildCdf(w);
  const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
  return static_cast<BankTier>(idx);
}

[[nodiscard]] inline double sampleOverdraftFee(random::Rng &rng, BankTier tier,
                                               bool enableConstraints) {
  const auto profile = tierFeeProfile(tier);
  if (profile.median <= 0.0) {
    return 0.0;
  }
  return sampleMoney(rng, profile.median, profile.sigma, 0.0,
                     enableConstraints);
}

[[nodiscard]] inline ProtectionType
sampleProtectionType(random::Rng &rng, const PersonaProtectionShares &shares) {
  // Four-way categorical over courtesy / linked / loc / none.
  const std::array<double, 4> w = {
      shares.courtesy,
      shares.linked,
      shares.loc,
      shares.none(),
  };
  const auto cdf = distributions::buildCdf(w);
  const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());

  switch (idx) {
  case 0:
    return ProtectionType::courtesy;
  case 1:
    return ProtectionType::linked;
  case 2:
    return ProtectionType::loc;
  default:
    return ProtectionType::none;
  }
}

[[nodiscard]] inline double sampleBufferAmount(random::Rng &rng,
                                               ProtectionType type,
                                               const PersonaBufferProfile &prof,
                                               const BalanceRules &rules) {
  switch (type) {
  case ProtectionType::none:
    return 0.0;
  case ProtectionType::courtesy:
    return sampleMoney(rng, prof.courtesyMedian, rules.courtesySigma, 0.0,
                       rules.enableConstraints);
  case ProtectionType::linked:
    return sampleMoney(rng, prof.linkedMedian, rules.linkedSigma, 0.0,
                       rules.enableConstraints);
  case ProtectionType::loc:
    return sampleMoney(rng, prof.locCreditLimitMedian,
                       rules.locCreditLimitSigma, 0.0, rules.enableConstraints);
  }
  return 0.0;
}

} // namespace detail

/// Bootstrap a Ledger with randomized balances, protection, tier, and
/// LOC parameters.
///
/// For each internal account with an owner:
///   1. Sample cash balance (lognormal around persona median).
///   2. Sample bank tier from the 15/10/75 weights.
///   3. Sample overdraft fee amount from the tier's lognormal.
///   4. Sample protection type from the persona's 4-way categorical.
///   5. Sample buffer amount conditional on protection type.
///   6. For LOC accounts, sample APR and billing day.
///
/// Hub accounts get effectively infinite cash, NONE protection, and
/// ZERO_FEE tier. External accounts are skipped entirely.
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
      ledger.setProtection(idx, ProtectionType::none, 0.0);
      ledger.setBankTier(idx, BankTier::zeroFee, 0.0);
      continue;
    }

    const auto &persona = detail::personaFor(personas, record.owner);
    const auto profile = bufferProfile(persona.archetype.type);

    // 1. Cash balance.
    ledger.cash(idx) = detail::sampleMoney(rng, profile.balanceMedian,
                                           rules.initialBalanceSigma, 1.0,
                                           rules.enableConstraints);

    // 2-3. Bank tier and overdraft fee amount.
    const auto tier = detail::sampleTier(rng, rules.tierWeights);
    const double fee =
        detail::sampleOverdraftFee(rng, tier, rules.enableConstraints);
    ledger.setBankTier(idx, tier, fee);

    // 4-5. Protection type (mutually exclusive) and buffer amount.
    const auto protection = detail::sampleProtectionType(rng, profile.shares);
    const double buffer =
        detail::sampleBufferAmount(rng, protection, profile, rules);
    ledger.setProtection(idx, protection, buffer);

    // 6. LOC-specific parameters.
    if (protection == ProtectionType::loc) {
      const double apr = std::max(
          0.0, probability::distributions::normal(
                   rng, rules.locDefaults.aprMean, rules.locDefaults.aprSigma));
      const int billingDay =
          static_cast<int>(rng.uniformInt(rules.locDefaults.billingDayMin,
                                          rules.locDefaults.billingDayMax + 1));
      ledger.setLoc(idx, apr, billingDay);
    }
  }
}

/// Add a burden buffer to each person's primary account.
///
/// Adds `fraction` of the person's estimated monthly fixed obligation
/// burden (mortgage + auto loan + student loan + insurance + tax) to
/// their primary account's cash balance. This prevents the first
/// month's obligations from immediately overdrawing a newly
/// bootstrapped ledger.
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
      ledger.cash(idx) =
          primitives::utils::roundMoney(ledger.cash(idx) + fraction * burden);
    }
  }
}

} // namespace PhantomLedger::clearing
