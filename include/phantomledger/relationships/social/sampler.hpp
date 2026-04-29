#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/relationships/social/communities.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::relationships::social {

class ContactSampler {
public:
  ContactSampler(std::vector<double> attract, const Communities &communities,
                 double localProb, double crossProb);

  void drawUnique(std::uint32_t srcIdx, std::uint32_t desiredCount,
                  random::Rng &rng, std::vector<std::uint32_t> &out) const;

  [[nodiscard]] std::uint32_t personCount() const noexcept {
    return personCount_;
  }
  [[nodiscard]] const std::vector<double> &globalCdf() const noexcept {
    return globalCdf_;
  }
  [[nodiscard]] const std::vector<double> &
  blockCdf(std::uint32_t blockIdx) const noexcept {
    return blockCdfs_[blockIdx];
  }
  [[nodiscard]] const Communities *communities() const noexcept {
    return communities_;
  }
  [[nodiscard]] double localProb() const noexcept { return localProb_; }
  [[nodiscard]] double crossProb() const noexcept { return crossProb_; }

private:
  const Communities *communities_ = nullptr;
  std::uint32_t personCount_ = 0;
  std::vector<double> globalCdf_;
  std::vector<std::vector<double>> blockCdfs_;
  double localProb_ = 0.0;
  double crossProb_ = 0.0;
};

} // namespace PhantomLedger::relationships::social
