#pragma once

#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

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

struct MerchantPool {
  std::span<const double> cdf{};
  std::uint32_t totalCount = 0;

  /// Popularity-weighted draw via the merchant CDF.
  [[nodiscard]] std::uint32_t samplerIndex(random::Rng &rng) const {
    return static_cast<std::uint32_t>(
        PhantomLedger::probability::distributions::sampleIndex(
            cdf, rng.nextDouble()));
  }

  /// Uniform fallback when the CDF draw collides repeatedly.
  [[nodiscard]] std::uint32_t randomIndex(random::Rng &rng) const {
    return static_cast<std::uint32_t>(rng.choiceIndex(totalCount));
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
  if (rng.coin(cfg.merchantAddP) &&
      favorites.size() < static_cast<std::size_t>(cfg.maxFavorites) &&
      favorites.size() < pool.totalCount) {
    bool added = false;

    for (int attempt = 0; attempt < 10 && !added; ++attempt) {
      const auto candidate = pool.samplerIndex(rng);
      if (!detail::contains(favorites.begin(), favorites.end(), candidate)) {
        favorites.push_back(candidate);
        added = true;
      }
    }

    for (int attempt = 0; attempt < 10 && !added; ++attempt) {
      const auto candidate = pool.randomIndex(rng);
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
