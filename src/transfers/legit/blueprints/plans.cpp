#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/landlords.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/personas/dob.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/synth/personas/make.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/personas/names.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::blueprints {

namespace {

/*
  Entropy compatibility for the retired customer-hub selector. The former
  implementation consumed choiceIndices(population, floor(1%)) on the shared
  stream before opening-balance and transaction generation. Burning that exact
  operation contains this accounting repair to endpoint/boundary semantics;
  no selected index is retained and no account receives special treatment.
*/
void burnRetiredCounterpartySelection(random::Rng &rng,
                                      std::size_t personCount) {
  if (personCount == 0) {
    return;
  }
  const auto requested =
      static_cast<std::size_t>(static_cast<double>(personCount) * 0.01);
  const auto count = std::clamp(requested, std::size_t{1}, personCount);
  (void)rng.choiceIndices(personCount, count, /*replace=*/false);
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
  entity::counterparty::SizedKeys ids;
  std::unordered_map<entity::Key, entity::landlord::Type> typeOf;
};

[[nodiscard]] LandlordResolution
resolveLandlords(CounterpartyPools counterparties, entity::Key fallbackAcct) {
  LandlordResolution out;

  if (counterparties.landlords != nullptr &&
      !counterparties.landlords->records.empty()) {
    const auto &roster = *counterparties.landlords;
    out.ids.keys.reserve(roster.records.size());
    out.typeOf.reserve(roster.records.size());

    for (const auto &record : roster.records) {
      out.ids.keys.push_back(record.accountId);
      out.typeOf.emplace(record.accountId, record.type);
    }
    out.ids.law = roster.pool;

    return out;
  }

  out.ids = entity::counterparty::SizedKeys::single(fallbackAcct);

  return out;
}

/* Build a draw-free, event-time lookup from each observable customer area to
 * its nearest service points. The same helper is used for cash access, cash
 * deposits, and check capture so no rail quietly falls back to a global hub.
 * Distance groups nearer than the kNearbyCount cut are kept whole; the group
 * that straddles the cut is stored whole too, and NearbyIndex::select breaks
 * that tie per person. */
[[nodiscard]] ::PhantomLedger::counterparties::cash::NearbyIndex
buildNearbyPoints(std::span<const entity::Key> points,
                  std::span<const entity::geography::GeoAreaId> pointAreas,
                  CounterpartyPools counterparties, std::uint64_t domain) {
  namespace cash = ::PhantomLedger::counterparties::cash;
  cash::NearbyIndex out{domain};
  if (points.empty() || points.size() != pointAreas.size()) {
    return out;
  }

  std::unordered_set<entity::geography::GeoAreaId> customerAreas;
  customerAreas.reserve(counterparties.homeAreas.size());
  for (const auto area : counterparties.homeAreas) {
    if (entity::geography::validArea(area)) {
      customerAreas.insert(area);
    }
  }
  if (counterparties.relocation != nullptr) {
    for (const auto area : counterparties.relocation->allAreas()) {
      if (entity::geography::validArea(area)) {
        customerAreas.insert(area);
      }
    }
  }

  const auto &catalog = ::PhantomLedger::synth::geo::geography();
  for (const auto home : customerAreas) {
    if (!catalog.contains(home)) {
      continue;
    }

    std::vector<std::pair<double, std::size_t>> ranked;
    ranked.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
      if (!catalog.contains(pointAreas[i])) {
        continue;
      }
      ranked.emplace_back(entity::geography::distanceMiles(
                              catalog.at(home), catalog.at(pointAreas[i])),
                          i);
    }
    if (ranked.empty()) {
      continue;
    }

    // (distance, pool index) order, the same order the former partial sort
    // produced for its first kNearbyCount entries.
    std::ranges::sort(ranked);

    cash::NearbyTiers tiers;
    for (std::size_t first = 0; first < ranked.size();) {
      auto last = first + 1;
      while (last < ranked.size() &&
             ranked[last].first == ranked[first].first) {
        ++last;
      }
      const auto need = cash::kNearbyCount - tiers.fixed.size();
      if (last - first <= need) {
        for (auto i = first; i < last; ++i) {
          tiers.fixed.push_back(points[ranked[i].second]);
        }
      } else {
        tiers.boundary.reserve(last - first);
        for (auto i = first; i < last; ++i) {
          tiers.boundary.push_back(points[ranked[i].second]);
        }
        tiers.need = static_cast<std::uint8_t>(need);
        break;
      }
      if (tiers.fixed.size() == cash::kNearbyCount) {
        break;
      }
      first = last;
    }
    out.assign(home, std::move(tiers));
  }
  return out;
}

[[nodiscard]] CounterpartyAccess
buildCounterpartyAccess(CounterpartyPools counterparties) {
  namespace cash = ::PhantomLedger::counterparties::cash;
  CounterpartyAccess plan;

  const auto *directory = counterparties.directory;

  // Inbound funding that originates outside the modeled customer ledger must
  // use an external counterparty. Internal ownerless business accounts do not
  // get synthetic infinite liquidity.
  if (directory != nullptr && !directory->employers.accounts.external.empty()) {
    plan.employers.keys = directory->employers.accounts.external;
    plan.employers.law = directory->employers.pool;
  } else {
    plan.employers =
        entity::counterparty::SizedKeys::single(cash::fallbackEmployer());
  }

  auto landlords = resolveLandlords(counterparties, cash::fallbackLandlord());
  plan.landlords = std::move(landlords.ids);
  plan.landlordTypeOf = std::move(landlords.typeOf);

  // A roster handed over without its law would pick from a pool of the wrong
  // size, so the pair must agree.
  if (plan.employers.law.size() != plan.employers.size() ||
      plan.landlords.law.size() != plan.landlords.size()) {
    throw std::invalid_argument(
        "counterparty roster and its size law disagree on the member count");
  }

  if (directory != nullptr) {
    plan.cashWithdrawalPoints = directory->external.atmTerminals;
    plan.cashDepositPoints = directory->external.cashDepositories;
    plan.checkDepositPoints = directory->external.checkCapturePoints;
    plan.cryptoVenues = directory->external.cryptoVenues;
    plan.billerAccounts = directory->external.billers;
  }

  // Standalone blueprint callers may not provide the synthesized directory.
  // Keep those paths realistic too: small fixed external pools, never a
  // customer-account fallback.
  if (plan.cashWithdrawalPoints.empty()) {
    plan.cashWithdrawalPoints = {cash::atmTerminal(1), cash::atmTerminal(2)};
  }
  if (plan.cashDepositPoints.empty()) {
    plan.cashDepositPoints = {cash::depository(1), cash::depository(2)};
  }
  if (plan.checkDepositPoints.empty()) {
    plan.checkDepositPoints = {cash::checkCapture(1), cash::checkCapture(2)};
  }
  if (plan.cryptoVenues.empty()) {
    plan.cryptoVenues = {cash::cryptoVenue(1), cash::cryptoVenue(2),
                         cash::cryptoVenue(3), cash::cryptoVenue(4)};
  }
  if (plan.billerAccounts.empty()) {
    plan.billerAccounts.reserve(8);
    for (std::uint64_t ordinal = 1; ordinal <= 8; ++ordinal) {
      plan.billerAccounts.push_back(cash::biller(ordinal));
    }
  }

  plan.homeAreas = counterparties.homeAreas;
  plan.relocation = counterparties.relocation;

  // Precompute small geographic choice sets for all physical service rails.
  // Emission does no distance scan and remains deterministic at event time.
  if (directory != nullptr) {
    plan.nearbyWithdrawalPoints = buildNearbyPoints(
        plan.cashWithdrawalPoints, directory->external.atmTerminalAreas,
        counterparties, cash::kWithdrawalSetDomain);
    plan.nearbyCashDepositPoints = buildNearbyPoints(
        plan.cashDepositPoints, directory->external.cashDepositoryAreas,
        counterparties, cash::kDepositSetDomain);
    plan.nearbyCheckDepositPoints = buildNearbyPoints(
        plan.checkDepositPoints, directory->external.checkCaptureAreas,
        counterparties, cash::kCheckSetDomain);
  }

  return plan;
}

[[nodiscard]] PersonaAccess
buildPersonaAccess(random::Rng &rng, LegitTimeframe timeframe,
                   PersonaCatalog personas,
                   const std::vector<entity::PersonId> &persons) {
  PersonaAccess plan;

  if (personas.pack != nullptr) {
    plan.pack = personas.pack;
  } else {
    const auto popSize = static_cast<std::uint32_t>(persons.size());
    plan.ownedPack = synth::personas::makePack(rng, popSize, timeframe.seed);
    // H2 steps 2a/2b + H3 3c-ii: standalone blueprint packs carry the
    // same join-cohort + single-age-axis + timeline carriers
    // production fills at the entities stage — the {"join-cohort"/
    // "dob"/"persona-era", personId} lanes off the run seed, anchored
    // at the window start for seeds and at the JOIN date for the
    // cohort (authority U-7/U-8 + addendum).
    plan.ownedPack->joinDays = synth::personas::join_cohort::deriveJoinDays(
        timeframe.seed, popSize, timeframe.window);
    plan.ownedPack->birthDates = synth::personas::birthDates(
        timeframe.seed, timeframe.window.start, plan.ownedPack->assignment,
        plan.ownedPack->joinDays);
    plan.ownedPack->timelines = synth::personas::timeline::deriveAll(
        timeframe.seed, timeframe.window.start, plan.ownedPack->assignment,
        plan.ownedPack->birthDates, plan.ownedPack->joinDays);
    plan.pack = &*plan.ownedPack;
  }

  plan.names.reserve(::PhantomLedger::personas::kKindCount);

  for (std::size_t i = 0; i < ::PhantomLedger::personas::kKindCount; ++i) {
    const auto kind = static_cast<::PhantomLedger::personas::Type>(i);
    plan.names.push_back(::PhantomLedger::personas::name(kind));
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

LegitBlueprint &
LegitBlueprint::addCounterparties(random::Rng &rng,
                                  CounterpartyPools counterparties) {
  burnRetiredCounterpartySelection(rng, accounts_.persons.size());
  counterparties_ = buildCounterpartyAccess(counterparties);
  return *this;
}

LegitBlueprint &LegitBlueprint::addPersonas(random::Rng &rng,
                                            LegitTimeframe timeframe,
                                            PersonaCatalog personas) {
  personas_ = buildPersonaAccess(rng, timeframe, personas, accounts_.persons);
  return *this;
}

LegitBlueprint buildLegitBlueprint(LegitTimeframe timeframe,
                                   AccountCensus census) {
  LegitBlueprint plan;

  plan.calendar_.startDate = timeframe.window.start;
  plan.calendar_.days = static_cast<std::int32_t>(timeframe.window.days);
  plan.seed_ = timeframe.seed;

  plan.accounts_.registry = census.accounts;
  plan.accounts_.ownedSlices = ownedAccountSlicesByPerson(census);
  plan.accounts_.persons = extractPersons(census);
  plan.accounts_.primaryRecordIx = primaryAcctRecordIxByPerson(census);

  const auto endExcl =
      time::addDays(timeframe.window.start, plan.calendar_.days);
  plan.calendar_.monthStarts =
      time::monthStarts(timeframe.window.start, endExcl);

  return plan;
}

} // namespace PhantomLedger::transfers::legit::blueprints
