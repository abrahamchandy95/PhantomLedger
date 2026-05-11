#pragma once

#include "phantomledger/activity/spending/simulator/state.hpp"
#include "phantomledger/activity/spending/spenders/prepared.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

namespace PhantomLedger::spending::simulator {

namespace detail {

// Default cycle period (days) for personas whose payday sequence is
// empty or contains a single observation. 14 reflects US biweekly
// scheduling, the modal pay frequency per BLS Current Employment
// Statistics (CES) tables -- ~36% of private-sector employees
// (https://www.bls.gov/opub/btn/volume-3/how-frequently-do-private-businesses-pay-workers.htm).
inline constexpr std::uint16_t kDefaultCycleDays = 14;

[[nodiscard]] inline std::uint16_t
inferCycleDays(std::span<const std::uint32_t> paydays) noexcept {
  // Use the first observed gap as the cycle estimate. For US payroll
  // calendars this is exact for weekly/biweekly schedules and a good
  // approximation for monthly/semi-monthly (which have small calendar
  // drift between consecutive months). For populations with mixed
  // schedules this would warrant the median gap; in practice each
  // person's schedule is fixed by their employer and the first gap
  // is the right signal.
  if (paydays.size() < 2) {
    return kDefaultCycleDays;
  }
  const std::uint32_t gap = paydays[1] - paydays[0];
  if (gap == 0) {
    return kDefaultCycleDays;
  }
  constexpr std::uint32_t kMax = std::numeric_limits<std::uint16_t>::max();
  return static_cast<std::uint16_t>(std::min(gap, kMax));
}

} // namespace detail

// Compute the initial `daysSincePayday` value for a single person
// such that, after the day-driver's per-tick bump-and-maybe-reset
// sequence, the spender's emission-time `daysSincePayday` reflects
// realistic steady-state cycle position rather than an arbitrary
// warm-up bias.
//
// Method: deterministic backward projection from the observed first
// in-window payday and the inferred cycle period.
//
// Let:
//   f = paydays.front()           (first observed sim-window payday day)
//   T = inferCycleDays(paydays)   (inferred payroll cadence in days)
//   d_emit(0) = (T - f) mod T     (steady-state days-since-payday at
//                                  day-0 emission time)
//
// The day-driver performs `bump; reset_if_payday; emit` each tick, so
// to land on d_emit(0) at emission time the pre-tick state must be
// d_emit(0) - 1 on non-payday days (the bump adds one), and may be
// any value on a payday-on-day-0 (the reset overwrites). We return
// max(0, d_emit(0) - 1) to satisfy both cases uniformly.
//
// References:
//   * Law, "Simulation Modeling and Analysis" (5th ed, 2014) §9.5.2,
//     "Methods for the initial-transient problem"
//   * Ross, "Introduction to Probability Models" (12th ed, 2019) §7.3,
//     "renewal theory and stationary equilibrium" -- the inspection
//     paradox: for a renewal process with period T the stationary
//     distribution of "time since last event" is Uniform(0,T)
//   * Welch (1983), "The Statistical Analysis of Simulation Results"
//
// For populations whose payday sequence is empty (e.g., income modeled
// outside the day-driver's payday channel), we return T/2, the
// stationary mean of Uniform(0, T) under the default cadence. This
// matches the textbook "use the steady-state mean when no per-unit
// information is available" recommendation (Law §9.5.3).
[[nodiscard]] inline std::uint16_t
warmStartDaysSincePaydayFor(std::span<const std::uint32_t> paydays) noexcept {
  const std::uint16_t cycle = detail::inferCycleDays(paydays);

  if (paydays.empty()) {
    return static_cast<std::uint16_t>(cycle / 2);
  }

  const std::uint32_t firstPay = paydays.front();
  const std::uint32_t modFirst = firstPay % cycle;
  const std::uint32_t dEmit = (cycle - modFirst) % cycle;

  return dEmit == 0 ? std::uint16_t{0} : static_cast<std::uint16_t>(dEmit - 1);
}

// Apply warm-start initialization to every spender in a prepared
// run. Writes through `state.setDaysSincePayday` so the per-person
// values are addressed by `Spender::personIndex`, which is the same
// index the day-driver and liquidity multiplier read.
//
// Call once, after RunState construction and before the day loop
// begins. Subsequent day-driver bumps and resets operate on these
// initial values exactly as they did before.
inline void applyWarmStartDaysSincePayday(
    RunState &state,
    std::span<const spenders::PreparedSpender> spenders) noexcept {
  for (const auto &prepared : spenders) {
    state.setDaysSincePayday(prepared.spender.personIndex,
                             warmStartDaysSincePaydayFor(prepared.paydays));
  }
}

} // namespace PhantomLedger::spending::simulator
