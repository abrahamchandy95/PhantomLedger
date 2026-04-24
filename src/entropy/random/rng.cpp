#include "phantomledger/entropy/random/rng.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace PhantomLedger::random {
namespace {

constexpr std::uint64_t kSplitmixGamma = 0x9e3779b97f4a7c15ULL;
constexpr std::uint64_t kSplitmixMix1 = 0xbf58476d1ce4e5b9ULL;
constexpr std::uint64_t kSplitmixMix2 = 0x94d049bb133111ebULL;

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t &state) noexcept {
  state += kSplitmixGamma;
  std::uint64_t value = state;
  value = (value ^ (value >> 30U)) * kSplitmixMix1;
  value = (value ^ (value >> 27U)) * kSplitmixMix2;
  return value ^ (value >> 31U);
}

// Floyd's O(k) sample-without-replacement: cache-friendly for small k
// against arbitrarily large n. Produces the same set distribution as
// partial Fisher-Yates but uses O(k) memory instead of O(n).
//
// The output order is *not* uniformly random over permutations; it is
// whatever Floyd's insertion order gives. Every PhantomLedger caller
// treats the result as an unordered subset (members, mules, victims,
// ring pools, persona assignments) so this is the correct trade.
[[nodiscard]] std::vector<std::size_t>
floydSample(auto &boundedFn, std::size_t n, std::size_t k) {
  std::vector<std::size_t> out;
  out.reserve(k);

  // j ranges over [n-k, n). `range` is j+1 for the inclusive draw.
  for (std::size_t j = n - k; j < n; ++j) {
    const std::uint64_t range = static_cast<std::uint64_t>(j) + 1ULL;
    const auto t = static_cast<std::size_t>(boundedFn(range));

    // Linear scan. With the k<=1024 guard in choiceIndices(), the
    // inner loop stays in L1 and this is faster than a hashset for
    // realistic inputs.
    const bool duplicate = std::find(out.begin(), out.end(), t) != out.end();
    out.push_back(duplicate ? j : t);
  }

  return out;
}

} // namespace

Rng Rng::fromSeed(std::uint64_t seed) {
  std::uint64_t state = seed;
  const std::uint64_t s0 = splitmix64(state);
  const std::uint64_t s1 = splitmix64(state);
  const std::uint64_t i0 = splitmix64(state);
  const std::uint64_t i1 = splitmix64(state) | 1ULL;
  return Rng(Pcg64(s0, s1, i0, i1));
}

std::int64_t Rng::uniformInt(std::int64_t low, std::int64_t high) {
  if (low >= high) {
    throw std::invalid_argument("uniformInt: low must be < high");
  }
  const auto range = static_cast<std::uint64_t>(high - low);
  return low + static_cast<std::int64_t>(bounded(range));
}

std::size_t Rng::choiceIndex(std::size_t n) {
  if (n == 0) {
    throw std::invalid_argument("choiceIndex: n must be > 0");
  }
  return static_cast<std::size_t>(bounded(static_cast<std::uint64_t>(n)));
}

std::vector<std::size_t> Rng::choiceIndices(std::size_t n, std::size_t k,
                                            bool replace) {
  if (k == 0) {
    return {};
  }
  if (n == 0) {
    throw std::invalid_argument("choiceIndices: n must be > 0 when k > 0");
  }
  if (!replace && k > n) {
    throw std::invalid_argument(
        "choiceIndices: k exceeds n without replacement");
  }

  if (replace) {
    std::vector<std::size_t> out(k);
    for (auto &idx : out) {
      idx = static_cast<std::size_t>(bounded(static_cast<std::uint64_t>(n)));
    }
    return out;
  }

  // Floyd's sample when k is small relative to n and small in absolute
  // terms: O(k) memory, O(k) RNG draws, O(k^2) worst-case compares that
  // stay L1-resident. The 2*k <= n guard prevents Floyd's from running
  // on near-exhaustive samples where partial Fisher-Yates is cheaper.
  constexpr std::size_t kFloydMaxK = 1024;
  if (k <= kFloydMaxK && k * 2 <= n) {
    auto boundedFn = [this](std::uint64_t range) { return bounded(range); };
    return floydSample(boundedFn, n, k);
  }

  // Partial Fisher-Yates fallback for large k / near-exhaustive sampling.
  std::vector<std::size_t> pool(n);
  std::iota(pool.begin(), pool.end(), std::size_t{0});
  for (std::size_t i = 0; i < k; ++i) {
    const auto j = i + static_cast<std::size_t>(
                           bounded(static_cast<std::uint64_t>(n - i)));
    std::swap(pool[i], pool[j]);
  }
  pool.resize(k);
  return pool;
}

std::uint64_t Rng::bounded(std::uint64_t range) noexcept {
  if (range <= 1) {
    return 0;
  }
  std::uint64_t x = engine_.next_u64();
#if defined(__SIZEOF_INT128__)
  __uint128_t product =
      static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
  auto low = static_cast<std::uint64_t>(product);
  if (low < range) {
    const std::uint64_t threshold = static_cast<std::uint64_t>(-range) % range;
    while (low < threshold) {
      x = engine_.next_u64();
      product = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
      low = static_cast<std::uint64_t>(product);
    }
  }
  return static_cast<std::uint64_t>(product >> 64U);
#else
  const std::uint64_t limit =
      (std::numeric_limits<std::uint64_t>::max() / range) * range;
  while (x >= limit) {
    x = engine_.next_u64();
  }
  return x % range;
#endif
}

} // namespace PhantomLedger::random
