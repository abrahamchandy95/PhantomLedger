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

[[nodiscard]] std::size_t hubCountFor(const PopulationTuning &population,
                                      std::size_t personCount) noexcept {
  if (personCount == 0) {
    return 0;
  }

  const auto populationCount = population.count == 0
                                   ? personCount
                                   : static_cast<std::size_t>(population.count);

  const double fraction = std::clamp(population.hubFraction, 0.0, 0.5);

  const auto requested =
      static_cast<std::size_t>(static_cast<double>(populationCount) * fraction);

  return std::clamp(requested, std::size_t{1}, personCount);
}

[[nodiscard]] std::vector<entity::Key>
selectHubAccounts(const PlanRequest &request,
                  const std::vector<entity::PersonId> &persons) {
  const auto &census = request.census;

  if (persons.empty() || request.rng == nullptr || census.accounts == nullptr ||
      census.ownership == nullptr) {
    return {};
  }

  const auto count = hubCountFor(request.population, persons.size());
  if (count == 0) {
    return {};
  }

  const auto chosenIdx =
      request.rng->choiceIndices(persons.size(), count, /*replace=*/false);

  std::vector<entity::Key> out;
  out.reserve(chosenIdx.size());

  for (const auto idx : chosenIdx) {
    const auto person = persons[idx];
    const auto recordIx = census.ownership->primaryIndex(person);

    if (recordIx < census.accounts->records.size()) {
      out.push_back(census.accounts->records[recordIx].id);
    }
  }

  return out;
}

[[nodiscard]] std::unordered_map<entity::PersonId, std::uint32_t>
primaryAcctRecordIxByPerson(const CensusSource &census) {
  std::unordered_map<entity::PersonId, std::uint32_t> out;

  if (census.accounts == nullptr || census.ownership == nullptr) {
    return out;
  }

  const auto &ownership = *census.ownership;
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
resolveLandlords(const CounterpartySource &source,
                 const std::vector<entity::Key> &hubAccounts,
                 entity::Key fallbackAcct) {
  LandlordResolution out;

  if (source.landlords != nullptr && !source.landlords->records.empty()) {
    out.ids.reserve(source.landlords->records.size());
    out.typeOf.reserve(source.landlords->records.size());

    for (const auto &record : source.landlords->records) {
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
buildCounterpartyPlan(const PlanRequest &request,
                      const std::vector<entity::PersonId> &persons) {
  const auto *accounts = request.census.accounts;

  if (accounts == nullptr || accounts->records.empty()) {
    throw std::invalid_argument("PlanRequest.census.accounts must be non-empty "
                                "to build counterparties");
  }

  CounterpartyPlan plan;

  plan.hubAccounts = selectHubAccounts(request, persons);
  plan.hubSet.reserve(plan.hubAccounts.size());
  plan.hubSet.insert(plan.hubAccounts.begin(), plan.hubAccounts.end());

  const auto fallbackAcct = !plan.hubAccounts.empty()
                                ? plan.hubAccounts.front()
                                : accounts->records.front().id;

  const auto *counterparties = request.counterparties.directory;

  if (counterparties != nullptr &&
      !counterparties->employers.accounts.all.empty()) {
    plan.employers = counterparties->employers.accounts.all;
  } else if (!plan.hubAccounts.empty()) {
    const auto take = std::max<std::size_t>(1, plan.hubAccounts.size() / 5);
    plan.employers.assign(plan.hubAccounts.begin(),
                          plan.hubAccounts.begin() + take);
  } else {
    plan.employers.push_back(fallbackAcct);
  }

  auto landlords =
      resolveLandlords(request.counterparties, plan.hubAccounts, fallbackAcct);
  plan.landlords = std::move(landlords.ids);
  plan.landlordTypeOf = std::move(landlords.typeOf);

  plan.billerAccounts = !plan.hubAccounts.empty()
                            ? plan.hubAccounts
                            : std::vector<entity::Key>{fallbackAcct};

  plan.issuerAcct = fallbackAcct;

  return plan;
}

[[nodiscard]] PersonaPlan
buildPersonaPlan(const PlanRequest &request,
                 const std::vector<entity::PersonId> &persons) {
  PersonaPlan plan;

  if (request.personas.pack != nullptr) {
    plan.pack = request.personas.pack;
  } else {
    if (request.rng == nullptr) {
      throw std::invalid_argument("buildLegitPlan requires either "
                                  "PersonaSource.pack or PlanRequest.rng");
    }

    const auto popSize = static_cast<std::uint32_t>(persons.size());
    const auto baseSeed = static_cast<std::uint64_t>(request.population.seed);

    plan.ownedPack =
        entities::synth::personas::makePack(*request.rng, popSize, baseSeed);
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
extractPersons(const CensusSource &census) {
  std::vector<entity::PersonId> out;

  if (census.accounts == nullptr || census.ownership == nullptr) {
    return out;
  }

  const auto &ownership = *census.ownership;
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

LegitBuildPlan buildLegitPlan(const PlanRequest &request) {
  LegitBuildPlan plan;

  plan.startDate = request.window.start;
  plan.days = static_cast<std::int32_t>(request.window.days);
  plan.seed = static_cast<std::uint64_t>(request.population.seed);

  plan.allAccounts = request.census.accounts;
  plan.persons = extractPersons(request.census);

  plan.counterparties = buildCounterpartyPlan(request, plan.persons);

  plan.personas = buildPersonaPlan(request, plan.persons);

  plan.primaryAcctRecordIx = primaryAcctRecordIxByPerson(request.census);

  const auto endExcl = time::addDays(request.window.start, plan.days);
  plan.monthStarts = time::monthStarts(request.window.start, endExcl);

  return plan;
}

} // namespace PhantomLedger::transfers::legit::blueprints
