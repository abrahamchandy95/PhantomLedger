#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/distributions/normal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/clearing/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/clearing/protection.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::clearing {

/// Per-persona sampling parameters.
struct PersonaBufferProfile {
  double balanceMedian = 0.0;

  // Protection shares must sum to <= 1.0.
  PersonaProtectionShares shares{};

  // Conditional buffer medians.
  double courtesyMedian = 0.0;
  double linkedMedian = 0.0;
  double locCreditLimitMedian = 0.0;
};

namespace detail {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

static_assert(enumTax::isIndexable(personas::kTypes));

inline constexpr std::array<PersonaBufferProfile, personas::kTypeCount>
    kBufferProfiles{{
        // personas::Type::student
        {
            .balanceMedian = 200.0,
            .shares = {.courtesy = 0.12, .linked = 0.08, .loc = 0.02},
            .courtesyMedian = 65.0,
            .linkedMedian = 225.0,
            .locCreditLimitMedian = 800.0,
        },

        // personas::Type::retiree
        {
            .balanceMedian = 1500.0,
            .shares = {.courtesy = 0.16, .linked = 0.22, .loc = 0.04},
            .courtesyMedian = 100.0,
            .linkedMedian = 500.0,
            .locCreditLimitMedian = 2500.0,
        },

        // personas::Type::freelancer
        {
            .balanceMedian = 900.0,
            .shares = {.courtesy = 0.16, .linked = 0.18, .loc = 0.12},
            .courtesyMedian = 120.0,
            .linkedMedian = 600.0,
            .locCreditLimitMedian = 4000.0,
        },

        // personas::Type::smallBusiness
        {
            .balanceMedian = 8000.0,
            .shares = {.courtesy = 0.20, .linked = 0.24, .loc = 0.20},
            .courtesyMedian = 180.0,
            .linkedMedian = 2200.0,
            .locCreditLimitMedian = 7000.0,
        },

        // personas::Type::highNetWorth
        {
            .balanceMedian = 25000.0,
            .shares = {.courtesy = 0.22, .linked = 0.30, .loc = 0.28},
            .courtesyMedian = 250.0,
            .linkedMedian = 10000.0,
            .locCreditLimitMedian = 15000.0,
        },

        // personas::Type::salaried
        {
            .balanceMedian = 1200.0,
            .shares = {.courtesy = 0.18, .linked = 0.24, .loc = 0.12},
            .courtesyMedian = 140.0,
            .linkedMedian = 700.0,
            .locCreditLimitMedian = 3000.0,
        },
    }};

static_assert(kBufferProfiles.size() == personas::kTypeCount);

} // namespace detail

/// Persona-indexed table.
[[nodiscard]] constexpr PersonaBufferProfile
bufferProfile(personas::Type type) noexcept {
  const auto index = detail::enumTax::toIndex(type);

  if (index >= detail::kBufferProfiles.size()) {
    return detail::kBufferProfiles[detail::enumTax::toIndex(
        personas::Type::salaried)];
  }

  return detail::kBufferProfiles[index];
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

static_assert(kBankTiers.size() == kBankTierCount);

inline constexpr std::array<ProtectionType, kProtectionTypeCount>
    kProtectionSamplingOrder{
        ProtectionType::courtesy,
        ProtectionType::linked,
        ProtectionType::loc,
        ProtectionType::none,
    };

static_assert(kProtectionSamplingOrder.size() == kProtectionTypeCount);

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

[[nodiscard]] inline const entity::behavior::Persona &
personaFor(const entity::behavior::Table &personas, entity::PersonId owner) {
  if (owner == entity::invalidPerson) {
    throw std::invalid_argument("personaFor: invalid owner id");
  }

  const auto index = static_cast<std::size_t>(owner - 1);
  if (index >= personas.byPerson.size()) {
    throw std::out_of_range("personaFor: owner id exceeds personas table");
  }

  return personas.byPerson[index];
}

[[nodiscard]] inline bool
hasPrimaryAccount(const entity::account::Ownership &ownership,
                  entity::PersonId person) noexcept {
  if (person == entity::invalidPerson) {
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
  const std::array<double, kBankTierCount> tierWeights = {
      weights.zeroFee,
      weights.reducedFee,
      weights.standardFee,
  };

  const auto cdf = distributions::buildCdf(tierWeights);
  const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());

  return kBankTiers[idx];
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
  const std::array<double, kProtectionSamplingOrder.size()> weights = {
      shares.courtesy,
      shares.linked,
      shares.loc,
      shares.none(),
  };

  const auto cdf = distributions::buildCdf(weights);
  const auto idx = distributions::sampleIndex(cdf, rng.nextDouble());

  return kProtectionSamplingOrder[idx];
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

class OpeningBalanceSeeder {
public:
  OpeningBalanceSeeder(Ledger &ledger, random::Rng &rng,
                       const BalanceRules &rules = {}) noexcept
      : ledger_{&ledger}, rng_{&rng}, rules_{rules} {}

  void seedHubAccount(Ledger::Index idx) const {
    ledger().cash(idx) = detail::kHubCash;
    ledger().setProtection(idx, ProtectionType::none, 0.0);
    ledger().setBankTier(idx, BankTier::zeroFee, 0.0);
  }

  void seedOwnedAccount(Ledger::Index idx,
                        const entity::behavior::Persona &persona) {
    const auto profile = bufferProfile(persona.archetype.type);

    // 1. Cash balance.
    ledger().cash(idx) = detail::sampleMoney(rng(), profile.balanceMedian,
                                             rules_.initialBalanceSigma, 1.0,
                                             rules_.enableConstraints);

    // 2-3. Bank tier and overdraft fee amount.
    const auto tier = detail::sampleTier(rng(), rules_.tierWeights);
    const double fee =
        detail::sampleOverdraftFee(rng(), tier, rules_.enableConstraints);
    ledger().setBankTier(idx, tier, fee);

    // 4-5. Protection type and buffer amount.
    const auto protection = detail::sampleProtectionType(rng(), profile.shares);
    const double buffer =
        detail::sampleBufferAmount(rng(), protection, profile, rules_);
    ledger().setProtection(idx, protection, buffer);

    // 6. LOC-specific parameters.
    if (protection == ProtectionType::loc) {
      const double apr = std::max(0.0, probability::distributions::normal(
                                           rng(), rules_.locDefaults.aprMean,
                                           rules_.locDefaults.aprSigma));

      const int billingDay = static_cast<int>(
          rng().uniformInt(rules_.locDefaults.billingDayMin,
                           rules_.locDefaults.billingDayMax + 1));

      ledger().setLoc(idx, apr, billingDay);
    }
  }

private:
  [[nodiscard]] Ledger &ledger() const noexcept { return *ledger_; }
  [[nodiscard]] random::Rng &rng() const noexcept { return *rng_; }

  Ledger *ledger_ = nullptr;
  random::Rng *rng_ = nullptr;
  BalanceRules rules_{};
};

inline void requireLedgerSlots(const Ledger &ledger,
                               const entity::account::Registry &registry) {
  const auto count = static_cast<Ledger::Index>(registry.records.size());

  if (ledger.size() < count) {
    throw std::invalid_argument(
        "bootstrap: ledger has fewer slots than registry records");
  }
}

[[nodiscard]] inline std::vector<Ledger::Index>
ownedNonHubAccountIndices(const entity::account::Registry &registry,
                          const std::unordered_set<Ledger::Index> &hubIndices) {
  std::vector<Ledger::Index> out;
  out.reserve(registry.records.size());

  const auto count = static_cast<Ledger::Index>(registry.records.size());
  for (Ledger::Index idx = 0; idx < count; ++idx) {
    const auto &record = registry.records[idx];

    if (record.owner == entity::invalidPerson) {
      continue;
    }
    if (hubIndices.contains(idx)) {
      continue;
    }

    out.push_back(idx);
  }

  return out;
}

inline void
seedHubAccounts(OpeningBalanceSeeder &seeder,
                const entity::account::Registry &registry,
                const std::unordered_set<Ledger::Index> &hubIndices) {
  const auto count = static_cast<Ledger::Index>(registry.records.size());
  for (Ledger::Index idx = 0; idx < count; ++idx) {
    const auto &record = registry.records[idx];

    if (record.owner == entity::invalidPerson) {
      continue;
    }
    if (!hubIndices.contains(idx)) {
      continue;
    }

    seeder.seedHubAccount(idx);
  }
}

inline void seedOwnedAccounts(OpeningBalanceSeeder &seeder,
                              const entity::account::Registry &registry,
                              const entity::behavior::Table &personas,
                              std::span<const Ledger::Index> accountIndices) {
  for (const auto idx : accountIndices) {
    if (idx >= registry.records.size()) {
      throw std::out_of_range(
          "seedOwnedAccounts: account index exceeds registry size");
    }

    const auto &record = registry.records[idx];
    if (record.owner == entity::invalidPerson) {
      continue;
    }

    const auto &persona = detail::personaFor(personas, record.owner);
    seeder.seedOwnedAccount(idx, persona);
  }
}

inline void addBurdenBuffer(Ledger &ledger,
                            const entity::account::Ownership &ownership,
                            const std::vector<double> &monthlyBurdens,
                            std::uint32_t people, double fraction = 0.35) {
  if (fraction <= 0.0) {
    return;
  }

  for (entity::PersonId person = 1; person <= people; ++person) {
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
