#include "phantomledger/relationships/family/links.hpp"

#include "phantomledger/relationships/family/predicates.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace PhantomLedger::relationships::family {

namespace {

namespace pred = ::PhantomLedger::relationships::family::predicates;

void splitHouseholdRoles(
    std::span<const entity::PersonId> members,
    std::span<const ::PhantomLedger::personas::Type> personas,
    std::vector<entity::PersonId> &adults,
    std::vector<entity::PersonId> &students) {
  adults.clear();
  students.clear();

  for (entity::PersonId p : members) {
    if (pred::isStudent(personas[p - 1])) {
      students.push_back(p);
    } else {
      adults.push_back(p);
    }
  }
}

[[nodiscard]] bool householdGetsCouple(std::size_t adultCount, random::Rng &rng,
                                       double spouseP) noexcept {
  return adultCount >= 2U && rng.coin(spouseP);
}

void seatCouple(std::span<const entity::PersonId> adults, random::Rng &rng,
                std::vector<entity::PersonId> &spouseOf) {
  const auto picked = rng.choiceIndices(adults.size(), 2U, /*replace=*/false);
  const auto a = adults[picked[0]];
  const auto b = adults[picked[1]];
  spouseOf[a - 1] = b;
  spouseOf[b - 1] = a;
}

[[nodiscard]] bool studentIsDependent(random::Rng &rng,
                                      double studentDependentP) noexcept {
  return rng.coin(studentDependentP);
}

[[nodiscard]] std::span<const entity::PersonId>
parentPoolFor(std::span<const entity::PersonId> localAdults,
              std::span<const entity::PersonId> globalAdults, random::Rng &rng,
              double studentCoresidesP) noexcept {
  const bool useLocal = !localAdults.empty() && rng.coin(studentCoresidesP);
  return useLocal ? localAdults : globalAdults;
}

[[nodiscard]] std::uint32_t parentCountFor(std::size_t poolSize,
                                           random::Rng &rng,
                                           double twoParentP) noexcept {
  if (poolSize >= 2U && rng.coin(twoParentP)) {
    return 2U;
  }
  return 1U;
}

void seatParents(entity::PersonId student,
                 std::span<const entity::PersonId> pool, std::uint32_t count,
                 random::Rng &rng,
                 std::vector<std::array<entity::PersonId, 2>> &parentsOf,
                 std::vector<std::vector<entity::PersonId>> &childrenOf) {
  const auto picked = rng.choiceIndices(pool.size(), count, /*replace=*/false);

  auto &slots = parentsOf[student - 1];
  for (std::size_t i = 0; i < picked.size(); ++i) {
    const auto parent = pool[picked[i]];
    slots[i] = parent;
    childrenOf[parent - 1].push_back(student);
  }
}

void assignParentsForStudent(
    entity::PersonId student, std::span<const entity::PersonId> localAdults,
    std::span<const entity::PersonId> globalAdults,
    const Dependents &dependents, random::Rng &rng,
    std::vector<std::array<entity::PersonId, 2>> &parentsOf,
    std::vector<std::vector<entity::PersonId>> &childrenOf) {
  if (!studentIsDependent(rng, dependents.studentDependentP)) {
    return;
  }

  const auto pool = parentPoolFor(localAdults, globalAdults, rng,
                                  dependents.studentCoresidesP);
  if (pool.empty()) {
    return;
  }

  const auto count = parentCountFor(pool.size(), rng, dependents.twoParentP);
  seatParents(student, pool, count, rng, parentsOf, childrenOf);
}

[[nodiscard]] std::vector<entity::PersonId>
collectGlobalAdults(std::span<const ::PhantomLedger::personas::Type> personas,
                    std::uint32_t personCount) {
  std::vector<entity::PersonId> adults;
  adults.reserve(personCount);

  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isAdult(personas[p - 1])) {
      adults.push_back(p);
    }
  }

  if (adults.empty()) {
    adults.reserve(personCount);
    for (entity::PersonId p = 1; p <= personCount; ++p) {
      adults.push_back(p);
    }
  }

  return adults;
}

} // namespace

// =============================================================================
// Public entry points.
// =============================================================================

void assignSpouses(const LinkInputs &inputs, random::Rng &rng, Links &out) {
  if (inputs.partition == nullptr || inputs.households == nullptr ||
      inputs.dependents == nullptr || inputs.personCount == 0) {
    return;
  }

  const auto &part = *inputs.partition;
  const auto &households = *inputs.households;
  const double spouseP = households.spouseP;

  std::vector<entity::PersonId> adults;
  std::vector<entity::PersonId> studentsScratch;
  adults.reserve(static_cast<std::size_t>(households.maxSize));
  studentsScratch.reserve(static_cast<std::size_t>(households.maxSize));

  const auto hhCount = part.householdCount();
  for (std::uint32_t h = 0; h < hhCount; ++h) {
    const auto lo = part.offsets[h];
    const auto hi = part.offsets[h + 1];
    const std::span<const entity::PersonId> members{part.members.data() + lo,
                                                    hi - lo};

    splitHouseholdRoles(members, inputs.personas, adults, studentsScratch);

    if (householdGetsCouple(adults.size(), rng, spouseP)) {
      seatCouple(std::span<const entity::PersonId>{adults}, rng, out.spouseOf);
    }
  }
}

void assignParents(const LinkInputs &inputs, random::Rng &rng, Links &out) {
  if (inputs.partition == nullptr || inputs.households == nullptr ||
      inputs.dependents == nullptr || inputs.personCount == 0) {
    return;
  }

  const auto &part = *inputs.partition;
  const auto &households = *inputs.households;
  const auto &dep = *inputs.dependents;

  const auto globalAdults =
      collectGlobalAdults(inputs.personas, inputs.personCount);
  const std::span<const entity::PersonId> globalAdultsView{globalAdults};

  // Scratch buffers reused across households.
  std::vector<entity::PersonId> adults;
  std::vector<entity::PersonId> students;
  adults.reserve(static_cast<std::size_t>(households.maxSize));
  students.reserve(static_cast<std::size_t>(households.maxSize));

  const auto hhCount = part.householdCount();
  for (std::uint32_t h = 0; h < hhCount; ++h) {
    const auto lo = part.offsets[h];
    const auto hi = part.offsets[h + 1];
    const std::span<const entity::PersonId> members{part.members.data() + lo,
                                                    hi - lo};

    splitHouseholdRoles(members, inputs.personas, adults, students);

    if (students.empty()) {
      continue;
    }

    const std::span<const entity::PersonId> localAdultsView{adults};
    for (const auto student : students) {
      assignParentsForStudent(student, localAdultsView, globalAdultsView, dep,
                              rng, out.parentsOf, out.childrenOf);
    }
  }
}

Links buildLinks(const LinkInputs &inputs, random::Rng &rng) {
  Links out{};

  if (inputs.personCount == 0) {
    return out;
  }

  const std::array<entity::PersonId, 2> emptyParents{entity::invalidPerson,
                                                     entity::invalidPerson};
  out.spouseOf.assign(inputs.personCount, entity::invalidPerson);
  out.parentsOf.assign(inputs.personCount, emptyParents);
  out.childrenOf.assign(inputs.personCount, std::vector<entity::PersonId>{});

  assignSpouses(inputs, rng, out);
  assignParents(inputs, rng, out);

  return out;
}

} // namespace PhantomLedger::relationships::family
