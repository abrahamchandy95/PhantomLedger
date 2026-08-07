#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market::population {

/* Sparse set of payday day-indices for one person within the run window. */
struct PaydaySet {
  std::span<const std::uint32_t> days;

  [[nodiscard]] bool contains(std::uint32_t dayIndex) const noexcept;
};

/* "No retirement consumption step in this window" sentinel for
 * Census::retirementDays / View::retirementDay. */
inline constexpr std::uint32_t kNoRetirementDay =
    std::numeric_limits<std::uint32_t>::max();

/* "Does not die in this window" sentinel for Census::deathDays /
 * View::deathDay. */
inline constexpr std::uint32_t kNoDeathDay =
    std::numeric_limits<std::uint32_t>::max();

struct Census {
  std::uint32_t count = 0;

  // Per-person, indexed by PersonId-1.
  std::span<const entity::Key> primaryAccounts;

  /* EVERY deposit account the person owns, as a flat array bounded by
   * `depositOffsets` (count + 1 entries, so person i occupies
   * `[depositOffsets[i], depositOffsets[i+1])`). Slot 0 of each row is the
   * PRIMARY, which is what lets the bill / external / P2P channels keep
   * sourcing from one fixed account.
   *
   * Only the primary was ever reachable before, so a person could present at
   * most ONE debit identity in the card view no matter how many accounts the
   * world had seeded for them. The card-view exporter already mints a
   * `D`-prefixed identity per deposit ACCOUNT, so nothing downstream had to
   * change to make the rest of them visible.
   *
   * EMPTY is legal and means "primary only" — every direct harness that
   * builds a Census without an ownership pack gets exactly the pre-round
   * single-instrument behaviour. */
  std::span<const std::uint32_t> depositOffsets;
  std::span<const entity::Key> depositAccounts;

  std::span<const personas::Type> personaTypes;
  std::span<const entity::behavior::Persona> personaObjects;

  // Payday rosters: one PaydaySet per person.
  std::span<const PaydaySet> paydays;

  /* Per-person home area (PersonId-1), from the compact People::homeAreas
   * carrier. Drives distance-decay merchant selection. EMPTY on the monolith
   * reference oracle (addSpending has no People in scope); the production
   * windowed path and the test world supply it. */
  std::span<const entity::geography::GeoAreaId> homeAreas;

  /* The home-area HISTORY behind the snapshot above. NON-OWNING and nullable
   * — null means "home never moves", which is what the monolith reference
   * oracle and every direct unit harness get.
   *
   * THE VIEW NEEDS THE SCHEDULE, NOT JUST THE SNAPSHOT, for two reasons the
   * snapshot cannot serve: the merchant geo-pool builder must cover the UNION
   * of every area anyone ever occupies (a mover arriving where there is no
   * pool would silently fall back to the national CDF, switching distance
   * decay off for them), and the monthly evolver refreshes the snapshot from
   * it at each month boundary. */
  const entity::parties::relocation::Schedule *relocation = nullptr;

  /* The window day-index from which the retirement consumption step applies
   * (kNoRetirementDay = never in this window). Computed by the transfers
   * layer from the persona-timeline carrier, which BOTH engines share — so
   * unlike homeAreas this is never empty on the oracle. Seed retirees and
   * highNetWorth carry the sentinel: a seed retiree's archetype already
   * encodes retired-calibrated spending (rate x0.6 / amount x0.9), so the
   * step models only the IN-WINDOW transition. */
  std::span<const std::uint32_t> retirementDays;

  /* The window day-index of the person's DEATH (kNoDeathDay = the person
   * survives the window). Same provenance as retirementDays — the blueprint
   * pack's timeline lane carries tl.death, both engines identical. The
   * emission loop stops a spender's person-days here; no exemptions. */
  std::span<const std::uint32_t> deathDays;
};

} // namespace PhantomLedger::activity::spending::market::population
