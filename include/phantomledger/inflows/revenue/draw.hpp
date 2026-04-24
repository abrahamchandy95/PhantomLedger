#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::inflows::revenue {

using Key = entity::Key;

/// Uniform integer in [low, highExclusive).
[[nodiscard]] inline int randInt(random::Rng &rng, int low, int highExclusive) {
  if (highExclusive <= low) {
    return low;
  }
  return static_cast<int>(rng.uniformInt(low, highExclusive));
}

/// Pick one key uniformly from a span. Returns nullopt if empty.
[[nodiscard]] inline std::optional<Key> pickOne(random::Rng &rng,
                                                std::span<const Key> items) {
  if (items.empty()) {
    return std::nullopt;
  }
  const auto idx = rng.choiceIndex(items.size());
  return items[idx];
}

/// Sample k items without replacement from a pool (Fisher-Yates partial).
/// k is drawn uniformly from [low, high] and clamped to pool size.
[[nodiscard]] inline std::vector<Key>
choiceK(random::Rng &rng, std::span<const Key> items, int low, int high) {
  if (items.empty()) {
    return {};
  }

  low = std::max(1, low);
  high = std::max(low, high);
  int k = randInt(rng, low, high + 1);
  k = std::min(k, static_cast<int>(items.size()));

  // Partial Fisher-Yates on a mutable copy.
  std::vector<Key> pool(items.begin(), items.end());
  std::vector<Key> out;
  out.reserve(static_cast<std::size_t>(k));

  for (int i = 0; i < k; ++i) {
    const auto remaining = static_cast<int>(pool.size()) - i;
    const int j = i + randInt(rng, 0, remaining);
    std::swap(pool[static_cast<std::size_t>(i)],
              pool[static_cast<std::size_t>(j)]);
    out.push_back(pool[static_cast<std::size_t>(i)]);
  }

  return out;
}

/// Draw a payment count uniformly from [low, high] (inclusive).
[[nodiscard]] inline int paymentCount(random::Rng &rng, int low, int high) {
  return randInt(rng, low, high + 1);
}

} // namespace PhantomLedger::inflows::revenue
