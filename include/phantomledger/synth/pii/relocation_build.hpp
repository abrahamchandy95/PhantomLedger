#pragma once

#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

/*
  Construction of the per-person home-area history. The MODEL and its
  citations live in `entities/parties/relocation.hpp`; this file is the
  sampler.

  DETERMINISM CONTRACT. Construction runs on
  `RngFactory{worldSeed}.rng({"home-relocation", <group>})` — its OWN lane,
  keyed by coresident group exactly as home placement is keyed by household.
  It spends nothing on the shared entity stream, so no downstream entity value
  moves and the blast radius stays confined to what the FOLD does with a home
  that changes. Sizing anything off the shared stream instead has been
  measured at 51,079 account-closure violations in a gate with nothing to do
  with the changed subsystem.

  THE DRAW COUNT IS FIXED PER YEAR PER GROUP — FOUR uniforms, unconditionally,
  whether or not the group moves:

    1. the move coin
    2. the day-within-year
    3. the destination position in the residential CDF
    4. the same-state/different-state coin

  ALL FOUR MUST BE DRAWN BEFORE ANY BRANCH TESTS THEM. A data-dependent draw
  count would make a group's later moves depend on whether its earlier ones
  happened to land in-state — a coupling with no mechanism behind it. The same
  trap has already cost a round via `WeightedPicker`, which retries on
  collision, so a bigger CDF spends fewer uniforms and shifted every person's
  exploration propensity.

  A "GROUP" IS (household, initial area), NOT simply a household, and the
  reason is a real asymmetry rather than a nicety. `Generator::buildRecord`
  samples `country` PER PERSON off the locale mix and only THEN calls
  `homeAreaFor(person, country)`, which draws on the household lane but inside
  that person's own country — so two members of one household who drew
  different countries already hold DIFFERENT home areas, and the production
  locale mix (`usBankDefault`, 96% US + 15 foreign weights) makes this
  reachable. Real coresidents (same household, same area) therefore move
  together, preserving the binding coresidency invariant, while a
  foreign-domiciled member of a nominal household moves on their own inside
  their own country.
 */

namespace PhantomLedger::synth::pii::relocation {

namespace reloc = ::PhantomLedger::entity::parties::relocation;
namespace geoent = ::PhantomLedger::entity::geography;

struct Inputs {
  std::uint64_t worldSeed = 0;
  // PersonId-1 indexed home area at window start — the SAME carrier
  // `People::homeAreas` holds, so tenure 0 is byte-identical to it.
  std::span<const geoent::GeoAreaId> initialAreas{};
  // PersonId-1 indexed household id, from `synth::pii::reproduceHouseholds`.
  std::span<const std::uint32_t> householdOf{};
  // PersonId-1 indexed country, for the in-country destination constraint.
  std::span<const locale::Country> countries{};
  const geoent::GeoCatalog *catalog = nullptr;
  const geo::ResidenceSampler *residence = nullptr;
  time::Window window{};
  reloc::Rules rules{};
};

namespace detail {

// Exact day count for a calendar year, from the calendar primitive rather
// than a `% 4` leap test — the corpus era is 1990-2030 so a naive test would
// happen to be right, and relying on that is how a 2100-era run breaks
// silently.
[[nodiscard]] inline int daysInYear(int year) {
  const auto begin = time::toEpochSeconds(time::makeTime({year, 1, 1}));
  const auto end = time::toEpochSeconds(time::makeTime({year + 1, 1, 1}));
  return static_cast<int>((end - begin) / 86'400);
}

} // namespace detail

/* Build the relocation schedule. One tenure per person minimum; coresidents
 * receive IDENTICAL tenure sequences. */
[[nodiscard]] inline reloc::Schedule build(const Inputs &in) {
  const auto people = in.initialAreas.size();
  if (people == 0 || in.catalog == nullptr || in.residence == nullptr ||
      in.window.days <= 0) {
    return {};
  }

  const auto windowStart = time::toEpochSeconds(in.window.start);
  const auto windowEndExcl =
      time::toEpochSeconds(time::addDays(in.window.start, in.window.days));
  const auto startYear = time::toCalendarDate(in.window.start).year;
  const auto endYear =
      time::toCalendarDate(time::addDays(in.window.start, in.window.days - 1))
          .year;

  using GroupKey = std::pair<std::uint32_t, geoent::GeoAreaId>;

  const auto householdFor = [&](std::size_t personIndex) -> std::uint32_t {
    return personIndex < in.householdOf.size()
               ? in.householdOf[personIndex]
               : static_cast<std::uint32_t>(personIndex);
  };
  const auto groupFor = [&](std::size_t personIndex) -> GroupKey {
    return {householdFor(personIndex), in.initialAreas[personIndex]};
  };

  // `std::map`, not `unordered_map`: iteration order is part of nothing here
  // (each group has its own lane) but an ordered container makes that
  // independence auditable rather than something a reader has to trust.
  std::map<GroupKey, std::size_t> representative;
  for (std::size_t i = 0; i < people; ++i) {
    representative.try_emplace(groupFor(i), i);
  }

  random::RngFactory factory{in.worldSeed};
  std::map<GroupKey, std::vector<reloc::Tenure>> byGroup;

  for (const auto &[group, repIndex] : representative) {
    const auto lane =
        std::to_string(group.first) + ":" + std::to_string(group.second);
    auto rng = factory.rng({"home-relocation", lane});

    const auto country = repIndex < in.countries.size()
                             ? in.countries[repIndex]
                             : locale::kDefaultCountry;

    auto current = in.initialAreas[repIndex];
    std::vector<reloc::Tenure> moves;

    for (int year = startYear; year <= endYear; ++year) {
      // FOUR UNIFORMS, UNCONDITIONALLY. See the header note.
      const double moveRoll = rng.nextDouble();
      const double dayRoll = rng.nextDouble();
      const double destRoll = rng.nextDouble();
      const double stateRoll = rng.nextDouble();

      const double annual =
          reloc::moverRateFor(year) * in.rules.areaChangingShare;
      if (moveRoll >= annual) {
        continue;
      }

      // Day within the calendar year, then required to fall INSIDE the
      // window. A tenure stamped outside it is one no consumer can observe,
      // and exporting it would put a `since_unix_time` before the corpus
      // starts.
      const auto yearStart = time::makeTime({year, 1, 1});
      const auto offset = static_cast<int>(
          dayRoll * static_cast<double>(detail::daysInYear(year)));
      const auto when = time::toEpochSeconds(time::addDays(yearStart, offset));
      if (when <= windowStart || when >= windowEndExcl) {
        continue;
      }

      // Destination: in-state for the CPS same-state share, country-wide
      // otherwise. Population-weighted either way, always inside the origin's
      // country (see the foreign-move limitation in
      // entities/parties/relocation.hpp), and always EXCLUDING the origin
      // area.
      //
      // The exclusion is not a tidiness measure. Sampling the whole pool and
      // dropping a self-hit as a no-op delivered HALF the CPS rate — 0.0511
      // moves/person-year against 0.103 — and pulled the realized same-state
      // share down to 0.63 from 0.818, because the pinned catalogue holds only
      // a few areas per state so in-state redraws self-hit far more often than
      // national ones. Conditional on moving, a household moves SOMEWHERE
      // ELSE.
      const auto sameState = stateRoll < in.rules.sameStateShare;
      const auto *origin =
          in.catalog->contains(current) ? &in.catalog->at(current) : nullptr;
      const std::string_view stateCode =
          (sameState && origin != nullptr) ? std::string_view{origin->stateCode}
                                           : std::string_view{};
      const auto destination =
          in.residence->sampleExcluding(destRoll, country, stateCode, current);

      // Still guarded: a country with a single residential area has no
      // destination, and that must not become a same-area tenure. The
      // Schedule invariant is that consecutive areas DIFFER, and a same-area
      // row would otherwise export as a spurious `cf_Has_Std_*` edge.
      if (!geoent::validArea(destination) || destination == current) {
        continue;
      }
      moves.push_back({when, destination});
      current = destination;
    }

    byGroup.emplace(group, std::move(moves));
  }

  // Expand to the flat per-person CSR.
  std::vector<std::uint32_t> offsets;
  std::vector<reloc::Tenure> tenures;
  offsets.reserve(people + 1);
  tenures.reserve(people + people / 2);
  offsets.push_back(0);
  for (std::size_t i = 0; i < people; ++i) {
    tenures.push_back({windowStart, in.initialAreas[i]});
    const auto it = byGroup.find(groupFor(i));
    if (it != byGroup.end()) {
      for (const auto &tenure : it->second) {
        tenures.push_back(tenure);
      }
    }
    offsets.push_back(static_cast<std::uint32_t>(tenures.size()));
  }

  return reloc::Schedule{std::move(offsets), std::move(tenures)};
}

} // namespace PhantomLedger::synth::pii::relocation
