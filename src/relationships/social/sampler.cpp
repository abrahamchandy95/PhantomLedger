#include "phantomledger/relationships/social/sampler.hpp"

#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::relationships::social {

namespace {

namespace dist = ::PhantomLedger::distributions;

inline constexpr int kLocalMaxTries = 8;
inline constexpr int kGlobalMaxTries = 8;
inline constexpr int kCrossMaxTries = 24;

inline constexpr int kDedupBaseTries = 24;
inline constexpr int kDedupMultiplier = 10;

[[nodiscard]] bool containsLinear(std::span<const std::uint32_t> seen,
                                  std::uint32_t value) noexcept {
  for (const auto v : seen) {
    if (v == value) {
      return true;
    }
  }
  return false;
}

} // namespace

ContactSampler::ContactSampler(std::vector<double> attract,
                               const Communities &communities, double localProb,
                               double crossProb)
    : communities_(&communities),
      personCount_(static_cast<std::uint32_t>(attract.size())),
      localProb_(localProb), crossProb_(crossProb) {
  // Global CDF over the full attractiveness vector.
  globalCdf_ = dist::buildCdf(std::span<const double>{attract});

  const auto blockCount = communities.count();
  blockCdfs_.reserve(blockCount);
  for (std::uint32_t b = 0; b < blockCount; ++b) {
    const auto lo = communities.starts[b];
    const auto hi = communities.ends[b];
    blockCdfs_.push_back(
        dist::buildCdf(std::span<const double>{attract.data() + lo, hi - lo}));
  }
}

std::uint32_t
ContactSampler::fallbackOther(std::uint32_t srcIdx) const noexcept {
  if (personCount_ <= 1) {
    return srcIdx;
  }
  return (srcIdx + 1) % personCount_;
}

std::uint32_t ContactSampler::drawLocal(std::uint32_t srcIdx,
                                        std::uint32_t blockIdx,
                                        random::Rng &rng) const {
  const auto blockLo = communities_->starts[blockIdx];
  const auto blockHi = communities_->ends[blockIdx];
  const auto blockCdfView = std::span<const double>{blockCdfs_[blockIdx]};

  if (blockHi - blockLo <= 1) {
    return fallbackOther(srcIdx);
  }

  for (int attempt = 0; attempt < kLocalMaxTries; ++attempt) {
    const auto pickInBlock = static_cast<std::uint32_t>(
        dist::sampleIndex(blockCdfView, rng.nextDouble()));
    const auto candidate = blockLo + pickInBlock;
    if (candidate != srcIdx) {
      return candidate;
    }
  }

  if (srcIdx != blockLo) {
    return blockLo;
  }
  if (srcIdx + 1U < blockHi) {
    return srcIdx + 1U;
  }
  return fallbackOther(srcIdx);
}

std::uint32_t ContactSampler::drawGlobal(std::uint32_t srcIdx,
                                         random::Rng &rng) const {
  if (personCount_ <= 1) {
    return srcIdx;
  }

  const auto globalCdfView = std::span<const double>{globalCdf_};
  for (int attempt = 0; attempt < kGlobalMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(
        dist::sampleIndex(globalCdfView, rng.nextDouble()));
    if (candidate != srcIdx) {
      return candidate;
    }
  }

  return fallbackOther(srcIdx);
}

std::uint32_t ContactSampler::drawCross(std::uint32_t srcIdx,
                                        std::uint32_t blockIdx,
                                        random::Rng &rng) const {
  const auto blockLo = communities_->starts[blockIdx];
  const auto blockHi = communities_->ends[blockIdx];
  const auto blockSize = blockHi - blockLo;

  if (personCount_ - blockSize == 0) {
    return drawGlobal(srcIdx, rng);
  }

  const auto globalCdfView = std::span<const double>{globalCdf_};
  for (int attempt = 0; attempt < kCrossMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(
        dist::sampleIndex(globalCdfView, rng.nextDouble()));
    if (candidate != srcIdx && (candidate < blockLo || candidate >= blockHi)) {
      return candidate;
    }
  }

  return drawGlobal(srcIdx, rng);
}

std::uint32_t ContactSampler::drawCandidate(std::uint32_t srcIdx,
                                            std::uint32_t blockIdx,
                                            random::Rng &rng) const {
  const auto blockLo = communities_->starts[blockIdx];
  const auto blockHi = communities_->ends[blockIdx];
  const auto blockSize = blockHi - blockLo;

  if (blockSize > 1U && rng.coin(localProb_)) {
    return drawLocal(srcIdx, blockIdx, rng);
  }
  if (personCount_ - blockSize > 0U && rng.coin(crossProb_)) {
    return drawCross(srcIdx, blockIdx, rng);
  }
  return drawGlobal(srcIdx, rng);
}

void ContactSampler::drawUnique(std::uint32_t srcIdx,
                                std::uint32_t desiredCount, random::Rng &rng,
                                std::vector<std::uint32_t> &out) const {
  out.clear();
  if (desiredCount == 0 || personCount_ == 0) {
    return;
  }
  out.reserve(desiredCount);

  const auto blockIdx = communities_->memberOf[srcIdx];

  const int maxTries = std::max<int>(
      kDedupBaseTries, kDedupMultiplier * static_cast<int>(desiredCount));

  for (int attempt = 0; attempt < maxTries && out.size() < desiredCount;
       ++attempt) {
    const auto candidate = drawCandidate(srcIdx, blockIdx, rng);
    if (!containsLinear(std::span<const std::uint32_t>{out}, candidate)) {
      out.push_back(candidate);
    }
  }

  if (out.empty()) {
    out.push_back(fallbackOther(srcIdx));
  }
}

} // namespace PhantomLedger::relationships::social
