#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace PhantomLedger::math::timing {

namespace detail {

[[nodiscard]] consteval std::array<double, 24>
normalize(std::array<double, 24> p) {
  double sum = 0.0;
  for (auto x : p) {
    sum += x;
  }
  if (sum <= 0.0) {
    throw "timing: pmf must sum to a positive value";
  }
  for (auto &x : p) {
    x /= sum;
  }
  return p;
}

[[nodiscard]] consteval std::array<double, 24>
makeCdf(const std::array<double, 24> &p) {
  std::array<double, 24> cdf{};
  double sum = 0.0;
  for (std::size_t i = 0; i < 24; ++i) {
    sum += p[i];
    cdf[i] = sum;
  }
  // Clamp the last bin to 1.0 so a uniform draw of exactly the sum
  // never walks off the end.
  cdf[23] = 1.0;
  return cdf;
}

inline constexpr std::array<double, 24> kConsumerRaw{
    0.02, 0.01, 0.01, 0.01, 0.01, 0.02, 0.04, 0.06, 0.06, 0.05, 0.05, 0.05,
    0.05, 0.05, 0.05, 0.06, 0.07, 0.08, 0.07, 0.06, 0.05, 0.04, 0.03, 0.02,
};

inline constexpr std::array<double, 24> kConsumerDayRaw{
    0.01, 0.01, 0.01, 0.01, 0.01, 0.02, 0.04, 0.07, 0.08, 0.08, 0.07, 0.06,
    0.06, 0.06, 0.06, 0.06, 0.06, 0.05, 0.04, 0.03, 0.02, 0.02, 0.01, 0.01,
};

inline constexpr std::array<double, 24> kBusinessRaw{
    0.005, 0.003, 0.003, 0.003, 0.004, 0.010, 0.030, 0.060,
    0.080, 0.090, 0.090, 0.080, 0.070, 0.060, 0.060, 0.060,
    0.060, 0.050, 0.040, 0.020, 0.010, 0.008, 0.006, 0.005,
};

inline constexpr auto kConsumerCdf = makeCdf(normalize(kConsumerRaw));
inline constexpr auto kConsumerDayCdf = makeCdf(normalize(kConsumerDayRaw));
inline constexpr auto kBusinessCdf = makeCdf(normalize(kBusinessRaw));

[[nodiscard]] constexpr const std::array<double, 24> &
cdfFor(personas::Timing t) noexcept {
  switch (t) {
  case personas::Timing::consumer:
    return kConsumerCdf;
  case personas::Timing::consumerDay:
    return kConsumerDayCdf;
  case personas::Timing::business:
    return kBusinessCdf;
  }
  return kConsumerCdf;
}

[[nodiscard]] inline std::size_t pickHour(const std::array<double, 24> &cdf,
                                          double u) noexcept {
  std::size_t hour = 0;
  while (hour < 23 && u >= cdf[hour]) {
    ++hour;
  }
  return hour;
}

} // namespace detail

/// Offset in seconds from the start of the day.
[[nodiscard]] inline std::int32_t sampleOffset(random::Rng &rng,
                                               personas::Timing t) {
  const auto &cdf = detail::cdfFor(t);
  const auto hour = detail::pickHour(cdf, rng.nextDouble());
  const auto minute = rng.uniformInt(0, 60);
  const auto second = rng.uniformInt(0, 60);
  return static_cast<std::int32_t>(hour * 3600 + minute * 60 + second);
}

/// Fill a caller-owned span with n offsets. No heap allocation, no
/// virtual dispatch. Single CDF lookup, hot loop over rng draws.
inline void sampleOffsetsBatch(random::Rng &rng, personas::Timing t,
                               std::span<std::int32_t> out) {
  const auto &cdf = detail::cdfFor(t);
  for (auto &slot : out) {
    const auto hour = detail::pickHour(cdf, rng.nextDouble());
    const auto minute = rng.uniformInt(0, 60);
    const auto second = rng.uniformInt(0, 60);
    slot = static_cast<std::int32_t>(hour * 3600 + minute * 60 + second);
  }
}

} // namespace PhantomLedger::math::timing
