#include "phantomledger/relationships/social/builder.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/probability/distributions/gamma.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/relationships/social/communities.hpp"
#include "phantomledger/relationships/social/sampler.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::social {

namespace {

namespace cdf = ::PhantomLedger::distributions;
namespace probdist = ::PhantomLedger::probability::distributions;
namespace commerce = ::PhantomLedger::spending::market::commerce;

[[nodiscard]] std::vector<double>
buildAttractiveness(random::Rng &rng, std::uint32_t personCount,
                    std::span<const std::uint8_t> hubFlags, double sigma,
                    double hubMultiplier) {
  std::vector<double> attract(personCount, 0.0);

  for (std::uint32_t i = 0; i < personCount; ++i) {
    attract[i] = probdist::lognormal(rng, /*mu=*/0.0, sigma);
  }

  if (!hubFlags.empty()) {
    const auto limit = std::min<std::size_t>(hubFlags.size(), attract.size());
    for (std::size_t i = 0; i < limit; ++i) {
      if (hubFlags[i] != 0) {
        attract[i] *= hubMultiplier;
      }
    }
  }

  return attract;
}

[[nodiscard]] std::uint32_t baseUniqueCount(int degree) noexcept {
  const int half = std::max(3, (degree + 1) / 2);
  const int target = std::min(degree, half);
  return static_cast<std::uint32_t>(std::max(1, target));
}

void populateRow(std::uint32_t srcIdx, std::span<std::uint32_t> rowSlots,
                 const ContactSampler &sampler, double tieStrengthShape,
                 random::Rng &rng, std::vector<std::uint32_t> &uniquesScratch,
                 std::vector<double> &tieWeightsScratch,
                 std::vector<double> &tieCdfScratch) {
  const auto degree = static_cast<std::uint32_t>(rowSlots.size());
  if (degree == 0) {
    return;
  }

  const auto needed =
      std::min(degree, baseUniqueCount(static_cast<int>(degree)));
  sampler.drawUnique(srcIdx, needed, rng, uniquesScratch);

  if (uniquesScratch.empty()) {
    return;
  }

  if (uniquesScratch.size() == 1U) {
    std::fill(rowSlots.begin(), rowSlots.end(), uniquesScratch.front());
    return;
  }

  tieWeightsScratch.clear();
  tieWeightsScratch.reserve(uniquesScratch.size());
  for (std::size_t i = 0; i < uniquesScratch.size(); ++i) {
    tieWeightsScratch.push_back(
        probdist::gamma(rng, tieStrengthShape, /*scale=*/1.0));
  }

  tieCdfScratch = cdf::buildCdf(std::span<const double>{tieWeightsScratch});

  for (std::uint32_t s = 0; s < degree; ++s) {
    const auto pick = static_cast<std::size_t>(cdf::sampleIndex(
        std::span<const double>{tieCdfScratch}, rng.nextDouble()));
    rowSlots[s] = uniquesScratch[std::min(pick, uniquesScratch.size() - 1U)];
  }
}

} // namespace

commerce::Contacts build(const Social &cfg, const BuildInputs &inputs) {
  if (inputs.personCount == 0) {
    return {};
  }

  const auto degree = cfg.effectiveDegree();
  const auto [cMin, cMax] = cfg.communityBounds();
  const random::RngFactory factory{inputs.baseSeed};

  auto communitiesRng = factory.rng({"social", "communities"});
  auto communities =
      buildCommunities(communitiesRng, inputs.personCount, cMin, cMax);

  auto attractRng = factory.rng({"social", "attractiveness"});
  auto attract =
      buildAttractiveness(attractRng, inputs.personCount, inputs.hubFlags,
                          cfg.influenceSigma, cfg.hubMultiplier);

  const ContactSampler sampler(std::move(attract), communities, cfg.localProb,
                               cfg.crossProb());

  auto contactsRng = factory.rng({"social", "contacts"});
  commerce::Contacts contacts(inputs.personCount,
                              static_cast<std::uint16_t>(degree));

  std::vector<std::uint32_t> uniquesScratch;
  std::vector<double> tieWeightsScratch;
  std::vector<double> tieCdfScratch;
  uniquesScratch.reserve(static_cast<std::size_t>(degree));
  tieWeightsScratch.reserve(static_cast<std::size_t>(degree));
  tieCdfScratch.reserve(static_cast<std::size_t>(degree));

  for (std::uint32_t i = 0; i < inputs.personCount; ++i) {
    populateRow(i, contacts.rowOfMutable(i), sampler, cfg.tieStrengthShape,
                contactsRng, uniquesScratch, tieWeightsScratch, tieCdfScratch);
  }

  return contacts;
}

} // namespace PhantomLedger::relationships::social
