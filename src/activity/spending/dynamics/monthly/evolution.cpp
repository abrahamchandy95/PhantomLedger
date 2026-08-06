#include "phantomledger/activity/spending/dynamics/monthly/evolution.hpp"

#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include <algorithm>
#include <vector>

namespace PhantomLedger::activity::spending::dynamics::monthly {

namespace {

/* Rebuild the national popularity CDF over the merchants LIVE at `ts`.
 *
 * A dead merchant keeps its catalogue slot and its historical transactions —
 * closing a shop does not erase last year's receipts — but its weight goes to
 * zero so nothing new can route to it. A merchant not yet born is likewise
 * weightless until its open date.
 *
 * MUST STAY DRAW-FREE: pure arithmetic over world state at a fixed month
 * boundary, so the batch and windowed engines compute the same CDF at the
 * same instant and stay in lockstep. */
void rebuildLiveMerchCdf(market::commerce::View &commerce, std::int64_t ts) {
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr || catalog->records.empty()) {
    return;
  }

  std::vector<double> weights;
  weights.reserve(catalog->records.size());
  for (const auto &record : catalog->records) {
    weights.push_back(record.liveAt(ts) ? record.weight : 0.0);
  }

  auto cdf = market::commerce::cdfOrEmpty(weights);
  if (cdf.empty()) {
    /* Every merchant dead at this instant. Unreachable under the shipped
     * hazards (the replacement cohort guarantees births), but leaving the
     * previous CDF in place is the safe degradation: selection continues
     * against last month's live set rather than sampling an all-zero
     * distribution. */
    return;
  }
  commerce.merchCdf() = std::move(cdf);
}

/* Rebuild the REACH model — the law favourite MEMBERSHIP is drawn from — over
 * the merchants live at `ts`.
 *
 * IT MUST BE REBUILT MONTHLY, for the same reason the volume CDF is: the
 * solved flattening exponent depends on which merchants are live, and a
 * catalogue that has churned 25% of its members carries a different weight
 * vector than the one the bootstrap solved against. Leaving the window-start
 * model in place lets realized reach drift silently away from its target over
 * a long run — the failure mode the target exists to prevent.
 *
 * Draw-free and stateless (a fixed-iteration bisection over world state), so
 * the batch and windowed engines compute the identical model at the identical
 * instant. MUST RUN BEFORE THE FAVOURITES PASS, because that pass's add
 * samples this CDF. */
void rebuildLiveReach(market::commerce::View &commerce, std::int64_t ts,
                      double meanSetSize) {
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr || catalog->records.empty()) {
    return;
  }

  std::vector<double> weights;
  weights.reserve(catalog->records.size());
  for (const auto &record : catalog->records) {
    weights.push_back(record.liveAt(ts) ? record.weight : 0.0);
  }

  auto model = market::commerce::buildReachModel(weights, meanSetSize);
  auto cdf = market::commerce::cdfOrEmpty(model.membership);
  if (cdf.empty()) {
    return; // every merchant dead: keep last month's law, as above
  }
  commerce.reachMutable() = std::move(model);
  commerce.reachCdf() = std::move(cdf);

  /* And the home-conditioned MEMBERSHIP sampler over the same law. Refreshed
   * rather than rebuilt from scratch so the per-area index
   * (placement-derived, liveness-independent) is reused: a mask-and-prefix-sum
   * over O(M + A*k), which is what the factorisation in local_pools.hpp buys.
   *
   * THE CNP SHARE MUST READ THE MONTH'S OWN YEAR so a 20-year run walks the
   * dated series instead of freezing at the window-start value. */
  commerce.membershipMutable().rebuildLive(
      *catalog, ts,
      market::commerce::cnpShareForYear(
          time::toCalendarDate(time::fromEpochSeconds(ts)).year));
}

/* Rebuild the biller CDF over the BILLER-CATEGORY merchants live at `ts`.
 * Kept separate from the national rebuild because the biller population is a
 * category subset, and because `emitBill` samples this CDF directly on the
 * "no known biller" branch. */
void rebuildLiveBillerCdf(market::commerce::View &commerce, std::int64_t ts) {
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr || catalog->records.empty()) {
    return;
  }

  std::vector<double> weights(catalog->records.size(), 0.0);
  bool any = false;
  for (std::size_t i = 0; i < catalog->records.size(); ++i) {
    const auto &rec = catalog->records[i];
    if (market::isBillerCategory(rec.category) && rec.liveAt(ts)) {
      weights[i] = rec.weight;
      any = true;
    }
  }
  if (!any) {
    return; // keep last month's CDF rather than sampling an empty one
  }

  auto cdf = market::commerce::cdfOrEmpty(weights);
  if (!cdf.empty()) {
    commerce.billerCdf() = std::move(cdf);
  }
}

/* Replace a person's CLOSED billers, ONE FOR ONE.
 *
 * A FAVOURITE THAT CLOSES IS DROPPED; A BILLER THAT CLOSES IS REPLACED.
 * Nobody is obliged to shop anywhere, but a utility or subscription that
 * closes still leaves the customer needing power, phone or insurance, so the
 * relationship MOVES to another provider rather than disappearing. Dropping
 * without replacing quietly thins every long-run household's recurring-debit
 * count — a realism regression dressed as a bug fix. */
void churnBillers(random::Rng &rng, market::commerce::View &commerce,
                  std::uint32_t personIdx, std::int64_t ts) {
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr) {
    return;
  }
  auto &billers = commerce.billersMutable();
  const auto &cdf = commerce.billerCdf();
  if (cdf.empty()) {
    return;
  }

  auto row = billers.rowOf(personIdx);
  for (std::size_t slot = row.size(); slot-- > 0;) {
    const auto idx = row[slot];
    const bool dead =
        idx >= catalog->records.size() || !catalog->records[idx].liveAt(ts);
    if (!dead) {
      continue;
    }

    /* Draw a live successor, avoiding one this person already bills with.
     * Bounded attempts: a duplicate is harmless (the row is a preference set,
     * not a ledger), so the loop degrades to "no replacement" rather than
     * spinning. */
    std::uint32_t successor = idx;
    for (int attempt = 0; attempt < 8; ++attempt) {
      const auto candidate = static_cast<std::uint32_t>(
          probability::distributions::sampleIndex(cdf, rng.nextDouble()));
      const auto current = billers.rowOf(personIdx);
      const bool held =
          std::find(current.begin(), current.end(), candidate) != current.end();
      if (!held && candidate < catalog->records.size() &&
          catalog->records[candidate].liveAt(ts)) {
        successor = candidate;
        break;
      }
    }

    if (successor != idx) {
      billers.rowOfMutable(personIdx)[slot] = successor;
    } else {
      (void)billers.swapRemove(personIdx, slot);
    }
    row = billers.rowOf(personIdx);
  }
}

} // namespace

void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce, std::uint32_t totalPersons,
               std::int64_t ts,
               std::span<const entity::geography::GeoAreaId> homeAreas) {
  /* MERCHANT LIVENESS FIRST, and the order is load-bearing: the favourites
   * pass below draws replacements from these laws, so it must see this
   * month's live set. Adding a favourite that closed last month would create
   * a relationship that can never transact.
   *
   * The reach solve takes `maxFavorites`, not the seeded mean — see the
   * `meanSetSize` note in `market/bootstrap.cpp`. */
  rebuildLiveMerchCdf(commerce, ts);
  rebuildLiveBillerCdf(commerce, ts);
  rebuildLiveReach(commerce, ts, static_cast<double>(cfg.maxFavorites));
  commerce.geoPoolsMutable().rebuildLive(*commerce.catalog(), ts);

  auto &contacts = commerce.contactsMutable();

  for (std::uint32_t personIdx = 0; personIdx < totalPersons; ++personIdx) {
    const auto row = contacts.rowOfMutable(personIdx);
    math::evolution::evolveContacts(rng, cfg,
                                    math::evolution::ContactRow{
                                        .row = row,
                                        .personIdx = personIdx,
                                        .nPeople = totalPersons,
                                    });
  }

  // ------------------------------------- THE FAVOURITES PASS, NOW WIRED
  //
  // This closes the `TODO(structural)` that stood here. The TODO was
  // accurate about the obstacle — `Csr` had fixed-length rows and
  // `evolveFavorites` wants to push and erase — and badly understated the
  // consequence: `evolveFavorites` was DEAD CODE, so **every person's
  // favourite merchant set was frozen for the entire run** while
  // `merchantAddP` / `merchantDropP` / `maxFavorites` sat in the config
  // being validated at startup. `Csr` now carries per-row capacity plus a
  // live count, so both operations are O(1).
  //
  // TWO PASSES, IN THIS ORDER, and the order is the point:
  //
  //   1. FORCED DROPS. A favourite whose merchant has closed must go,
  //      regardless of the drop coin. This is what makes merchant churn
  //      reach the ~98% exploit path — the monthly CDF rebuild only governs
  //      exploration, so without this a closed merchant keeps receiving
  //      transactions from everyone who already favoured it. Measured at
  //      49% of merchant rows out of tenure before this pass existed.
  //
  //   2. VOLUNTARY add/drop, the modelled preference drift.
  //
  // A forced drop is NOT a draw — it is a consequence of world state — so
  // it is applied before any coin is spent and cannot shift the draw
  // sequence for a person whose favourites all survived.
  const auto *catalog = commerce.catalog();
  auto &favorites = commerce.favoritesMutable();
  /* THE REACH CDF, NEVER THE POPULARITY CDF. Reading `commerce.merchCdf()`
   * — the volume law — makes the monthly add pass a concentration ratchet
   * pointed at the highest-volume merchants: popularity-weighted add at
   * 0.35/mo against a UNIFORM drop at 0.10/mo, measured taking top-1 card
   * penetration from 0.494 to 0.853 over 240 months. Correcting the bootstrap
   * draw alone leaves this to rebuild the hub over any long run. */
  const auto &membership = commerce.membership();
  const auto &fallbackCdf =
      commerce.reachCdf().empty() ? commerce.merchCdf() : commerce.reachCdf();

  for (std::uint32_t personIdx = 0; personIdx < totalPersons; ++personIdx) {
    churnBillers(rng, commerce, personIdx, ts);

    /* Home-conditioned per person. `homeArea` comes from the population View,
     * which the relocation refresh re-points BEFORE this pass runs, so a
     * mover acquires favourites near their NEW home. */
    const auto home = personIdx < homeAreas.size()
                          ? homeAreas[personIdx]
                          : entity::geography::invalidGeoArea;
    const math::evolution::MerchantPool pool{
        .sample = membership.ready()
                      ? std::function<std::uint32_t(double)>(
                            [&membership, home](double u) {
                              return membership.sample(home, u);
                            })
                      : std::function<std::uint32_t(double)>(
                            [&fallbackCdf](double u) {
                              return static_cast<std::uint32_t>(
                                  probability::distributions::sampleIndex(
                                      fallbackCdf, u));
                            }),
        .totalCount = static_cast<std::uint32_t>(fallbackCdf.size()),
    };

    if (catalog != nullptr) {
      /* Walk backwards: swapRemove moves the last live entry into the hole,
       * so a forward walk would skip the entry it just moved down. */
      auto row = favorites.rowOf(personIdx);
      for (std::size_t slot = row.size(); slot-- > 0;) {
        const auto idx = row[slot];
        if (idx >= catalog->records.size() ||
            !catalog->records[idx].liveAt(ts)) {
          (void)favorites.swapRemove(personIdx, slot);
          row = favorites.rowOf(personIdx);
        }
      }
    }

    /* evolveFavorites operates on an owning vector, so the live row is copied
     * out and written back. The copy is bounded by capacity (48 uint32 = 192
     * bytes) and runs once per person per MONTH, not per transaction. */
    const auto live = favorites.rowOf(personIdx);
    std::vector<std::uint32_t> working(live.begin(), live.end());
    const auto before = working.size();

    math::evolution::evolveFavorites(rng, cfg, working, pool);

    if (working.size() != before ||
        !std::equal(working.begin(), working.end(), live.begin(),
                    live.begin() + static_cast<std::ptrdiff_t>(before))) {
      /* Rewrite the row from the evolved set. The capacity truncation is
       * unreachable while `favoriteCapacity >= maxFavorites`; it guards
       * against a future config raising maxFavorites alone. */
      const auto capacity = favorites.rowCapacity(personIdx);
      while (favorites.rowOf(personIdx).size() > 0) {
        (void)favorites.swapRemove(personIdx, 0);
      }
      for (std::size_t i = 0; i < working.size() && i < capacity; ++i) {
        (void)favorites.pushBack(personIdx, working[i]);
      }
    }
  }
}

} // namespace PhantomLedger::activity::spending::dynamics::monthly
