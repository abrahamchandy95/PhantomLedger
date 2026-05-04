#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/relationships/social/communities.hpp"

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

private:
  [[nodiscard]] std::uint32_t
  fallbackOther(std::uint32_t srcIdx) const noexcept;
  [[nodiscard]] std::uint32_t drawLocal(std::uint32_t srcIdx,
                                        std::uint32_t blockIdx,
                                        random::Rng &rng) const;
  [[nodiscard]] std::uint32_t drawGlobal(std::uint32_t srcIdx,
                                         random::Rng &rng) const;
  [[nodiscard]] std::uint32_t drawCross(std::uint32_t srcIdx,
                                        std::uint32_t blockIdx,
                                        random::Rng &rng) const;
  [[nodiscard]] std::uint32_t drawCandidate(std::uint32_t srcIdx,
                                            std::uint32_t blockIdx,
                                            random::Rng &rng) const;

  const Communities *communities_ = nullptr;
  std::uint32_t personCount_ = 0;
  std::vector<double> globalCdf_;
  std::vector<std::vector<double>> blockCdfs_;
  double localProb_ = 0.0;
  double crossProb_ = 0.0;
};

} // namespace PhantomLedger::relationships::social
