#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::math::evolution {

struct Config {
  double merchantAddP = 0.35;
  double merchantDropP = 0.10;
  double contactAddP = 0.08;
  double contactDropP = 0.03;
  int maxFavorites = 40;
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

/// Evolve favorite merchant indices in place. First tries weighted
/// sampling from the global popularity CDF (organic discovery of
/// popular merchants); falls back to uniform if the weighted draw
/// keeps colliding with the existing set.
inline void evolveFavorites(random::Rng &rng, const Config &cfg,
                            std::vector<std::uint32_t> &favorites,
                            std::span<const double> merchCdf,
                            std::uint32_t totalMerchants) {
  // --- Add pass ---
  if (rng.coin(cfg.merchantAddP) &&
      favorites.size() < static_cast<std::size_t>(cfg.maxFavorites) &&
      favorites.size() < totalMerchants) {
    bool added = false;

    for (int attempt = 0; attempt < 10 && !added; ++attempt) {
      const auto candidate =
          static_cast<std::uint32_t>(PhantomLedger::distributions::sampleIndex(
              merchCdf, rng.nextDouble()));
      if (!detail::contains(favorites.begin(), favorites.end(), candidate)) {
        favorites.push_back(candidate);
        added = true;
      }
    }

    for (int attempt = 0; attempt < 10 && !added; ++attempt) {
      const auto candidate =
          static_cast<std::uint32_t>(rng.choiceIndex(totalMerchants));
      if (!detail::contains(favorites.begin(), favorites.end(), candidate)) {
        favorites.push_back(candidate);
        added = true;
      }
    }
  }

  // --- Drop pass ---
  if (rng.coin(cfg.merchantDropP) && favorites.size() > 3) {
    const auto dropIdx = rng.choiceIndex(favorites.size());
    favorites.erase(favorites.begin() + static_cast<std::ptrdiff_t>(dropIdx));
  }
}

/// Evolve a fixed-width contact row. Up to one slot mutates per call:
/// either an add (replace a slot with a fresh contact) or a drop
/// (duplicate an existing contact, reducing effective diversity).
inline void evolveContacts(random::Rng &rng, const Config &cfg,
                           std::span<std::uint32_t> contactsRow,
                           std::uint32_t personIdx, std::uint32_t nPeople) {
  const auto degree = contactsRow.size();
  if (degree == 0) {
    return;
  }

  if (rng.coin(cfg.contactAddP)) {
    for (int attempt = 0; attempt < 10; ++attempt) {
      const auto candidate =
          static_cast<std::uint32_t>(rng.choiceIndex(nPeople));
      if (candidate == personIdx) {
        continue;
      }
      if (!detail::contains(contactsRow.begin(), contactsRow.end(),
                            candidate)) {
        const auto slot = rng.choiceIndex(degree);
        contactsRow[slot] = candidate;
        break;
      }
    }
  }

  if (rng.coin(cfg.contactDropP) && degree >= 2) {
    const auto dropSlot = rng.choiceIndex(degree);
    const auto keepSlot = rng.choiceIndex(degree);
    if (dropSlot != keepSlot) {
      contactsRow[dropSlot] = contactsRow[keepSlot];
    }
  }
}

} // namespace PhantomLedger::math::evolution
