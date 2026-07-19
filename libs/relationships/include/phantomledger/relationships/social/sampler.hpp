#pragma once
#include "phantomledger/primitives/random/distributions/alias.hpp"
#include "phantomledger/primitives/random/rng.hpp"
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
  using AliasTable = ::PhantomLedger::probability::distributions::AliasTable;
  [[nodiscard]] std::uint32_t
  fallbackOther(std::uint32_t srcIdx) const noexcept;
  [[nodiscard]] std::uint32_t
  fallbackCrossBlock(std::uint32_t blockIdx) const noexcept;
  [[nodiscard]] std::uint32_t drawFromBlock(std::uint32_t blockIdx,
                                            random::Rng &rng) const;
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
  void fillDeterministic(std::uint32_t srcIdx, std::uint32_t desiredCount,
                         std::vector<std::uint32_t> &out) const;
  const Communities *communities_ = nullptr;
  std::uint32_t personCount_ = 0;
  AliasTable globalAlias_;
  AliasTable blockMassAlias_;
  std::vector<AliasTable> blockAliases_;
  double localProb_ = 0.0;
  double crossProb_ = 0.0;
  // O(1) membership scratch for drawUnique's dedup. An index is "present" in
  // the current call when seenMark_[idx] == seenEpoch_. Bumping seenEpoch_ each
  // call logically clears the set without touching the buffer (except on the
  // rare epoch wraparound, handled in drawUnique).
  // mutable: drawUnique is const but must mutate this scratch state.
  // NON-thread-safe: use one ContactSampler per thread if the person loop is
  // ever parallelized.
  mutable std::vector<std::uint32_t> seenMark_;
  mutable std::uint32_t seenEpoch_ = 0;
};
} // namespace PhantomLedger::relationships::social
