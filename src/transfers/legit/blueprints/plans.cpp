#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/synth/personas/make.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/personas/names.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::blueprints {

namespace {

[[nodiscard]] std::size_t hubCountFor(const Macro &macro,
                                      std::size_t personCount) noexcept {
  if (personCount == 0) {
    return 0;
  }

  const auto populationCount =
      macro.population.count == 0
          ? personCount
          : static_cast<std::size_t>(macro.population.count);

  const double fraction = std::clamp(macro.hubSelection.fraction, 0.0, 0.5);

  const auto requested =
      static_cast<std::size_t>(static_cast<double>(populationCount) * fraction);

  return std::clamp(requested, std::size_t{1}, personCount);
}

[[nodiscard]] std::vector<entity::Key>
selectHubAccounts(const Timeline &timeline, const Network &network,
                  const Macro &macro,
                  const std::vector<entity::PersonId> &persons) {
  if (persons.empty() || timeline.rng == nullptr ||
      network.accounts == nullptr || network.ownership == nullptr) {
    return {};
  }

  const auto count = hubCountFor(macro, persons.size());
  if (count == 0) {
    return {};
  }

  const auto chosenIdx = timeline.rng->choiceIndices(persons.size(), count,
                                                     /*replace=*/false);

  std::vector<entity::Key> out;
  out.reserve(chosenIdx.size());

  for (const auto idx : chosenIdx) {
    const auto person = persons[idx];
    const auto recordIx = network.ownership->primaryIndex(person);

    if (recordIx < network.accounts->records.size()) {
      out.push_back(network.accounts->records[recordIx].id);
    }
  }

  return out;
}

[[nodiscard]] std::unordered_map<entity::PersonId, std::uint32_t>
primaryAcctRecordIxByPerson(const Network &network) {
  std::unordered_map<entity::PersonId, std::uint32_t> out;

  if (network.accounts == nullptr || network.ownership == nullptr) {
    return out;
  }

  const auto &ownership = *network.ownership;
  const std::size_t personCount = ownership.byPersonOffset.empty()
                                      ? 0
                                      : ownership.byPersonOffset.size() - 1;

  out.reserve(personCount);

  for (entity::PersonId person = 1;
       person <= static_cast<entity::PersonId>(personCount); ++person) {
    const auto start = ownership.byPersonOffset[person - 1];
    const auto end = ownership.byPersonOffset[person];

    if (start == end) {
      continue;
    }

    out.emplace(person, ownership.primaryIndex(person));
  }

  return out;
}

struct LandlordResolution {
  std::vector<entity::Key> ids;
  std::unordered_map<entity::Key, entity::landlord::Type> typeOf;
};

[[nodiscard]] LandlordResolution
resolveLandlords(const Network &network,
                 const std::vector<entity::Key> &hubAccounts,
                 entity::Key fallbackAcct) {
  LandlordResolution out;

  if (network.landlords != nullptr && !network.landlords->records.empty()) {
    out.ids.reserve(network.landlords->records.size());
    out.typeOf.reserve(network.landlords->records.size());

    for (const auto &record : network.landlords->records) {
      out.ids.push_back(record.accountId);
      out.typeOf.emplace(record.accountId, record.type);
    }

    return out;
  }

  if (!hubAccounts.empty()) {
    out.ids = hubAccounts;
  } else {
    out.ids.push_back(fallbackAcct);
  }

  return out;
}

[[nodiscard]] CounterpartyPlan
buildCounterpartyPlan(const Timeline &timeline, const Network &network,
                      const Macro &macro, const Overrides &overrides,
                      const std::vector<entity::PersonId> &persons) {
  if (network.accounts == nullptr || network.accounts->records.empty()) {
    throw std::invalid_argument(
        "Network.accounts must be non-empty to build a counterparty plan");
  }

  CounterpartyPlan plan;

  plan.hubAccounts = selectHubAccounts(timeline, network, macro, persons);
  plan.hubSet.reserve(plan.hubAccounts.size());
  plan.hubSet.insert(plan.hubAccounts.begin(), plan.hubAccounts.end());

  const auto fallbackAcct = !plan.hubAccounts.empty()
                                ? plan.hubAccounts.front()
                                : network.accounts->records.front().id;

  const auto *pools = overrides.counterpartyPools;

  if (pools != nullptr && !pools->employerIds.empty()) {
    plan.employers = pools->employerIds;
  } else if (!plan.hubAccounts.empty()) {
    const auto take = std::max<std::size_t>(1, plan.hubAccounts.size() / 5);
    plan.employers.assign(plan.hubAccounts.begin(),
                          plan.hubAccounts.begin() + take);
  } else {
    plan.employers.push_back(fallbackAcct);
  }

  auto landlords = resolveLandlords(network, plan.hubAccounts, fallbackAcct);
  plan.landlords = std::move(landlords.ids);
  plan.landlordTypeOf = std::move(landlords.typeOf);

  plan.billerAccounts = !plan.hubAccounts.empty()
                            ? plan.hubAccounts
                            : std::vector<entity::Key>{fallbackAcct};

  plan.issuerAcct = fallbackAcct;

  return plan;
}

[[nodiscard]] PersonaPlan
buildPersonaPlan(const Timeline &timeline, const Macro &macro,
                 const Overrides &overrides,
                 const std::vector<entity::PersonId> &persons) {
  PersonaPlan plan;

  if (overrides.personas != nullptr) {
    plan.pack = overrides.personas;
  } else {
    if (timeline.rng == nullptr) {
      throw std::invalid_argument(
          "buildLegitPlan requires either Overrides.personas or Timeline.rng");
    }

    const auto popSize = static_cast<std::uint32_t>(persons.size());
    const auto baseSeed = static_cast<std::uint64_t>(macro.population.seed);

    plan.ownedPack =
        entities::synth::personas::makePack(*timeline.rng, popSize, baseSeed);
    plan.pack = &*plan.ownedPack;
  }

  plan.personaNames.reserve(::PhantomLedger::personas::kKindCount);

  for (std::size_t i = 0; i < ::PhantomLedger::personas::kKindCount; ++i) {
    const auto kind = static_cast<::PhantomLedger::personas::Type>(i);
    plan.personaNames.push_back(::PhantomLedger::personas::name(kind));
  }

  return plan;
}

[[nodiscard]] std::vector<entity::PersonId>
extractPersons(const Network &network) {
  std::vector<entity::PersonId> out;

  if (network.accounts == nullptr || network.ownership == nullptr) {
    return out;
  }

  const auto &ownership = *network.ownership;
  if (ownership.byPersonOffset.size() <= 1) {
    return out;
  }

  const auto personCount = ownership.byPersonOffset.size() - 1;
  out.reserve(personCount);

  for (entity::PersonId person = 1;
       person <= static_cast<entity::PersonId>(personCount); ++person) {
    const auto start = ownership.byPersonOffset[person - 1];
    const auto end = ownership.byPersonOffset[person];

    if (start != end) {
      out.push_back(person);
    }
  }

  return out;
}

} // namespace

LegitBuildPlan buildLegitPlan(const Timeline &timeline, const Network &network,
                              const Macro &macro, const Overrides &overrides) {
  LegitBuildPlan plan;

  plan.startDate = timeline.window.start;
  plan.days = static_cast<std::int32_t>(timeline.window.days);
  plan.seed = static_cast<std::uint64_t>(macro.population.seed);

  plan.allAccounts = network.accounts;
  plan.persons = extractPersons(network);

  plan.counterparties =
      buildCounterpartyPlan(timeline, network, macro, overrides, plan.persons);

  plan.personas = buildPersonaPlan(timeline, macro, overrides, plan.persons);

  plan.primaryAcctRecordIx = primaryAcctRecordIxByPerson(network);

  const auto endExcl = time::addDays(timeline.window.start, plan.days);
  plan.monthStarts = time::monthStarts(timeline.window.start, endExcl);

  return plan;
}

} // namespace PhantomLedger::transfers::legit::blueprints
