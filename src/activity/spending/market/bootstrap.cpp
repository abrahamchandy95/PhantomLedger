#include "phantomledger/activity/spending/market/bootstrap.hpp"

#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/population/paydays.hpp"
#include "phantomledger/primitives/random/distributions/beta.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/relationships/social/builder.hpp"
#include "phantomledger/synth/geo/catalog.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::spending::market {

namespace {

namespace social = ::PhantomLedger::relationships::social;
namespace synthgeo = ::PhantomLedger::synth::geo;

population::View buildPopulationView(const population::Census &census) {
  std::vector<entity::Key> primary(census.primaryAccounts.begin(),
                                   census.primaryAccounts.end());
  std::vector<personas::Type> kinds(census.personaTypes.begin(),
                                    census.personaTypes.end());
  std::vector<entity::behavior::Persona> objects(census.personaObjects.begin(),
                                                 census.personaObjects.end());

  /* Per-person home area. Empty on the monolith oracle, in which case
   * View::homeArea() reports invalidGeoArea (no local anchor); the windowed
   * and test paths carry the real areas. */
  std::vector<entity::geography::GeoAreaId> homeAreas(census.homeAreas.begin(),
                                                      census.homeAreas.end());

  /* Per-person retirement and death day-indices, both from the blueprint's
   * persona-timeline lane — both engines supply them identically. */
  std::vector<std::uint32_t> retirementDays(census.retirementDays.begin(),
                                            census.retirementDays.end());
  std::vector<std::uint32_t> deathDays(census.deathDays.begin(),
                                       census.deathDays.end());

  std::vector<std::uint32_t> offsets;
  offsets.reserve(census.count + 1);

  std::vector<std::uint32_t> flat;
  flat.reserve(static_cast<std::size_t>(census.count) * 12);

  offsets.push_back(0);
  for (std::uint32_t i = 0; i < census.count; ++i) {
    if (i < census.paydays.size()) {
      const auto &set = census.paydays[i];
      flat.insert(flat.end(), set.days.begin(), set.days.end());
    }

    offsets.push_back(static_cast<std::uint32_t>(flat.size()));
  }

  population::Paydays paydays(std::move(offsets), std::move(flat));

  /* Every deposit account per person, primary first. Empty on any harness
   * that supplies no ownership carrier, in which case `View::deposits`
   * reports the primary alone. */
  std::vector<std::uint32_t> depositOffsets(census.depositOffsets.begin(),
                                            census.depositOffsets.end());
  std::vector<entity::Key> depositAccounts(census.depositAccounts.begin(),
                                           census.depositAccounts.end());

  return population::View(
      std::move(primary), std::move(kinds), std::move(objects),
      std::move(paydays), std::move(homeAreas), std::move(retirementDays),
      std::move(deathDays), census.relocation, std::move(depositOffsets),
      std::move(depositAccounts));
}

std::vector<double> buildMerchCdf(const entity::merchant::Catalog &catalog) {
  std::vector<double> weights;
  weights.reserve(catalog.records.size());

  for (const auto &rec : catalog.records) {
    weights.push_back(rec.weight);
  }

  return probability::distributions::buildCdf(weights);
}

std::vector<double> buildBillerCdf(const entity::merchant::Catalog &catalog,
                                   const std::vector<double> &fallback) {
  std::vector<double> weights(catalog.records.size(), 0.0);

  bool any = false;
  for (std::size_t i = 0; i < catalog.records.size(); ++i) {
    if (isBillerCategory(catalog.records[i].category)) {
      weights[i] = catalog.records[i].weight;
      any = true;
    }
  }

  if (!any) {
    return fallback;
  }

  return probability::distributions::buildCdf(weights);
}

class WeightedPicker {
public:
  explicit WeightedPicker(std::uint16_t maxTries) noexcept
      : maxTries_(maxTries) {}

  /* The same retry-on-duplicate loop as `pick`, over an arbitrary one-uniform
   * sampler, so home-conditioned membership costs exactly the draws the CDF
   * form does. `pick` is retained for the fallback and for the biller draw,
   * which is not geography-conditioned. */
  template <typename Sampler>
  void pickFrom(random::Rng &rng, std::uint16_t k,
                std::vector<std::uint32_t> &out, Sampler &&sample) const {
    out.reserve(static_cast<std::size_t>(k));
    std::uint16_t tries = 0;
    while (out.size() < k && tries < maxTries_) {
      const auto idx = sample(rng.nextDouble());
      ++tries;
      if (std::find(out.begin(), out.end(), idx) == out.end()) {
        out.push_back(idx);
      }
    }
    if (out.empty()) {
      out.push_back(sample(rng.nextDouble()));
    }
  }

  void pick(random::Rng &rng, const std::vector<double> &cdf, std::uint16_t k,
            std::vector<std::uint32_t> &out) const {
    out.reserve(static_cast<std::size_t>(k));

    std::uint16_t tries = 0;
    while (out.size() < k && tries < maxTries_) {
      const auto idx = static_cast<std::uint32_t>(
          probability::distributions::sampleIndex(cdf, rng.nextDouble()));

      ++tries;

      if (std::find(out.begin(), out.end(), idx) == out.end()) {
        out.push_back(idx);
      }
    }

    if (out.empty() && !cdf.empty()) {
      out.push_back(static_cast<std::uint32_t>(
          probability::distributions::sampleIndex(cdf, rng.nextDouble())));
    }
  }

private:
  std::uint16_t maxTries_;
};

/* LANE SEPARATION. `WeightedPicker::pick` retries on duplicate draws, so ITS
 * DRAW COUNT IS DATA-DEPENDENT: a larger merchant CDF collides less often,
 * finishes in fewer tries, and spends fewer uniforms. Anything drawn AFTER it
 * on the same lane therefore shifts whenever the catalogue changes size.
 * `exploreProp`, `burstStart` and `burstLen` once shared the `{"payees", id}`
 * lane, so growing the merchant pool moved every person's exploration
 * propensity and burst window — a measured +4,138 rows (189,035 -> 193,173)
 * of movement that looked like a merchant change and was not.
 *
 * ANY DRAW WHOSE COUNT DEPENDS ON DATA MUST BE LAST ON ITS OWN LANE, never
 * upstream of something else. */
[[nodiscard]] random::Rng makeLaneRng(random::RngFactory &factory,
                                      std::string_view lane,
                                      std::uint32_t personIndex) {
  std::array<char, 16> idBuf{};
  const auto id = personIndex + 1u;
  auto [ptr, ec] = std::to_chars(idBuf.data(), idBuf.data() + idBuf.size(),
                                 static_cast<unsigned>(id));
  (void)ec;
  const std::string_view idStr(idBuf.data(),
                               static_cast<std::size_t>(ptr - idBuf.data()));
  return factory.rng({lane, idStr});
}

[[nodiscard]] random::Rng makePerPersonRng(random::RngFactory &factory,
                                           std::uint32_t personIndex) {
  std::array<char, 16> idBuf{};

  const auto id = personIndex + 1u;
  auto [ptr, ec] = std::to_chars(idBuf.data(), idBuf.data() + idBuf.size(),
                                 static_cast<unsigned>(id));
  (void)ec;

  const std::string_view idStr(idBuf.data(),
                               static_cast<std::size_t>(ptr - idBuf.data()));

  return factory.rng({"payees", idStr});
}

[[nodiscard]] Cards normalizeCards(Cards cards, std::uint32_t personCount) {
  const auto expectedSize = static_cast<std::size_t>(personCount);

  if (cards.size() == 0) {
    return Cards(expectedSize);
  }

  if (cards.size() != expectedSize) {
    throw std::invalid_argument(
        "buildMarket requires cards.size() to match census.count");
  }

  return cards;
}

[[nodiscard]] social::Contacts buildSocialContacts(std::uint32_t personCount,
                                                   std::uint64_t baseSeed) {
  return social::build(social::kDefaultSocial, social::BuildInputs{
                                                   .personCount = personCount,
                                                   .hubFlags = {},
                                                   .baseSeed = baseSeed,
                                               });
}

} // namespace

Market buildMarket(MarketSources sources, PayeeSelectionRules payees,
                   ShopperBehaviorRules behavior) {
  population::View popView = buildPopulationView(sources.census);

  random::RngFactory factory(sources.baseSeed);

  const auto *catalog = sources.network.catalog;

  std::vector<double> merchCdf =
      catalog != nullptr ? buildMerchCdf(*catalog) : std::vector<double>{};

  std::vector<double> billerCdf = catalog != nullptr
                                      ? buildBillerCdf(*catalog, merchCdf)
                                      : std::vector<double>{};

  /* The live-at-window-start weighting the initial favourite draw uses. Empty
   * when there is no catalogue or no lifecycle, in which case the draw falls
   * back to the national CDF. */
  std::vector<double> startLiveCdf;
  std::vector<double> startLiveBillerCdf;

  /* The MEMBERSHIP law, built from the same live weights. See
   * commerce/reach.hpp — `startLiveCdf` is a VOLUME law and drawing
   * favourites from it puts one merchant on 51% of all cards. */
  commerce::ReachModel reachModel;
  std::vector<double> reachCdf;

  /* The set size the reach solve inverts against: an inclusion probability is
   * only meaningful per NUMBER OF DRAWS, since pi = 1-(1-q)^k.
   *
   * THE SOLVE MUST USE `favoriteMax`, NOT THE SEEDED MEAN OF 19.
   * `evolveFavorites` adds at 0.35/mo against a 0.10/mo voluntary drop, so a
   * set climbs from its seed to the evolver's cap and SITS there — 20 years
   * is 240 monthly steps. Solving against 19 pins reach correctly for the
   * 60-day golden and lets realized reach drift above target on every long
   * run: a window-length-dependent constant. Solving against the cap
   * under-shoots on short windows instead, the safe direction for a defect
   * whose failure mode is a hub.
   *
   * MUST TRACK `math::evolution::Config::maxFavorites` — the same coupling
   * `favoriteCapacity` documents. The monthly rebuild in
   * `dynamics/monthly/evolution.cpp` passes that config value directly. */
  const double meanSetSize = static_cast<double>(payees.favoriteMax);

  if (catalog != nullptr && !catalog->records.empty()) {
    const auto startTs = time::toEpochSeconds(sources.bounds.startDate);
    std::vector<double> liveWeights;
    liveWeights.reserve(catalog->records.size());
    for (const auto &rec : catalog->records) {
      liveWeights.push_back(rec.liveAt(startTs) ? rec.weight : 0.0);
    }
    startLiveCdf = commerce::cdfOrEmpty(liveWeights);

    reachModel = commerce::buildReachModel(liveWeights, meanSetSize);
    reachCdf = commerce::cdfOrEmpty(reachModel.membership);

    std::vector<double> liveBillerWeights(catalog->records.size(), 0.0);
    for (std::size_t i = 0; i < catalog->records.size(); ++i) {
      const auto &rec = catalog->records[i];
      if (isBillerCategory(rec.category) && rec.liveAt(startTs)) {
        liveBillerWeights[i] = rec.weight;
      }
    }
    startLiveBillerCdf = commerce::cdfOrEmpty(liveBillerWeights);
  }

  /* THE POOLS MUST COVER THE UNION OF EVERY AREA ANYONE EVER OCCUPIES, not
   * the window-start snapshot. A household moving into an area with no pool
   * would silently fall back to the national CDF — distance decay switching
   * itself off for a mover, a quiet degradation that looks correct in
   * aggregate. */
  const auto pooledAreas = sources.census.relocation != nullptr &&
                                   !sources.census.relocation->empty()
                               ? sources.census.relocation->allAreas()
                               : std::vector<entity::geography::GeoAreaId>(
                                     sources.census.homeAreas.begin(),
                                     sources.census.homeAreas.end());

  /* The MEMBERSHIP sampler, built here because the favourite draw below is
   * home-conditioned. Online merchants route nationally; physical merchants
   * route through the home area's distance-decay pool, so a single-centroid
   * outlet is reachable only by people near it. */
  commerce::MembershipSampler membership;
  if (catalog != nullptr && !reachModel.membership.empty()) {
    membership = commerce::MembershipSampler::build(
        *catalog, synthgeo::geography(), pooledAreas, reachModel.membership,
        meanSetSize,
        commerce::cnpShareForYear(
            time::toCalendarDate(sources.bounds.startDate).year));
  }

  /* Person i's home area at window start. The relocation schedule re-points
   * homes monthly, but the initial favourite draw is a window-start fact, so
   * the window-start home is the right one here — the same reason
   * `startLiveCdf` uses window-start liveness. */
  const auto homeOf = [&](std::uint32_t i) -> entity::geography::GeoAreaId {
    return i < sources.census.homeAreas.size()
               ? sources.census.homeAreas[i]
               : entity::geography::invalidGeoArea;
  };

  std::vector<std::uint32_t> favOffsets;
  favOffsets.reserve(sources.census.count + 1);
  favOffsets.push_back(0);

  std::vector<std::uint32_t> favCounts;
  favCounts.reserve(sources.census.count);

  std::vector<std::uint32_t> favFlat;

  std::vector<std::uint32_t> billOffsets;
  billOffsets.reserve(sources.census.count + 1);
  billOffsets.push_back(0);

  std::vector<std::uint32_t> billCounts;
  billCounts.reserve(sources.census.count);

  std::vector<std::uint32_t> billFlat;

  std::vector<std::uint32_t> rowScratch;
  rowScratch.reserve(payees.favoriteMax);

  std::vector<float> exploreProp(sources.census.count);

  /* One CSR row of burst windows per person, built in day order so the
   * hot-path scan in `calculateExploreP` can exit early. */
  std::vector<std::uint32_t> burstOffsets;
  burstOffsets.reserve(sources.census.count + 1);
  burstOffsets.push_back(0);
  std::vector<commerce::BurstWindow> burstFlat;

  const WeightedPicker picker{payees.picking.maxPickAttempts};

  for (std::uint32_t i = 0; i < sources.census.count; ++i) {
    auto rng = makePerPersonRng(factory, i);

    const std::uint16_t favK = static_cast<std::uint16_t>(
        rng.uniformInt(payees.favoriteMin, payees.favoriteMax + 1));

    rowScratch.clear();
    /* THE INITIAL FAVOURITE SET IS DRAWN LIVE, FROM THE REACH LAW, AND
     * HOME-CONDITIONED. All three matter and each has its own measured
     * failure:
     *
     * LIVE AT WINDOW START, not the whole catalogue. The exploit path reads
     * this list directly rather than the CDF, so seeding a person with
     * merchants that have not opened yet produces transactions dated before
     * the merchant existed — measured at 49% of merchant rows out of tenure.
     *
     * REACH, NOT VOLUME. Membership is a graph EDGE and `favK ~ U[8,30]`
     * draws turn a weight `w` into `1-(1-w)^favK`, a favK-fold amplification
     * of a volume weight into an edge probability. Drawing against
     * `startLiveCdf` (the live popularity CDF) put the top merchant's ~5%
     * volume weight on 51% of every card, with P(two random cards share a
     * merchant) = 1.000. `reachCdf` is the same weights flattened by a solved
     * exponent. Falls back to the volume law only with no catalogue at all.
     *
     * HOME-CONDITIONED. The sampler routes online merchants nationally and
     * physical merchants through the home area's distance-decay pool. A
     * single national CDF put the mean home-to-favourite distance at 1,206
     * MILES with 4.96% inside 50 miles, for merchants the exporter publishes
     * as single-centroid acceptance LOCATIONS. Falls back to the
     * geography-free reach CDF only when the sampler could not be built. */
    if (membership.ready()) {
      picker.pickFrom(rng, favK, rowScratch, [&](double u) {
        return membership.sample(homeOf(i), u);
      });
    } else {
      picker.pick(rng,
                  !reachCdf.empty()      ? reachCdf
                  : startLiveCdf.empty() ? merchCdf
                                         : startLiveCdf,
                  favK, rowScratch);
    }

    favFlat.insert(favFlat.end(), rowScratch.begin(), rowScratch.end());
    favCounts.push_back(static_cast<std::uint32_t>(rowScratch.size()));
    /* Pad to FULL CAPACITY so the monthly evolution pass can add and drop in
     * O(1) without shifting offsets. The padding is never read: `rowOf`
     * returns only the live prefix. */
    favFlat.resize(favFlat.size() +
                   (payees.favoriteCapacity > rowScratch.size()
                        ? payees.favoriteCapacity - rowScratch.size()
                        : 0));
    favOffsets.push_back(static_cast<std::uint32_t>(favFlat.size()));

    const std::uint16_t billK = static_cast<std::uint16_t>(
        rng.uniformInt(payees.billerMin, payees.billerMax + 1));

    rowScratch.clear();
    picker.pick(rng,
                startLiveBillerCdf.empty() ? billerCdf : startLiveBillerCdf,
                billK, rowScratch);

    billFlat.insert(billFlat.end(), rowScratch.begin(), rowScratch.end());
    billCounts.push_back(static_cast<std::uint32_t>(rowScratch.size()));
    billFlat.resize(billFlat.size() +
                    (payees.billerCapacity > rowScratch.size()
                         ? payees.billerCapacity - rowScratch.size()
                         : 0));
    billOffsets.push_back(static_cast<std::uint32_t>(billFlat.size()));

    /* Own lane: immune to the picker's variable retry count above. */
    auto behaviorRng = makeLaneRng(factory, "payee-behavior", i);

    exploreProp[i] = static_cast<float>(probability::distributions::beta(
        behaviorRng, behavior.exploration.alpha, behavior.exploration.beta));

    /* ONE BERNOULLI PER WHOLE YEAR of the window, not one per run. The draw
     * count therefore depends on the WINDOW LENGTH and on no other data — the
     * same person at the same seed gets the same first N windows regardless
     * of how the pool or the roster changes, which is what keeps this lane's
     * separation meaningful. `buildPersonBursts` is the single source of
     * truth and is what the gate measures. */
    for (const auto &window :
         buildPersonBursts(behaviorRng, behavior.burst,
                           static_cast<int>(sources.bounds.days))) {
      burstFlat.push_back(window);
    }
    burstOffsets.push_back(static_cast<std::uint32_t>(burstFlat.size()));
  }

  commerce::MerchantSelection selection{catalog, std::move(merchCdf),
                                        std::move(billerCdf)};

  /* Carried on the selection so the FOLD and the monthly evolver read the
   * identical law. ANY CARRIER THE FOLD READS MUST BE BUILT HERE, not wired
   * into production separately: `gate_world.hpp` builds its own market
   * through this same function, so the harness gets it automatically. A
   * carrier wired into production but not the harness once produced a
   * 5,156-row `test_arch_equivalence` divergence that read exactly like a
   * windowing bug. */
  selection.reachMutable() = std::move(reachModel);
  selection.reachCdf() = std::move(reachCdf);
  selection.membershipMutable() = std::move(membership);

  commerce::AssignedPayees assignedPayees{
      commerce::Favorites{std::move(favOffsets), std::move(favFlat),
                          std::move(favCounts)},
      commerce::Billers{std::move(billOffsets), std::move(billFlat),
                        std::move(billCounts)}};

  commerce::ShopperActivity activity{
      std::move(exploreProp),
      commerce::BurstSchedule{std::move(burstOffsets), std::move(burstFlat)}};

  social::Contacts contacts =
      buildSocialContacts(sources.census.count, sources.baseSeed);

  /* `LocalPools` over the VOLUME weights — the exploration pool, distinct
   * from the reach-weighted membership sampler above. Pure data derived from
   * the catalogue and geography, no RNG, so it moves no golden. Empty with no
   * catalogue, in which case card-present selection falls back to the
   * national CDF.
   *
   * Same distribution as the dense predecessor — the within-area factor is
   * home-independent, so that matrix stored one vector once per area — at
   * O(M + A*k) instead of O(A*M). local_pools.hpp carries the measured memory
   * pair. */
  std::vector<double> volumeWeights;
  if (catalog != nullptr) {
    volumeWeights.reserve(catalog->records.size());
    for (const auto &rec : catalog->records) {
      volumeWeights.push_back(rec.weight);
    }
  }
  commerce::LocalPools geoPools =
      catalog != nullptr
          ? commerce::LocalPools::build(*catalog, synthgeo::geography(),
                                        pooledAreas, volumeWeights)
          : commerce::LocalPools{};

  commerce::View commerceView(std::move(selection), std::move(assignedPayees),
                              std::move(activity), std::move(contacts),
                              std::move(geoPools));

  Cards cards = normalizeCards(std::move(sources.cards), sources.census.count);

  return Market(sources.bounds, std::move(popView), std::move(commerceView),
                std::move(cards));
}

} // namespace PhantomLedger::activity::spending::market
