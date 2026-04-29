#include "phantomledger/relationships/social/sampler.hpp"

#include "phantomledger/probability/distributions/cdf.hpp"

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

[[nodiscard]] std::uint32_t fallbackOther(std::uint32_t srcIdx,
                                          std::uint32_t personCount) noexcept {
  if (personCount <= 1) {
    return srcIdx;
  }
  return (srcIdx + 1) % personCount;
}

[[nodiscard]] std::uint32_t
drawLocal(std::uint32_t srcIdx, std::uint32_t blockLo, std::uint32_t blockHi,
          std::span<const double> blockCdfView, std::uint32_t personCount,
          random::Rng &rng) {
  if (blockHi - blockLo <= 1) {
    return fallbackOther(srcIdx, personCount);
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
  return fallbackOther(srcIdx, personCount);
}

[[nodiscard]] std::uint32_t drawGlobal(std::uint32_t srcIdx,
                                       std::span<const double> globalCdfView,
                                       std::uint32_t personCount,
                                       random::Rng &rng) {
  if (personCount <= 1) {
    return srcIdx;
  }

  for (int attempt = 0; attempt < kGlobalMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(
        dist::sampleIndex(globalCdfView, rng.nextDouble()));
    if (candidate != srcIdx) {
      return candidate;
    }
  }

  return fallbackOther(srcIdx, personCount);
}

[[nodiscard]] std::uint32_t
drawCross(std::uint32_t srcIdx, std::uint32_t blockLo, std::uint32_t blockHi,
          std::span<const double> globalCdfView, std::uint32_t personCount,
          random::Rng &rng) {
  if (personCount - (blockHi - blockLo) == 0) {
    return drawGlobal(srcIdx, globalCdfView, personCount, rng);
  }

  for (int attempt = 0; attempt < kCrossMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(
        dist::sampleIndex(globalCdfView, rng.nextDouble()));
    if (candidate != srcIdx && (candidate < blockLo || candidate >= blockHi)) {
      return candidate;
    }
  }

  return drawGlobal(srcIdx, globalCdfView, personCount, rng);
}

[[nodiscard]] std::uint32_t
drawCandidate(std::uint32_t srcIdx, std::uint32_t blockLo,
              std::uint32_t blockHi, std::span<const double> blockCdfView,
              std::span<const double> globalCdfView, std::uint32_t personCount,
              double localProb, double crossProb, random::Rng &rng) {
  const auto blockSize = blockHi - blockLo;

  if (blockSize > 1U && rng.coin(localProb)) {
    return drawLocal(srcIdx, blockLo, blockHi, blockCdfView, personCount, rng);
  }
  if (personCount - blockSize > 0U && rng.coin(crossProb)) {
    return drawCross(srcIdx, blockLo, blockHi, globalCdfView, personCount, rng);
  }
  return drawGlobal(srcIdx, globalCdfView, personCount, rng);
}

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

void ContactSampler::drawUnique(std::uint32_t srcIdx,
                                std::uint32_t desiredCount, random::Rng &rng,
                                std::vector<std::uint32_t> &out) const {
  out.clear();
  if (desiredCount == 0 || personCount_ == 0) {
    return;
  }
  out.reserve(desiredCount);

  const auto blockIdx = communities_->memberOf[srcIdx];
  const auto blockLo = communities_->starts[blockIdx];
  const auto blockHi = communities_->ends[blockIdx];
  const std::span<const double> blockCdfView{blockCdfs_[blockIdx]};
  const std::span<const double> globalCdfView{globalCdf_};

  const int maxTries = std::max<int>(
      kDedupBaseTries, kDedupMultiplier * static_cast<int>(desiredCount));

  for (int attempt = 0; attempt < maxTries && out.size() < desiredCount;
       ++attempt) {
    const auto candidate =
        drawCandidate(srcIdx, blockLo, blockHi, blockCdfView, globalCdfView,
                      personCount_, localProb_, crossProb_, rng);
    if (!containsLinear(std::span<const std::uint32_t>{out}, candidate)) {
      out.push_back(candidate);
    }
  }

  if (out.empty()) {
    out.push_back(fallbackOther(srcIdx, personCount_));
  }
}

} // namespace PhantomLedger::relationships::social
