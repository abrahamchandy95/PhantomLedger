#pragma once

#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace PhantomLedger::math::evolution {

struct Config {
  double merchantAddP = 0.35;
  double merchantDropP = 0.10;
  double contactAddP = 0.08;
  double contactDropP = 0.03;

  /* MUST STAY PINNED TO `PayeeSelectionRules::favoriteMax` (30). `add` runs
   * 3.5x `drop`, so any cap above what the seed can produce is a ratchet:
   * at 40 every set grew monotonically past its own seeded ceiling — 19.1 ->
   * 37.8 over 240 monthly steps — and, because the add pass samples a
   * weighted pool while the drop pass is uniform, everyone converged onto
   * the same global head (top-1 card penetration 0.494 -> 0.853, P(two
   * random people share a favourite) 0.93 -> 1.000).
   *
   * The empirical anchor is that the set SIZE is conserved while its
   * MEMBERSHIP turns over — Alessandretti, Sapiezynski, Sekara, Lehmann,
   * Baronchelli, "Evidence for a conserved quantity in human mobility",
   * Nature Human Behaviour 2:485-491 (2018): ~25 familiar locations, size
   * conserved over multi-year traces. CITED (accessed 2026-08-04) for the
   * CONSERVATION property; the level 30 is a declared choice. Turnover
   * survives via forced closure drops plus the uniform voluntary drop, which
   * churn membership against the cap rather than freezing it. */
  int maxFavorites = 30;
  int maxContacts = 20;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("merchantAddP", merchantAddP); });
    r.check([&] { v::unit("merchantDropP", merchantDropP); });
    r.check([&] { v::unit("contactAddP", contactAddP); });
    r.check([&] { v::unit("contactDropP", contactDropP); });
    r.check([&] { v::ge("maxFavorites", maxFavorites, 5); });
    r.check([&] { v::ge("maxContacts", maxContacts, 3); });
  }
};

inline constexpr Config kDefaultConfig{};

namespace detail {

template <class It>
[[nodiscard]] inline bool contains(It first, It last,
                                   std::uint32_t value) noexcept {
  return std::find(first, last, value) != last;
}

} // namespace detail

/* The law the monthly favourite ADD pass samples.
 *
 * IT MUST CARRY REACH, NEVER THE POPULARITY (VOLUME) LAW, and it must be the
 * same HOME-CONDITIONED membership sampler the bootstrap draw uses. Feeding
 * it a volume CDF rebuilds the global hub through the back door, and feeding
 * it a geography-free law rebuilds a geography-free favourite set one month
 * at a time: 240 monthly adds dominate a 19-merchant seed either way.
 *
 * A ONE-UNIFORM SAMPLER, NOT A CDF. `std::function` is acceptable here
 * because this runs once per person per MONTH, not per transaction. */
struct MerchantPool {
  std::function<std::uint32_t(double)> sample{};
  std::uint32_t totalCount = 0;

  [[nodiscard]] bool ready() const noexcept {
    return static_cast<bool>(sample);
  }

  [[nodiscard]] std::uint32_t samplerIndex(random::Rng &rng) const {
    return sample(rng.nextDouble());
  }
};

struct ContactRow {
  std::span<std::uint32_t> row{};
  std::uint32_t personIdx = 0;
  std::uint32_t nPeople = 0;

  [[nodiscard]] bool isValidNew(std::uint32_t candidate) const noexcept {
    if (candidate == personIdx) {
      return false;
    }
    return !detail::contains(row.begin(), row.end(), candidate);
  }
};

/* Evolve favorite merchant indices in place. */
inline void evolveFavorites(random::Rng &rng, const Config &cfg,
                            std::vector<std::uint32_t> &favorites,
                            const MerchantPool &pool) {
  /* Add pass: ONE weighted loop, and DO NOT ADD A UNIFORM FALLBACK. A
   * `rng.choiceIndex(pool.totalCount)` fallback ranges over the FULL
   * catalogue length — dead and unborn records included, since
   * `rebuildLiveMerchCdf` masks a closed merchant's weight to zero but keeps
   * its slot — so it can favourite a shop that has shut or not yet opened,
   * reopening the out-of-tenure hole the live-at-start bootstrap CDF exists
   * to close (49% of merchant rows when that hole was open). It fires on
   * ~2e-5 of adds, which is why nothing caught it.
   *
   * Extending the weighted loop keeps the same attempt budget and cannot
   * reach a non-live record, because the reach law gives one zero
   * probability. A run of 20 collisions simply skips the add — a favourite
   * not gained is strictly better than one gained at a closed shop. */
  if (pool.ready() && rng.coin(cfg.merchantAddP) &&
      favorites.size() < static_cast<std::size_t>(cfg.maxFavorites) &&
      favorites.size() < pool.totalCount) {
    for (int attempt = 0; attempt < 20; ++attempt) {
      const auto candidate = pool.samplerIndex(rng);
      if (!detail::contains(favorites.begin(), favorites.end(), candidate)) {
        favorites.push_back(candidate);
        break;
      }
    }
  }

  /* Drop pass. */
  if (rng.coin(cfg.merchantDropP) && favorites.size() > 3) {
    const auto dropIdx = rng.choiceIndex(favorites.size());
    favorites.erase(favorites.begin() + static_cast<std::ptrdiff_t>(dropIdx));
  }
}

inline void evolveContacts(random::Rng &rng, const Config &cfg,
                           const ContactRow &contact) {
  const auto degree = contact.row.size();
  if (degree == 0) {
    return;
  }

  if (rng.coin(cfg.contactAddP)) {
    for (int attempt = 0; attempt < 10; ++attempt) {
      const auto candidate =
          static_cast<std::uint32_t>(rng.choiceIndex(contact.nPeople));
      if (!contact.isValidNew(candidate)) {
        continue;
      }
      const auto slot = rng.choiceIndex(degree);
      contact.row[slot] = candidate;
      break;
    }
  }

  if (rng.coin(cfg.contactDropP) && degree >= 2) {
    const auto dropSlot = rng.choiceIndex(degree);
    const auto keepSlot = rng.choiceIndex(degree);
    if (dropSlot != keepSlot) {
      contact.row[dropSlot] = contact.row[keepSlot];
    }
  }
}

} // namespace PhantomLedger::math::evolution
