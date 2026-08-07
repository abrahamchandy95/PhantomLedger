#pragma once

/*
  BUILDING A PUBLIC-ENDPOINT POOL — the sampling half of
  `entities/infra/public_endpoints.hpp`, shared by the device and IP
  generators because the two populations differ only in what a link IS.

  SIZED OFF THE WORLD, NEVER OFF THE WINDOW. The line count is a function of
  the population alone, so a 60-day run and a 20-year run over the same roster
  hold the same terminals. Sizing it off `window.days` would make an endpoint's
  reach a window-length-dependent constant, which is the trap
  `merchant-churn` rule 1 and the burst-rate round each paid for once.

  THE DRAW COUNT HERE IS DATA-DEPENDENT — one reach uniform per line plus a
  chain whose link count depends on the window — so this must be built on an
  ISOLATED LANE. Both callers do that, and it is what lets the levels below be
  retuned without moving a single unrelated entity value.

  A fixed reach would put every public endpoint at the same fan-out and replace
  one cliff with another, so the weights are drawn from a bounded Pareto and the
  realized users-per-endpoint distribution spans a wide band.

  THE CEILING IS ON USERS, AND ITS FIRST FORM WAS ON THE WRONG VARIABLE.
  `kMaxReachMultiple` bounds the raw WEIGHT, and a weight is only ever read
  relative to the pool's total: a line's expected user count is
  `people * w / sum(w)`, which for a roster sized at `peoplePerLine` per line
  is `peoplePerLine * w / E[w]`. Bounding the RATIO at 40 against an `E[w]` of
  2.54 therefore declares a ceiling of `40 / 2.54 = 15.7` times the mean — a
  HIDDEN user cap that no constant states and that scales with the mean. The
  cap is now expressed in users directly and reached by solving a flattening
  exponent, which is the `commerce/reach.hpp` construction: draw-free, applied
  after every uniform is spent, and therefore unable to move a draw count.
 */

#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/infra/timeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::infra::publics {

/* DECLARED CHOICE. Pareto tail index of an endpoint's reach. 1.5 gives a
 * distribution whose top decile carries several times the median reach without
 * a single endpoint swallowing the pool. */
inline constexpr double kReachTailIndex = 1.5;

/* DECLARED CHOICE. Support bound on the DRAWN weight, which is what keeps the
 * inverse transform finite and the law's shape independent of how many lines
 * happened to be drawn. It is NOT the user ceiling — see `maxUsersPerLine`. */
inline constexpr double kMaxReachMultiple = 40.0;

struct PoolSpec {
  std::size_t lineCount = 0;

  /* The roster the pool serves. A line's expected user count is its share of
   * the reach CDF times this, so the ceiling below cannot be enforced without
   * it. */
  std::uint32_t people = 0;

  /* Ceiling on the EXPECTED users of the largest line. Zero leaves the drawn
   * law alone.
   *
   * A REALIZED count fluctuates around this by the binomial spread of the
   * per-person home-line hash; the cap is on the expectation, which is the
   * quantity that is population-invariant. */
  double maxUsersPerLine = 0.0;

  // Mean tenure of ONE link of a line, in days.
  double tenureDays = 0.0;

  double rowShare = 0.0;

  /* Which pool this is. Two pools sharing a domain would invert the same
   * uniform; see `publicEndpoints::kTerminalPoolDomain`. */
  std::uint64_t poolDomain = 0;
};

/* How many lines a roster of `people` supports. `peoplePerLine` is the MEAN
 * users per endpoint; the Pareto weights spread the realized counts around it.
 * `minLines` keeps small worlds — every gate leg is one — from rounding the
 * population out of existence, which would leave the pool inert exactly where
 * it is being measured. */
[[nodiscard]] inline std::size_t lineCountFor(std::uint32_t people,
                                              double peoplePerLine,
                                              std::size_t minLines) noexcept {
  if (people == 0 || !(peoplePerLine > 0.0)) {
    return 0;
  }
  const auto sized = static_cast<std::size_t>(
      std::llround(static_cast<double>(people) / peoplePerLine));
  return std::max(sized, minLines);
}

/* The exponent that holds the largest line's expected user count at
 * `maxUsersPerLine`, with weights read as `w^gamma`.
 *
 * `topShareFor` is MONOTONE INCREASING in gamma: at gamma 0 every line is
 * equal and the top share is `1/lineCount`; at gamma 1 the drawn law stands
 * unmodified. So the answer is a bisection on [0, 1] and it is a CAP, never a
 * pin — a pool whose drawn head already sits under the ceiling is returned
 * untouched at gamma 1 rather than inflated up to it.
 *
 * WHEN THE MEAN ITSELF EXCEEDS THE CEILING THERE IS NO ANSWER, and gamma 0 is
 * the closest a reach law can come. That is a real condition of a small world
 * rather than a bug: a roster of 900 divided over 14 lines puts 64 people on
 * the average line, so no exponent can hold any line under 64. The realized
 * head share is written back onto the pool so the condition is visible instead
 * of silent, exactly as `ReachModel::gamma` is.
 *
 * Iteration count is FIXED. A convergence-tested loop would make the work
 * data-dependent, and this runs beside a draw stream. */
[[nodiscard]] inline double flattenExponent(const std::vector<double> &weights,
                                            std::uint32_t people,
                                            double maxUsersPerLine) noexcept {
  if (weights.empty() || people == 0 || !(maxUsersPerLine > 0.0)) {
    return 1.0;
  }

  double maxWeight = 0.0;
  for (const auto w : weights) {
    if (w > 0.0 && std::isfinite(w)) {
      maxWeight = std::max(maxWeight, w);
    }
  }
  if (!(maxWeight > 0.0)) {
    return 1.0;
  }

  /* Scaled by the maximum so every base sits in (0, 1] and the sum stays far
   * from overflow at any exponent. */
  const auto topShareFor = [&](double gamma) {
    double sum = 0.0;
    for (const auto w : weights) {
      if (w > 0.0 && std::isfinite(w)) {
        sum += std::pow(w / maxWeight, gamma);
      }
    }
    return sum > 0.0 ? 1.0 / sum : 1.0;
  };

  const auto target = maxUsersPerLine / static_cast<double>(people);
  if (topShareFor(1.0) <= target) {
    return 1.0;
  }
  if (topShareFor(0.0) >= target) {
    return 0.0;
  }

  double low = 0.0;
  double high = 1.0;
  for (int i = 0; i < 60; ++i) {
    const auto mid = 0.5 * (low + high);
    if (topShareFor(mid) < target) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return 0.5 * (low + high);
}

/* Draws, in this exact order per line: ONE reach uniform, then the chain
 * (a phase uniform plus one per link), then whatever `mint` spends per link.
 * `mint(line, link)` returns the endpoint for that link and is where a caller
 * registers it in its own record table. */
template <typename Endpoint, typename MintLink>
[[nodiscard]] ::PhantomLedger::infra::PublicEndpointPool<Endpoint>
buildPublicPool(random::Rng &rng, time::Window window, PoolSpec spec,
                MintLink mint) {
  ::PhantomLedger::infra::PublicEndpointPool<Endpoint> out;
  if (spec.lineCount == 0 || !(spec.tenureDays > 0.0)) {
    return out;
  }

  out.rowShare = spec.rowShare;
  out.poolDomain = spec.poolDomain;
  out.lines.reserve(spec.lineCount);
  out.reachCdf.reserve(spec.lineCount);

  std::vector<double> weights;
  weights.reserve(spec.lineCount);

  for (std::size_t line = 0; line < spec.lineCount; ++line) {
    // Bounded Pareto by inverse transform. `nextDouble` is in [0,1), so the
    // residual is in (0,1] and the power is finite; the clamp bounds the tail.
    const auto residual = 1.0 - rng.nextDouble();
    const auto reach =
        std::min(kMaxReachMultiple, std::pow(residual, -1.0 / kReachTailIndex));
    weights.push_back(reach);

    const auto chain =
        timeline::sampleChain(rng, window.start, window.days,
                              [&](time::TimePoint) { return spec.tenureDays; });

    typename ::PhantomLedger::infra::PublicEndpointPool<Endpoint>::Line slice{
        .firstLink = static_cast<std::uint32_t>(out.links.size()),
        .linkCount = 0,
    };

    for (std::size_t k = 0; k < chain.size(); ++k) {
      const auto &link = chain[k];
      out.links.push_back(typename decltype(out)::Link{
          .endpoint = mint(line, k),
          .tenure =
              ::PhantomLedger::infra::Tenure{
                  .firstEpoch = time::toEpochSeconds(link.firstSeen),
                  .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
              },
      });
      ++slice.linkCount;
    }

    out.lines.push_back(slice);
  }

  /* Solved AFTER the loop above, so it reads only weights that are already
   * drawn and cannot reorder or add a uniform. */
  out.reachGamma = flattenExponent(weights, spec.people, spec.maxUsersPerLine);

  /* The identity exponent is skipped rather than applied, so a pool whose head
   * already sits under its ceiling is bit-identical to one built with no
   * ceiling at all. That is what lets the cap be disarmed in a gate without
   * the disarm itself moving a weight. */
  double total = 0.0;
  for (auto &w : weights) {
    if (out.reachGamma != 1.0) {
      w = std::pow(w, out.reachGamma);
    }
    total += w;
  }
  if (!(total > 0.0)) {
    return {};
  }

  double running = 0.0;
  for (const auto w : weights) {
    running += w;
    out.reachCdf.push_back(running / total);
    out.maxLineShare = std::max(out.maxLineShare, w / total);
  }
  // Guard the inversion against the last partial sum landing below 1.0.
  out.reachCdf.back() = 1.0;

  return out;
}

} // namespace PhantomLedger::synth::infra::publics
