#include "phantomledger/relationships/family/support.hpp"

#include "phantomledger/relationships/family/predicates.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

namespace pred = ::PhantomLedger::relationships::family::predicates;

namespace {

using Persona = ::PhantomLedger::personas::Type;

[[nodiscard]] bool inPopulation(entity::PersonId person,
                                std::uint32_t personCount) noexcept {
  return entity::valid(person) && person <= personCount;
}

[[nodiscard]] std::size_t personIx(entity::PersonId person) noexcept {
  return static_cast<std::size_t>(person - 1U);
}

[[nodiscard]] Persona personaFor(std::span<const Persona> personas,
                                 entity::PersonId person) noexcept {
  if (!entity::valid(person)) {
    return Persona::salaried;
  }

  const auto ix = personIx(person);
  if (ix >= personas.size()) {
    return Persona::salaried;
  }

  return personas[ix];
}

[[nodiscard]] bool isRetired(std::span<const Persona> personas,
                             entity::PersonId person) noexcept {
  return entity::valid(person) && pred::isRetired(personaFor(personas, person));
}

[[nodiscard]] bool canSupport(std::span<const Persona> personas,
                              entity::PersonId person) noexcept {
  return entity::valid(person) &&
         pred::canSupport(personaFor(personas, person));
}

[[nodiscard]] std::uint32_t sampleSupporterCount(random::Rng &rng) noexcept {
  const double draw = rng.nextDouble();
  if (draw < 0.65) {
    return 1U;
  }
  if (draw < 0.92) {
    return 2U;
  }
  return 3U;
}

[[nodiscard]] entity::PersonId spouseFor(const Links *links,
                                         entity::PersonId person,
                                         std::uint32_t personCount) noexcept {
  if (links == nullptr || !inPopulation(person, personCount)) {
    return entity::invalidPerson;
  }

  const auto ix = personIx(person);
  if (ix >= links->spouseOf.size()) {
    return entity::invalidPerson;
  }

  const auto spouse = links->spouseOf[ix];
  return inPopulation(spouse, personCount) ? spouse : entity::invalidPerson;
}

[[nodiscard]] std::span<const entity::PersonId>
householdFor(const Partition *partition, entity::PersonId person,
             std::uint32_t personCount) noexcept {
  if (partition == nullptr || !inPopulation(person, personCount)) {
    return {};
  }

  const auto personIndex = personIx(person);
  if (personIndex >= partition->householdOf.size()) {
    return {};
  }

  const auto household =
      static_cast<std::size_t>(partition->householdOf[personIndex]);
  if (household + 1U >= partition->offsets.size()) {
    return {};
  }

  const auto lo = partition->offsets[household];
  const auto hi = partition->offsets[household + 1U];
  if (lo > hi || hi > partition->members.size()) {
    return {};
  }

  return {partition->members.data() + lo, hi - lo};
}

void collectPeople(std::span<const Persona> personas, std::uint32_t personCount,
                   std::vector<entity::PersonId> &supporters,
                   std::vector<entity::PersonId> &retirees) {
  supporters.clear();
  retirees.clear();
  supporters.reserve(personCount);
  retirees.reserve(personCount);

  for (entity::PersonId person = 1; person <= personCount; ++person) {
    const auto persona = personaFor(personas, person);
    if (pred::canSupport(persona)) {
      supporters.push_back(person);
    }
    if (pred::isRetired(persona)) {
      retirees.push_back(person);
    }
  }
}

void collectResidentSupporters(std::span<const entity::PersonId> household,
                               std::span<const Persona> personas,
                               entity::PersonId retiredParent,
                               std::uint32_t personCount,
                               std::vector<entity::PersonId> &out) {
  out.clear();

  for (const auto person : household) {
    if (person != retiredParent && inPopulation(person, personCount) &&
        canSupport(personas, person)) {
      out.push_back(person);
    }
  }
}

void collectEligible(std::span<const entity::PersonId> pool,
                     entity::PersonId retiredParent,
                     std::vector<entity::PersonId> &out) {
  out.clear();
  out.reserve(pool.size());

  for (const auto person : pool) {
    if (person != retiredParent) {
      out.push_back(person);
    }
  }
}

[[nodiscard]] std::vector<entity::PersonId>
chooseSupporters(std::span<const entity::PersonId> eligible, random::Rng &rng) {
  const auto count = std::min<std::size_t>(
      eligible.size(), static_cast<std::size_t>(sampleSupporterCount(rng)));
  if (count == 0U) {
    return {};
  }

  const auto indices = rng.choiceIndices(eligible.size(), count,
                                         /*replace=*/false);

  std::vector<entity::PersonId> chosen;
  chosen.reserve(indices.size());
  for (const auto ix : indices) {
    chosen.push_back(eligible[ix]);
  }
  return chosen;
}

void addSupportedParent(SupportTies &ties, entity::PersonId adultChild,
                        entity::PersonId retiredParent,
                        std::uint32_t personCount) {
  if (!inPopulation(adultChild, personCount) ||
      !inPopulation(retiredParent, personCount)) {
    return;
  }

  ties.supportedParentsBy[personIx(adultChild)].push_back(retiredParent);
}

[[nodiscard]] bool maybeCopySpousalSupport(SupportTies &ties,
                                           const Links *links,
                                           std::span<const Persona> personas,
                                           entity::PersonId retiredParent,
                                           std::uint32_t personCount,
                                           random::Rng &rng) {
  const auto spouse = spouseFor(links, retiredParent, personCount);
  if (!entity::valid(spouse) || !isRetired(personas, spouse)) {
    return false;
  }

  const auto &sharedSupporters = ties.supportingChildrenOf[personIx(spouse)];
  if (sharedSupporters.empty() || !rng.coin(0.85)) {
    return false;
  }

  ties.supportingChildrenOf[personIx(retiredParent)] = sharedSupporters;
  for (const auto adultChild : sharedSupporters) {
    addSupportedParent(ties, adultChild, retiredParent, personCount);
  }
  return true;
}

} // namespace

SupportTies buildSupportTies(const SupportInputs &inputs, random::Rng &rng) {
  SupportTies out{};
  out.supportingChildrenOf.assign(inputs.personCount,
                                  std::vector<entity::PersonId>{});
  out.supportedParentsBy.assign(inputs.personCount,
                                std::vector<entity::PersonId>{});

  if (inputs.personCount == 0 || inputs.support == nullptr ||
      !inputs.support->enabled) {
    return out;
  }

  std::vector<entity::PersonId> supporters;
  std::vector<entity::PersonId> retirees;
  collectPeople(inputs.personas, inputs.personCount, supporters, retirees);

  std::vector<entity::PersonId> residentSupporters;
  std::vector<entity::PersonId> eligible;

  for (const auto retiredParent : retirees) {
    if (maybeCopySpousalSupport(out, inputs.links, inputs.personas,
                                retiredParent, inputs.personCount, rng)) {
      continue;
    }

    if (!rng.coin(inputs.support->hasChildP)) {
      continue;
    }

    const auto household =
        householdFor(inputs.partition, retiredParent, inputs.personCount);
    collectResidentSupporters(household, inputs.personas, retiredParent,
                              inputs.personCount, residentSupporters);

    const bool useResidents =
        !residentSupporters.empty() && rng.coin(inputs.support->coresidesP);
    const std::span<const entity::PersonId> pool =
        useResidents ? std::span<const entity::PersonId>{residentSupporters}
                     : std::span<const entity::PersonId>{supporters};

    collectEligible(pool, retiredParent, eligible);
    if (eligible.empty()) {
      continue;
    }

    auto chosen =
        chooseSupporters(std::span<const entity::PersonId>{eligible}, rng);
    if (chosen.empty()) {
      continue;
    }

    for (const auto adultChild : chosen) {
      addSupportedParent(out, adultChild, retiredParent, inputs.personCount);
    }
    out.supportingChildrenOf[personIx(retiredParent)] = std::move(chosen);
  }

  return out;
}

} // namespace PhantomLedger::relationships::family
