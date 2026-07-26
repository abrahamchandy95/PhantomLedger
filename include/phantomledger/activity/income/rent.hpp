#pragma once

#include "phantomledger/activity/income/selection.hpp"
#include "phantomledger/activity/income/timestamps.hpp"
#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/activity/recurring/lease.hpp"
#include "phantomledger/activity/recurring/rent.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::income {

namespace rent {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

struct Rules {
  recurring::LeaseRules lease{};
  // Fit target for the population renter share. Each PL renter is a
  // SOLE TENANT paying one full household rent (L-3 C0 read), so the
  // comparator is the ACS household renter share ~.34-.36 — the old
  // .80 made four in five people full-rent payers, ~2.3x that share
  // (household-econ-2026-07; docs/fraud_model_audit.md L-3/L-5).
  double paidFraction = 0.35;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    lease.validate(r);
    r.check([&] { v::unit("rentPaidFraction", paidFraction); });
  }
};

using HomeownerCheck = std::function<bool(PersonId)>;

inline bool noHomeowners(PersonId) { return false; }

struct RentRoll {
  Timeframe timeframe;
  Entropy entropy;
  Population population;
  RentCounterparties counterparties;
  Rules rules{};
  HomeownerCheck isHomeowner = noHomeowners;

  void validate(primitives::validate::Report &r) const {
    timeframe.validate(r);
    rules.validate(r);
  }
};

namespace internal {

struct PersonaProbability {
  personas::Type persona;
  double probability = 0.0;
};

// Relative renting propensities, scaled by the paidFraction fit
// (scale ~= .66 at the .35 target, so effective shares run student
// ~.33, retiree ~.12, freelancer ~.38, smallBusiness ~.23, HNW ~.07,
// salaried ~.41 — renting skews young/mobile).
inline constexpr auto kPersonaProbabilities =
    std::to_array<PersonaProbability>({
        {personas::Type::student, 0.50},
        {personas::Type::retiree, 0.18},
        {personas::Type::freelancer, 0.58},
        {personas::Type::smallBusiness, 0.35},
        {personas::Type::highNetWorth, 0.10},
        {personas::Type::salaried, 0.62},
    });

struct ProbabilityBuild {
  std::array<double, personas::kKindCount> values{};
  bool valid = true;
};

[[nodiscard]] consteval bool unit(double value) {
  return value >= 0.0 && value <= 1.0;
}

[[nodiscard]] consteval ProbabilityBuild buildProbabilityTable() {
  ProbabilityBuild build{};
  std::array<bool, personas::kKindCount> seen{};

  if (kPersonaProbabilities.size() != personas::kKindCount) {
    build.valid = false;
    return build;
  }

  for (const auto entry : kPersonaProbabilities) {
    const auto index = enumTax::toIndex(entry.persona);

    if (index >= personas::kKindCount) {
      build.valid = false;
      return build;
    }

    if (seen[index]) {
      build.valid = false;
      return build;
    }

    if (!unit(entry.probability)) {
      build.valid = false;
      return build;
    }

    build.values[index] = entry.probability;
    seen[index] = true;
  }

  for (const bool found : seen) {
    if (!found) {
      build.valid = false;
      return build;
    }
  }

  return build;
}

inline constexpr auto kProbabilityBuild = buildProbabilityTable();

static_assert(kProbabilityBuild.valid,
              "rent probabilities must contain exactly one valid entry for "
              "each persona");

inline constexpr auto kProbabilityByPersona = kProbabilityBuild.values;

[[nodiscard]] constexpr double probabilityFor(personas::Type type) noexcept {
  return kProbabilityByPersona[enumTax::toIndex(type)];
}

} // namespace internal

[[nodiscard]] constexpr double probability(personas::Type type) noexcept {
  return internal::probabilityFor(type);
}

struct RentPayer {
  PersonId person;
  Key account;
};

[[nodiscard]] inline bool candidate(const Population &population,
                                    PersonId person,
                                    const HomeownerCheck &isHomeowner) {
  return population.exists(person) && population.hasAccount(person) &&
         !population.isHub(person) && !isHomeowner(person);
}

[[nodiscard]] inline double baseProbability(const Population &population,
                                            PersonId person) {
  return probability(population.persona(person));
}

[[nodiscard]] inline auto makePayerSelector(const RentRoll &rentRoll,
                                            const HomeownerCheck &isHomeowner) {
  return selection::makeSelector(
      [&rentRoll, &isHomeowner](PersonId person) {
        return candidate(rentRoll.population, person, isHomeowner);
      },
      [&rentRoll](PersonId person) {
        return baseProbability(rentRoll.population, person);
      });
}

template <class Selector>
[[nodiscard]] inline std::vector<RentPayer>
selectPayers(const RentRoll &rentRoll, random::Rng &rng, double scale,
             const Selector &selector) {
  std::vector<RentPayer> out;
  out.reserve(rentRoll.population.count());

  for (PersonId person = 1; person <= rentRoll.population.count(); ++person) {
    if (!selector.selected(rng, person, scale)) {
      continue;
    }

    out.push_back(RentPayer{
        .person = person,
        .account = rentRoll.population.primary(person),
    });
  }

  return out;
}

struct PayerState {
  PersonId person;
  Key account;
  std::string payerKey;
  recurring::Lease lease;
};

} // namespace rent

[[nodiscard]] inline std::vector<transactions::Transaction>
generateRentTxns(const rent::RentRoll &rentRoll, random::Rng &rng,
                 const transactions::Factory &txf,
                 const std::function<double()> &rentModel) {
  namespace recur = ::PhantomLedger::activity::recurring;
  namespace tlx = ::PhantomLedger::synth::personas::timeline;

  if (rentRoll.counterparties.landlords.empty()) {
    return {};
  }

  primitives::validate::require(rentRoll);

  const auto selector = rent::makePayerSelector(rentRoll, rentRoll.isHomeowner);
  const double scale = selector.fitScale(rentRoll.population.count(),
                                         rentRoll.rules.paidFraction);

  if (scale <= 0.0) {
    return {};
  }

  const auto rentPayers = rent::selectPayers(rentRoll, rng, scale, selector);
  if (rentPayers.empty()) {
    return {};
  }

  const auto &rules = rentRoll.rules.lease;
  const recur::RentRouter router;

  std::vector<rent::PayerState> states;
  states.reserve(rentPayers.size());

  for (const auto &payer : rentPayers) {
    rent::PayerState ps;
    ps.person = payer.person;
    ps.account = payer.account;
    ps.payerKey = std::to_string(static_cast<unsigned>(payer.account.number));

    auto initRng = rentRoll.entropy.factory.rng({"lease_init", ps.payerKey});

    ps.lease = recur::initializeLease(
        rules, rentRoll.entropy.factory, initRng,
        recur::LeaseInitInput{
            .payerKey = ps.payerKey,
            .startDate = rentRoll.timeframe.startDate,
            .landlords = rentRoll.counterparties.landlords,
            .rentSource = rentModel,
        });

    states.push_back(std::move(ps));
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(states.size() * rentRoll.timeframe.monthStarts.size());

  for (const auto &monthStart : rentRoll.timeframe.monthStarts) {
    for (auto &ps : states) {
      // H3: the lease dies with the tenant — no rent month posts at or
      // after the death (the estate's lease wind-down is a declared
      // simplification away). The skip removes this payer's shared-rng
      // jitter/router draws for the month — a declared model-moving
      // shift, absorbed by this round's re-pin.
      if (!tlx::aliveAt(rentRoll.population.timeline(ps.person), monthStart)) {
        continue;
      }

      while (monthStart >= ps.lease.end) {
        auto advRng = rentRoll.entropy.factory.rng(
            {"lease_advance", ps.payerKey, std::to_string(ps.lease.moveIndex)});

        ps.lease = recur::advanceLease(
            rules, rentRoll.entropy.factory, advRng,
            recur::LeaseAdvanceInput{
                .payerKey = ps.payerKey,
                .now = ps.lease.end,
                .landlords = rentRoll.counterparties.landlords,
                .previous = ps.lease,
                .resetRentSource = rentModel,
            });
      }

      const auto txnTs = timestamps::jittered(
          monthStart, 0, timestamps::kRentTimestampJitter, rng);

      if (!rentRoll.timeframe.contains(txnTs)) {
        continue;
      }

      const double amount =
          recur::calculateRent(rules, rentRoll.entropy.factory,
                               recur::RentQuery{
                                   .payerKey = ps.payerKey,
                                   .state = ps.lease,
                                   .payDate = monthStart,
                               });

      const auto channel = router.pick(
          rng, rentRoll.counterparties.landlordType(ps.lease.landlordAcct));

      txns.push_back(txf.make(transactions::Draft{
          .source = ps.account,
          .destination = ps.lease.landlordAcct,
          .amount = amount,
          .timestamp = time::toEpochSeconds(txnTs),
          .isFraud = 0,
          .ringId = -1,
          .channel = channel,
      }));
    }
  }

  sortTransfers(txns);

  return txns;
}

} // namespace PhantomLedger::activity::income
