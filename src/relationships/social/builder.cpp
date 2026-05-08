#include "phantomledger/relationships/social/builder.hpp"

#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/distributions/gamma.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/relationships/social/communities.hpp"
#include "phantomledger/relationships/social/sampler.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
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

class ContactRowBuilder {
public:
  ContactRowBuilder(const ContactSampler &sampler, double tieStrengthShape,
                    random::Rng &rng, std::uint32_t maxDegree)
      : sampler_(sampler), tieStrengthShape_(tieStrengthShape), rng_(rng) {
    const auto capacity = static_cast<std::size_t>(maxDegree);
    uniques_.reserve(capacity);
    tieWeights_.reserve(capacity);
    tieCdf_.reserve(capacity);
  }

  void populate(std::uint32_t srcIdx, std::span<std::uint32_t> rowSlots) {
    const auto degree = static_cast<std::uint32_t>(rowSlots.size());
    if (degree == 0) {
      return;
    }

    const auto needed =
        std::min(degree, baseUniqueCount(static_cast<int>(degree)));
    sampler_.drawUnique(srcIdx, needed, rng_, uniques_);

    if (uniques_.empty()) {
      return;
    }

    if (uniques_.size() == 1U) {
      std::fill(rowSlots.begin(), rowSlots.end(), uniques_.front());
      return;
    }

    drawTieWeights();
    fillWeighted(rowSlots);
  }

private:
  void drawTieWeights() {
    tieWeights_.clear();
    tieWeights_.reserve(uniques_.size());

    for (std::size_t i = 0; i < uniques_.size(); ++i) {
      tieWeights_.push_back(
          probdist::gamma(rng_, tieStrengthShape_, /*scale=*/1.0));
    }

    tieCdf_ = cdf::buildCdf(std::span<const double>{tieWeights_});
  }

  void fillWeighted(std::span<std::uint32_t> rowSlots) {
    for (auto &slot : rowSlots) {
      const auto pick = static_cast<std::size_t>(cdf::sampleIndex(
          std::span<const double>{tieCdf_}, rng_.nextDouble()));
      slot = uniques_[std::min(pick, uniques_.size() - 1U)];
    }
  }

  const ContactSampler &sampler_;
  double tieStrengthShape_ = 1.0;
  random::Rng &rng_;
  std::vector<std::uint32_t> uniques_;
  std::vector<double> tieWeights_;
  std::vector<double> tieCdf_;
};

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

  ContactRowBuilder rows{sampler, cfg.tieStrengthShape, contactsRng,
                         static_cast<std::uint32_t>(degree)};

  for (std::uint32_t i = 0; i < inputs.personCount; ++i) {
    rows.populate(i, contacts.rowOfMutable(i));
  }

  return contacts;
}

} // namespace PhantomLedger::relationships::social
