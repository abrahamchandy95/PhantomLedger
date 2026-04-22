#pragma once
/*
 * inflows/rent.hpp — rent transaction generator.
 *
 * Iterates the lease state machine month-by-month across the
 * simulation window. Each rent event is routed through
 * RentRouter::pick() so individual landlords produce
 * Zelle/check/ACH mixes while corporate property managers get
 * near-exclusive portal ACH.
 */

#include "phantomledger/inflows/selection.hpp"
#include "phantomledger/inflows/timestamps.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/recurring/lease.hpp"
#include "phantomledger/recurring/rent.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::inflows {

namespace rent {

struct ProbabilityTable {
  static constexpr std::array<double, personas::kKindCount> table{{
      0.50, // student
      0.18, // retiree
      0.58, // freelancer
      0.35, // smallBusiness
      0.10, // highNetWorth
      0.62, // salaried
  }};

  [[nodiscard]] static constexpr double forKind(personas::Kind kind) noexcept {
    return table[personas::indexOf(kind)];
  }
};

using HomeownerCheck = std::function<bool(PersonId)>;

inline bool noHomeowners(PersonId) { return false; }

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
  return ProbabilityTable::forKind(population.persona(person));
}

[[nodiscard]] inline std::vector<RentPayer>
selectPayers(const InflowSnapshot &snapshot, random::Rng &rng, double scale,
             const HomeownerCheck &isHomeowner) {
  const auto selector = selection::makeSelector(
      [&](PersonId person) {
        return rent::candidate(snapshot.population, person, isHomeowner);
      },
      [&](PersonId person) {
        return rent::baseProbability(snapshot.population, person);
      });

  std::vector<RentPayer> out;
  out.reserve(snapshot.population.count);

  for (PersonId person = 1; person <= snapshot.population.count; ++person) {
    if (!selector.selected(rng, person, scale)) {
      continue;
    }

    out.push_back(RentPayer{
        .person = person,
        .account = snapshot.population.primary(person),
    });
  }

  return out;
}

} // namespace rent

[[nodiscard]] inline std::vector<transactions::Transaction>
generateRentTxns(const InflowSnapshot &snapshot, random::Rng &rng,
                 const transactions::Factory &txf, double targetRentFraction,
                 const std::function<double()> &rentModel,
                 const rent::HomeownerCheck &isHomeowner = rent::noHomeowners) {
  using namespace recurring;

  if (!snapshot.hasRecurringPolicy() ||
      snapshot.counterparties.landlords.empty()) {
    return {};
  }

  const auto selector = selection::makeSelector(
      [&](PersonId person) {
        return rent::candidate(snapshot.population, person, isHomeowner);
      },
      [&](PersonId person) {
        return rent::baseProbability(snapshot.population, person);
      });

  const double scale =
      selector.fitScale(snapshot.population.count, targetRentFraction);
  if (scale <= 0.0) {
    return {};
  }

  const auto rentPayers = rent::selectPayers(snapshot, rng, scale, isHomeowner);
  if (rentPayers.empty()) {
    return {};
  }

  const LeaseRules rules{
      .tenure = snapshot.policy().tenure,
      .inflation = snapshot.policy().inflation,
      .raises = snapshot.policy().raises,
  };

  const RentRouter router;

  std::unordered_map<Key, Lease, std::hash<Key>> leases;
  leases.reserve(rentPayers.size());

  for (const auto &payer : rentPayers) {
    const auto payerKey =
        std::to_string(static_cast<unsigned>(payer.account.number));

    auto initRng = snapshot.entropy.factory.rng({"lease_init", payerKey});

    leases[payer.account] =
        initializeLease(rules, snapshot.entropy.factory, initRng,
                        LeaseInitInput{
                            .payerKey = payerKey,
                            .startDate = snapshot.timeframe.startDate,
                            .landlords = snapshot.counterparties.landlords,
                            .rentSource = rentModel,
                        });
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(rentPayers.size() * snapshot.timeframe.monthStarts.size());

  for (const auto &monthStart : snapshot.timeframe.monthStarts) {
    for (const auto &payer : rentPayers) {
      auto &state = leases[payer.account];

      const auto payerKey =
          std::to_string(static_cast<unsigned>(payer.account.number));

      while (monthStart >= state.end) {
        auto advRng = snapshot.entropy.factory.rng(
            {"lease_advance", payerKey, std::to_string(state.moveIndex)});

        state = advanceLease(rules, snapshot.entropy.factory, advRng,
                             LeaseAdvanceInput{
                                 .payerKey = payerKey,
                                 .now = state.end,
                                 .landlords = snapshot.counterparties.landlords,
                                 .previous = state,
                                 .resetRentSource = rentModel,
                             });
      }

      const auto txnTs = timestamps::jittered(
          monthStart, 0, timestamps::kRentTimestampJitter, rng);

      if (!snapshot.timeframe.contains(txnTs)) {
        continue;
      }

      const double amount = calculateRent(rules, snapshot.entropy.factory,
                                          RentQuery{
                                              .payerKey = payerKey,
                                              .state = state,
                                              .payDate = monthStart,
                                          });

      const auto channel = router.pick(
          rng, snapshot.counterparties.landlordClass(state.landlordAcct));

      txns.push_back(txf.make(transactions::Draft{
          .source = payer.account,
          .destination = state.landlordAcct,
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

} // namespace PhantomLedger::inflows
