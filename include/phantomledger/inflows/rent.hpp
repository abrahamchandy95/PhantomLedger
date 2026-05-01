#pragma once

#include "phantomledger/inflows/selection.hpp"
#include "phantomledger/inflows/timestamps.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/recurring/lease.hpp"
#include "phantomledger/recurring/rent.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::inflows {

namespace rent {

namespace detail {

using namespace ::PhantomLedger::taxonomies::enums;

[[nodiscard]] consteval auto buildProbabilityTable() {
  std::array<double, personas::kKindCount> table{};

  table[toIndex(personas::Type::student)] = 0.50;
  table[toIndex(personas::Type::retiree)] = 0.18;
  table[toIndex(personas::Type::freelancer)] = 0.58;
  table[toIndex(personas::Type::smallBusiness)] = 0.35;
  table[toIndex(personas::Type::highNetWorth)] = 0.10;
  table[toIndex(personas::Type::salaried)] = 0.62;

  return table;
}

inline constexpr auto kProbabilityByPersona = buildProbabilityTable();

} // namespace detail

[[nodiscard]] constexpr double probability(personas::Type type) noexcept {
  return detail::kProbabilityByPersona
      [::PhantomLedger::taxonomies::enums::toIndex(type)];
}

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
  return probability(population.persona(person));
}

[[nodiscard]] inline auto makePayerSelector(const InflowSnapshot &snapshot,
                                            const HomeownerCheck &isHomeowner) {
  return selection::makeSelector(
      [&snapshot, &isHomeowner](PersonId person) {
        return candidate(snapshot.population, person, isHomeowner);
      },
      [&snapshot](PersonId person) {
        return baseProbability(snapshot.population, person);
      });
}

template <class Selector>
[[nodiscard]] inline std::vector<RentPayer>
selectPayers(const InflowSnapshot &snapshot, random::Rng &rng, double scale,
             const Selector &selector) {
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

struct PayerState {
  PersonId person;
  Key account;
  std::string payerKey;
  recurring::Lease lease;
};

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

  const auto selector = rent::makePayerSelector(snapshot, isHomeowner);
  const double scale =
      selector.fitScale(snapshot.population.count, targetRentFraction);

  if (scale <= 0.0) {
    return {};
  }

  const auto rentPayers = rent::selectPayers(snapshot, rng, scale, selector);
  if (rentPayers.empty()) {
    return {};
  }

  const LeaseRules rules{
      .tenure = snapshot.policy().tenure,
      .inflation = snapshot.policy().inflation,
      .raises = snapshot.policy().raises,
  };

  const RentRouter router;

  std::vector<rent::PayerState> states;
  states.reserve(rentPayers.size());

  for (const auto &payer : rentPayers) {
    rent::PayerState ps;
    ps.person = payer.person;
    ps.account = payer.account;
    ps.payerKey = std::to_string(static_cast<unsigned>(payer.account.number));

    auto initRng = snapshot.entropy.factory.rng({"lease_init", ps.payerKey});

    ps.lease =
        initializeLease(rules, snapshot.entropy.factory, initRng,
                        LeaseInitInput{
                            .payerKey = ps.payerKey,
                            .startDate = snapshot.timeframe.startDate,
                            .landlords = snapshot.counterparties.landlords,
                            .rentSource = rentModel,
                        });

    states.push_back(std::move(ps));
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(states.size() * snapshot.timeframe.monthStarts.size());

  for (const auto &monthStart : snapshot.timeframe.monthStarts) {
    for (auto &ps : states) {
      while (monthStart >= ps.lease.end) {
        auto advRng = snapshot.entropy.factory.rng(
            {"lease_advance", ps.payerKey, std::to_string(ps.lease.moveIndex)});

        ps.lease =
            advanceLease(rules, snapshot.entropy.factory, advRng,
                         LeaseAdvanceInput{
                             .payerKey = ps.payerKey,
                             .now = ps.lease.end,
                             .landlords = snapshot.counterparties.landlords,
                             .previous = ps.lease,
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
                                              .payerKey = ps.payerKey,
                                              .state = ps.lease,
                                              .payDate = monthStart,
                                          });

      const auto channel = router.pick(
          rng, snapshot.counterparties.landlordType(ps.lease.landlordAcct));

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

} // namespace PhantomLedger::inflows
