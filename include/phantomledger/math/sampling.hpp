
#pragma once

#include "phantomledger/random/rng.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace PhantomLedger::math {

namespace detail {

/// Box-Muller: two uniform draws produce one standard normal variate.
/// We discard the second variate for simplicity; the hot path draws
/// one amount at a time and the PCG64 is fast enough that wasting
/// one draw is cheaper than maintaining paired state.
[[nodiscard]] inline double standardNormal(random::Rng &rng) {
  double u1 = rng.nextDouble();
  double u2 = rng.nextDouble();
  if (u1 < 1e-300)
    u1 = 1e-300;
  return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
}

} // namespace detail

// ===================================================================
// Continuous distributions
// ===================================================================

/// LogNormal parameterized by median (= exp(mu)).
/// median(X) = exp(mu), so mu = log(median).
[[nodiscard]] inline double lognormalByMedian(random::Rng &rng, double median,
                                              double sigma) {
  if (median <= 0.0) {
    throw std::invalid_argument("lognormalByMedian: median must be > 0");
  }
  if (sigma < 0.0) {
    throw std::invalid_argument("lognormalByMedian: sigma must be >= 0");
  }
  const double mu = std::log(median);
  return std::exp(mu + sigma * detail::standardNormal(rng));
}

/// LogNormal parameterized by mean and sigma (NumPy convention).
/// X ~ LogNormal(mean, sigma) means log(X) ~ Normal(mean, sigma^2).
[[nodiscard]] inline double lognormal(random::Rng &rng, double mean,
                                      double sigma) {
  if (sigma < 0.0) {
    throw std::invalid_argument("lognormal: sigma must be >= 0");
  }
  return std::exp(mean + sigma * detail::standardNormal(rng));
}

/// Gamma(shape, scale) via Marsaglia & Tsang's method.
[[nodiscard]] inline double gamma(random::Rng &rng, double shape,
                                  double scale) {
  if (shape <= 0.0 || scale <= 0.0) {
    throw std::invalid_argument("gamma: shape and scale must be > 0");
  }

  double adjShape = shape;
  double boost = 1.0;
  if (adjShape < 1.0) {
    // Ahrens-Dieter: Gamma(a) = Gamma(a+1) * U^(1/a) for a < 1.
    boost = std::pow(rng.nextDouble(), 1.0 / adjShape);
    adjShape += 1.0;
  }

  const double d = adjShape - 1.0 / 3.0;
  const double c = 1.0 / std::sqrt(9.0 * d);

  for (;;) {
    double x = detail::standardNormal(rng);
    double v = 1.0 + c * x;
    if (v <= 0.0)
      continue;

    v = v * v * v;
    double u = rng.nextDouble();
    if (u < 1e-300)
      u = 1e-300;

    if (u < 1.0 - 0.0331 * (x * x) * (x * x))
      return d * v * scale * boost;

    if (std::log(u) < 0.5 * x * x + d * (1.0 - v + std::log(v)))
      return d * v * scale * boost;
  }
}

/// Beta(alpha, beta) via ratio of independent gammas.
[[nodiscard]] inline double beta(random::Rng &rng, double alpha,
                                 double betaParam) {
  if (alpha <= 0.0 || betaParam <= 0.0) {
    throw std::invalid_argument("beta: alpha and beta must be > 0");
  }
  const double x = gamma(rng, alpha, 1.0);
  const double y = gamma(rng, betaParam, 1.0);
  return x / (x + y);
}

/// Normal(mean, stddev).
[[nodiscard]] inline double normal(random::Rng &rng, double mean,
                                   double stddev) {
  return mean + stddev * detail::standardNormal(rng);
}

/// Uniform double in [low, high).
[[nodiscard]] inline double uniform(random::Rng &rng, double low, double high) {
  return low + (high - low) * rng.nextDouble();
}

// ===================================================================
// Discrete distributions
// ===================================================================

/// Poisson(lambda).
/// Knuth's method for lambda < 30, transformed rejection for larger.
[[nodiscard]] inline std::int64_t poisson(random::Rng &rng, double lambda) {
  if (lambda <= 0.0)
    return 0;

  if (lambda < 30.0) {
    const double L = std::exp(-lambda);
    std::int64_t k = 0;
    double p = 1.0;
    do {
      ++k;
      p *= rng.nextDouble();
    } while (p > L);
    return k - 1;
  }

  // Transformed rejection (Ahrens-Dieter).
  const double sqrtLam = std::sqrt(lambda);
  const double logLam = std::log(lambda);
  const double b = 0.931 + 2.53 * sqrtLam;
  const double a = -0.059 + 0.02483 * b;
  const double invAlpha = 1.1239 + 1.1328 / (b - 3.4);
  const double vr = 0.9277 - 3.6224 / (b - 2.0);

  for (;;) {
    double u = rng.nextDouble() - 0.5;
    double v = rng.nextDouble();

    double us = 0.5 - std::abs(u);
    auto k = static_cast<std::int64_t>(
        std::floor((2.0 * a / us + b) * u + lambda + 0.43));
    if (k < 0)
      continue;

    if (us >= 0.07 && v <= vr)
      return k;
    if (us < 0.013 && v > us)
      continue;

    double kf = static_cast<double>(k);
    if (std::log(v * invAlpha / (a / (us * us) + b)) <=
        -lambda + kf * logLam - std::lgamma(kf + 1.0))
      return k;
  }
}

/// Binomial(n, p) via repeated Bernoulli for small n,
/// or normal approximation for large n.
[[nodiscard]] inline std::int64_t binomial(random::Rng &rng, std::int64_t n,
                                           double p) {
  if (n <= 0 || p <= 0.0)
    return 0;
  if (p >= 1.0)
    return n;

  if (n <= 64) {
    std::int64_t count = 0;
    for (std::int64_t i = 0; i < n; ++i) {
      if (rng.nextDouble() < p)
        ++count;
    }
    return count;
  }

  // For large n, use the waiting-time method or normal approximation.
  const double mean = static_cast<double>(n) * p;
  const double stddev = std::sqrt(mean * (1.0 - p));
  auto result =
      static_cast<std::int64_t>(std::round(normal(rng, mean, stddev)));
  return std::max(std::int64_t{0}, std::min(n, result));
}

// ===================================================================
// Money utilities
// ===================================================================

/// Round to 2 decimal places (money rounding).
[[nodiscard]] inline double roundMoney(double amount) {
  return std::round(amount * 100.0) / 100.0;
}

/// Clamp to [lo, hi].
[[nodiscard]] inline double clamp(double value, double lo, double hi) {
  if (value < lo)
    return lo;
  if (value > hi)
    return hi;
  return value;
}

/// max(floor, value) then round to cents.
[[nodiscard]] inline double floorAndRound(double value, double floor) {
  return roundMoney(std::max(floor, value));
}

} // namespace PhantomLedger::math
