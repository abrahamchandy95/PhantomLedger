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

  // geo-causal-v1 (G2a): the per-person home area (empty on the monolith
  // oracle → View::homeArea() then reports invalidGeoArea, i.e. no local
  // anchor). The windowed + test paths carry the real areas.
  std::vector<entity::geography::GeoAreaId> homeAreas(census.homeAreas.begin(),
                                                      census.homeAreas.end());

  // H2 step 2c: the per-person retirement day-index; H3: the death
  // day-index (both from the blueprint's persona-timeline lane — both
  // engines supply them identically).
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

  return population::View(std::move(primary), std::move(kinds),
                          std::move(objects), std::move(paydays),
                          std::move(homeAreas), std::move(retirementDays),
                          std::move(deathDays));
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

  std::vector<std::uint32_t> favOffsets;
  favOffsets.reserve(sources.census.count + 1);
  favOffsets.push_back(0);

  std::vector<std::uint32_t> favFlat;

  std::vector<std::uint32_t> billOffsets;
  billOffsets.reserve(sources.census.count + 1);
  billOffsets.push_back(0);

  std::vector<std::uint32_t> billFlat;

  std::vector<std::uint32_t> rowScratch;
  rowScratch.reserve(payees.favoriteMax);

  std::vector<float> exploreProp(sources.census.count);
  std::vector<std::uint32_t> burstStart(sources.census.count,
                                        commerce::kNoBurstDay);
  std::vector<std::uint16_t> burstLen(sources.census.count, 0u);

  const WeightedPicker picker{payees.picking.maxPickAttempts};

  for (std::uint32_t i = 0; i < sources.census.count; ++i) {
    auto rng = makePerPersonRng(factory, i);

    const std::uint16_t favK = static_cast<std::uint16_t>(
        rng.uniformInt(payees.favoriteMin, payees.favoriteMax + 1));

    rowScratch.clear();
    picker.pick(rng, merchCdf, favK, rowScratch);

    favFlat.insert(favFlat.end(), rowScratch.begin(), rowScratch.end());
    favOffsets.push_back(static_cast<std::uint32_t>(favFlat.size()));

    const std::uint16_t billK = static_cast<std::uint16_t>(
        rng.uniformInt(payees.billerMin, payees.billerMax + 1));

    rowScratch.clear();
    picker.pick(rng, billerCdf, billK, rowScratch);

    billFlat.insert(billFlat.end(), rowScratch.begin(), rowScratch.end());
    billOffsets.push_back(static_cast<std::uint32_t>(billFlat.size()));

    exploreProp[i] = static_cast<float>(probability::distributions::beta(
        rng, behavior.exploration.alpha, behavior.exploration.beta));

    if (sources.bounds.days > 0 && rng.coin(behavior.burst.probability)) {
      burstStart[i] = static_cast<std::uint32_t>(
          rng.uniformInt(0, static_cast<int>(sources.bounds.days)));

      burstLen[i] = static_cast<std::uint16_t>(
          rng.uniformInt(behavior.burst.minDays, behavior.burst.maxDays + 1));
    }
  }

  commerce::MerchantSelection selection{catalog, std::move(merchCdf),
                                        std::move(billerCdf)};

  commerce::AssignedPayees assignedPayees{
      commerce::Favorites{std::move(favOffsets), std::move(favFlat)},
      commerce::Billers{std::move(billOffsets), std::move(billFlat)}};

  commerce::ShopperActivity activity{
      std::move(exploreProp), std::move(burstStart), std::move(burstLen)};

  social::Contacts contacts =
      buildSocialContacts(sources.census.count, sources.baseSeed);

  // geo-causal-v1 (G2a step-2): the per-home-area distance-decay pools over
  // physical merchants (decay scale varies by the home area's urbanicity —
  // see geo_pools.hpp). Pure data derived from the catalogue + geography (no
  // RNG), so it moves no golden; the selection change (payments.cpp) reads it.
  // Empty when there is no catalogue or no home areas → card-present selection
  // falls back to the national CDF.
  commerce::GeographicMerchantPools geoPools =
      catalog != nullptr
          ? commerce::GeographicMerchantPools::build(
                *catalog, synthgeo::geography(), sources.census.homeAreas)
          : commerce::GeographicMerchantPools{};

  commerce::View commerceView(std::move(selection), std::move(assignedPayees),
                              std::move(activity), std::move(contacts),
                              std::move(geoPools));

  Cards cards = normalizeCards(std::move(sources.cards), sources.census.count);

  return Market(sources.bounds, std::move(popView), std::move(commerceView),
                std::move(cards));
}

} // namespace PhantomLedger::activity::spending::market
