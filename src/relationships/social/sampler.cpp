#include "phantomledger/relationships/social/sampler.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace PhantomLedger::relationships::social {
namespace {
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
void fillRemainingUnique(std::uint32_t srcIdx, std::uint32_t desiredCount,
                         std::uint32_t personCount,
                         std::vector<std::uint32_t> &out) {
  if (personCount <= 1U) {
    return;
  }
  std::uint32_t stride = (srcIdx * 1103515245U + 12345U) | 1U;
  stride %= personCount;
  if (stride == 0U) {
    stride = 1U;
  }
  for (std::uint32_t step = 1U; step < personCount && out.size() < desiredCount;
       ++step) {
    const auto candidate =
        static_cast<std::uint32_t>((srcIdx + step * stride) % personCount);
    if (candidate == srcIdx) {
      continue;
    }
    if (!containsLinear(std::span<const std::uint32_t>{out}, candidate)) {
      out.push_back(candidate);
    }
  }
  for (std::uint32_t step = 1U; step < personCount && out.size() < desiredCount;
       ++step) {
    const auto candidate = (srcIdx + step) % personCount;
    if (candidate == srcIdx) {
      continue;
    }
    if (!containsLinear(std::span<const std::uint32_t>{out}, candidate)) {
      out.push_back(candidate);
    }
  }
}
} // namespace
ContactSampler::ContactSampler(std::vector<double> attract,
                               const Communities &communities, double localProb,
                               double crossProb)
    : communities_(&communities),
      personCount_(static_cast<std::uint32_t>(attract.size())),
      localProb_(localProb), crossProb_(crossProb) {
  if (personCount_ == 0U || communities.count() == 0U) {
    return;
  }
  globalAlias_.build(std::span<const double>{attract});
  /* Sized once; reused across every drawUnique call via epoch stamping. */
  seenMark_.assign(personCount_, 0U);
  seenEpoch_ = 0U;
  const auto blockCount = communities.count();
  blockAliases_.reserve(blockCount);
  for (std::uint32_t b = 0U; b < blockCount; ++b) {
    const auto lo = communities.starts[b];
    const auto hi = communities.ends[b];
    blockAliases_.emplace_back(std::span<const double>{
        attract.data() + lo, static_cast<std::size_t>(hi - lo)});
  }
}
std::uint32_t
ContactSampler::fallbackOther(std::uint32_t srcIdx) const noexcept {
  if (personCount_ <= 1U) {
    return srcIdx;
  }
  return (srcIdx + 1U) % personCount_;
}

std::uint32_t ContactSampler::drawLocal(std::uint32_t srcIdx,
                                        std::uint32_t blockIdx,
                                        random::Rng &rng) const {
  const auto blockLo = communities_->starts[blockIdx];
  const auto blockHi = communities_->ends[blockIdx];
  if (blockHi - blockLo <= 1U) {
    return fallbackOther(srcIdx);
  }
  for (int attempt = 0; attempt < kLocalMaxTries; ++attempt) {
    const auto pickInBlock =
        static_cast<std::uint32_t>(blockAliases_[blockIdx].sample(rng));
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
  if (personCount_ <= 1U) {
    return srcIdx;
  }
  for (int attempt = 0; attempt < kGlobalMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(globalAlias_.sample(rng));
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
  if (personCount_ - (blockHi - blockLo) == 0U) {
    return drawGlobal(srcIdx, rng);
  }
  for (int attempt = 0; attempt < kCrossMaxTries; ++attempt) {
    const auto candidate = static_cast<std::uint32_t>(globalAlias_.sample(rng));
    if (candidate < blockLo || candidate >= blockHi) {
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
  if (desiredCount == 0U || personCount_ == 0U) {
    return;
  }
  if (personCount_ == 1U) {
    out.push_back(srcIdx);
    return;
  }
  const auto wanted = std::min(desiredCount, personCount_ - 1U);
  out.reserve(wanted);
  const auto blockIdx = communities_->memberOf[srcIdx];
  /* O(1) dedup: bump the epoch to logically clear the membership set, and on
   * the rare wraparound to 0 reset the buffer once. `srcIdx` is pre-marked so
   * it can never be selected. */
  if (++seenEpoch_ == 0U) {
    std::fill(seenMark_.begin(), seenMark_.end(), 0U);
    seenEpoch_ = 1U;
  }
  seenMark_[srcIdx] = seenEpoch_;
  const int maxTries = std::max<int>(
      kDedupBaseTries, kDedupMultiplier * static_cast<int>(wanted));
  for (int attempt = 0; attempt < maxTries && out.size() < wanted; ++attempt) {
    const auto candidate = drawCandidate(srcIdx, blockIdx, rng);
    if (seenMark_[candidate] != seenEpoch_) {
      seenMark_[candidate] = seenEpoch_;
      out.push_back(candidate);
    }
  }
  fillRemainingUnique(srcIdx, wanted, personCount_, out);
  if (out.empty()) {
    out.push_back(fallbackOther(srcIdx));
  }
}
} // namespace PhantomLedger::relationships::social
