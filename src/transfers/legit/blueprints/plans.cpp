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

[[nodiscard]] std::size_t hubCountFor(const HubSelectionRules &hubs,
                                      std::size_t personCount) noexcept {
  if (personCount == 0) {
    return 0;
  }

  const auto populationCount =
      hubs.populationCount == 0
          ? personCount
          : static_cast<std::size_t>(hubs.populationCount);

  const double fraction = std::clamp(hubs.fraction, 0.0, 0.5);

  const auto requested =
      static_cast<std::size_t>(static_cast<double>(populationCount) * fraction);

  return std::clamp(requested, std::size_t{1}, personCount);
}

[[nodiscard]] std::vector<entity::Key>
selectHubAccounts(random::Rng &rng, AccountCensus census,
                  const HubSelectionRules &hubs,
                  const std::vector<entity::PersonId> &persons) {
  if (persons.empty() || census.accounts == nullptr ||
      census.ownership == nullptr) {
    return {};
  }

  const auto count = hubCountFor(hubs, persons.size());
  if (count == 0) {
    return {};
  }

  const auto chosenIdx =
      rng.choiceIndices(persons.size(), count, /*replace=*/false);

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

[[nodiscard]] OwnedAccountSlices
ownedAccountSlicesByPerson(AccountCensus census) {
  OwnedAccountSlices out;

  if (census.accounts == nullptr || census.ownership == nullptr ||
      census.ownership->byPersonOffset.empty()) {
    return out;
  }

  const auto &ownership = *census.ownership;
  const auto personCount = ownership.byPersonOffset.size() - 1;

  out.offset.resize(personCount + 1);
  out.recordIx.reserve(ownership.byPersonIndex.size());

  for (std::size_t personIdx = 0; personIdx < personCount; ++personIdx) {
    out.offset[personIdx] = static_cast<std::uint32_t>(out.recordIx.size());

    const auto start = ownership.byPersonOffset[personIdx];
    const auto end = ownership.byPersonOffset[personIdx + 1];

    for (auto i = start; i < end; ++i) {
      const auto recordIx = ownership.byPersonIndex[i];
      if (recordIx < census.accounts->records.size()) {
        out.recordIx.push_back(recordIx);
      }
    }
  }

  out.offset[personCount] = static_cast<std::uint32_t>(out.recordIx.size());
  return out;
}

[[nodiscard]] std::unordered_map<entity::PersonId, std::uint32_t>
primaryAcctRecordIxByPerson(AccountCensus census) {
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
resolveLandlords(CounterpartyPools counterparties,
                 const std::vector<entity::Key> &hubAccounts,
                 entity::Key fallbackAcct) {
  LandlordResolution out;

  if (counterparties.landlords != nullptr &&
      !counterparties.landlords->records.empty()) {
    out.ids.reserve(counterparties.landlords->records.size());
    out.typeOf.reserve(counterparties.landlords->records.size());

    for (const auto &record : counterparties.landlords->records) {
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
buildCounterpartyPlan(random::Rng &rng, AccountCensus census,
                      CounterpartyPools counterparties,
                      const HubSelectionRules &hubs,
                      const std::vector<entity::PersonId> &persons) {
  const auto *accounts = census.accounts;

  if (accounts == nullptr || accounts->records.empty()) {
    throw std::invalid_argument("AccountCensus.accounts must be non-empty "
                                "to build legit counterparties");
  }

  CounterpartyPlan plan;

  plan.hubAccounts = selectHubAccounts(rng, census, hubs, persons);
  plan.hubSet.reserve(plan.hubAccounts.size());
  plan.hubSet.insert(plan.hubAccounts.begin(), plan.hubAccounts.end());

  const auto fallbackAcct = !plan.hubAccounts.empty()
                                ? plan.hubAccounts.front()
                                : accounts->records.front().id;

  const auto *directory = counterparties.directory;

  if (directory != nullptr && !directory->employers.accounts.all.empty()) {
    plan.employers = directory->employers.accounts.all;
  } else if (!plan.hubAccounts.empty()) {
    const auto take = std::max<std::size_t>(1, plan.hubAccounts.size() / 5);
    plan.employers.assign(plan.hubAccounts.begin(),
                          plan.hubAccounts.begin() + take);
  } else {
    plan.employers.push_back(fallbackAcct);
  }

  auto landlords =
      resolveLandlords(counterparties, plan.hubAccounts, fallbackAcct);
  plan.landlords = std::move(landlords.ids);
  plan.landlordTypeOf = std::move(landlords.typeOf);

  plan.billerAccounts = !plan.hubAccounts.empty()
                            ? plan.hubAccounts
                            : std::vector<entity::Key>{fallbackAcct};

  plan.issuerAcct = fallbackAcct;

  return plan;
}

[[nodiscard]] PersonaPlan
buildPersonaPlan(random::Rng &rng, LegitTimeframe timeframe,
                 PersonaCatalog personas,
                 const std::vector<entity::PersonId> &persons) {
  PersonaPlan plan;

  if (personas.pack != nullptr) {
    plan.pack = personas.pack;
  } else {
    const auto popSize = static_cast<std::uint32_t>(persons.size());
    plan.ownedPack =
        entities::synth::personas::makePack(rng, popSize, timeframe.seed);
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
extractPersons(AccountCensus census) {
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

LegitBuildPlan buildLegitPlan(random::Rng &rng, LegitTimeframe timeframe,
                              AccountCensus census,
                              CounterpartyPools counterparties,
                              PersonaCatalog personas, HubSelectionRules hubs) {
  LegitBuildPlan plan;

  plan.startDate = timeframe.window.start;
  plan.days = static_cast<std::int32_t>(timeframe.window.days);
  plan.seed = timeframe.seed;

  plan.allAccounts = census.accounts;
  plan.ownedAccountSlices = ownedAccountSlicesByPerson(census);
  plan.persons = extractPersons(census);

  plan.counterparties =
      buildCounterpartyPlan(rng, census, counterparties, hubs, plan.persons);

  plan.personas = buildPersonaPlan(rng, timeframe, personas, plan.persons);

  plan.primaryAcctRecordIx = primaryAcctRecordIxByPerson(census);

  const auto endExcl = time::addDays(timeframe.window.start, plan.days);
  plan.monthStarts = time::monthStarts(timeframe.window.start, endExcl);

  return plan;
}

} // namespace PhantomLedger::transfers::legit::blueprints
