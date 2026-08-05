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

  // merchant-selection-2026-08: 40 -> 30, AND THE OLD VALUE WAS A RATCHET.
  //
  // `merchantAddP` (0.35/mo) is 3.5x `merchantDropP` (0.10/mo) and the cap
  // sat at 40 while `PayeeSelectionRules::favoriteMax` seeds at most 30. So
  // every set grew monotonically past its own seeded ceiling: measured 19.1
  // -> 37.8 over 240 monthly steps, converging to nearly the same size for
  // everyone and — because the add pass sampled the POPULARITY CDF while the
  // drop pass is uniform — converging toward the global top-40 by volume
  // weight. Measured effect on the defect this round exists for: top-1 card
  // penetration 0.494 -> 0.853, and P(two random people share a favourite)
  // 0.93 -> 1.000.
  //
  // The empirical anchor is that the set SIZE is conserved while its
  // MEMBERSHIP turns over — Alessandretti, Sapiezynski, Sekara, Lehmann,
  // Baronchelli, "Evidence for a conserved quantity in human mobility",
  // Nature Human Behaviour 2:485-491 (2018): ~25 familiar locations, size
  // conserved over multi-year traces. CITED (accessed 2026-08-04) for the
  // CONSERVATION property; the level 30 is a declared choice pinned to
  // `favoriteMax` so the cap can no longer exceed what the seed can produce.
  //
  // Turnover survives: forced closure drops plus the uniform voluntary drop
  // keep membership churning against the cap rather than freezing it. What
  // is gone is the monotone growth PAST the seeded range.
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

// The law the monthly favourite ADD pass samples.
//
// merchant-selection-2026-08: THIS MUST CARRY THE REACH CDF, NOT THE
// POPULARITY CDF. The caller used to pass `commerce.merchCdf()` — the volume
// law — so even after the bootstrap draw was corrected, 240 months of adds
// would have rebuilt the hub through the back door. `evolution.cpp` now
// passes `commerce.reachCdf()`.
// merchant-selection-2026-08 step 2: A ONE-UNIFORM SAMPLER, NOT A CDF. The add
// pass must draw from the same HOME-CONDITIONED membership law the bootstrap
// draw uses, or a long run rebuilds a geography-free favourite set one month at
// a time — 240 monthly adds against a 19-merchant seed would dominate it.
// `std::function` is acceptable here: this runs once per person per MONTH, not
// per transaction.
struct MerchantPool {
  std::function<std::uint32_t(double)> sample{};
  std::uint32_t totalCount = 0;

  [[nodiscard]] bool ready() const noexcept { return static_cast<bool>(sample); }

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

/// Evolve favorite merchant indices in place
inline void evolveFavorites(random::Rng &rng, const Config &cfg,
                            std::vector<std::uint32_t> &favorites,
                            const MerchantPool &pool) {
  // --- Add pass ---
  //
  // merchant-selection-2026-08: ONE weighted loop, and the uniform fallback
  // is GONE. It was `rng.choiceIndex(pool.totalCount)` over the FULL
  // catalogue length — dead and unborn records included, because
  // `rebuildLiveMerchCdf` masks a closed merchant's weight to zero but keeps
  // its slot. So the fallback could favourite a merchant that had shut or had
  // not opened, reopening exactly the out-of-tenure hole the live-at-start
  // bootstrap CDF exists to close (measured at 49% of merchant rows before
  // that fix). It fired on ~2e-5 of adds, which is why nothing caught it.
  //
  // Extending the weighted loop instead keeps the same attempt budget and
  // cannot reach a non-live record, because the reach CDF gives one zero
  // probability. A run of 20 collisions simply skips the add — a favourite
  // not gained is not a defect, and it is strictly better than a favourite
  // gained at a closed shop.
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

  // --- Drop pass ---
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
