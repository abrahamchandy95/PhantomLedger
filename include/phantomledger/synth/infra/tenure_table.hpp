#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <array>
#include <cstdint>

/*
  How long a session endpoint stays with one person, by year. The era axis for
  access infrastructure — the device/IP counterpart of the dated US EMV
  terminal mix that makes `use_chip` causal.

  CLASS S (dated exogenous series), CALIBRATION UNCITED.

  The SHAPE is sourced, in that the direction and the location of the turning
  point are well established in trade reporting on US consumer replacement
  cycles: long replacement in the desktop-PC and feature-phone era, a minimum
  around 2012-2014 when two-year carrier subsidy contracts were the dominant
  US purchase channel, then steady lengthening as those contracts disappeared
  and handsets became more durable and expensive.

  THE ANCHOR NUMBERS BELOW ARE NOT TRANSCRIBED FROM A NAMED SERIES. They are a
  declared choice consistent with that shape, recorded as UNCITED rather than
  dressed in a citation this file cannot support. Anyone replacing them with a
  named series (CIRP, Kantar, or a carrier upgrade-rate disclosure) should
  re-pin the goldens in that arc and promote this block to CITED.

  DO NOT read a benchmark claim off device tenure until that happens. What IS
  defensible today: tenure is finite, it varies by era in the documented
  direction, and every routed session falls inside the endpoint's own
  interval.

  IPs are treated separately and much shorter. A residential address is a DHCP
  lease against an ISP pool, not a purchased object, so it churns on a scale of
  months regardless of era. The era axis for IP tenure is deliberately NOT
  modelled — dial-up per-session addressing, sticky broadband DHCP and mobile
  CGNAT pull in different directions and the net effect is not something this
  file can defend. Registered as follow-up, not silently era-flat.
 */

namespace PhantomLedger::synth::infra::tenure {

struct Anchor {
  int year;
  double months;
};

// Mean tenure of ONE device line, in months. Piecewise-linear between
// anchors, clamped flat outside them.
inline constexpr std::array<Anchor, 7> kDeviceAnchors{{
    {1991, 60.0}, // desktop-PC / early-mobile era: replacement is rare
    {2000, 48.0},
    {2007, 30.0}, // smartphones arrive
    {2012, 22.0}, // MINIMUM: two-year US carrier subsidy contracts
    {2016, 26.0}, // contracts unbundle, handset prices rise
    {2019, 33.0},
    {2022, 40.0}, // durable, expensive handsets held longer
}};

// One declared flat mean; see the header note on why IP tenure carries
// no era axis.
inline constexpr double kIpTenureMonths = 4.0;

[[nodiscard]] constexpr double deviceTenureMonths(int year) noexcept {
  const auto &a = kDeviceAnchors;
  if (year <= a.front().year) {
    return a.front().months;
  }
  if (year >= a.back().year) {
    return a.back().months;
  }
  for (std::size_t i = 1; i < a.size(); ++i) {
    if (year <= a[i].year) {
      const auto &lo = a[i - 1];
      const auto &hi = a[i];
      const auto span = static_cast<double>(hi.year - lo.year);
      const auto t = static_cast<double>(year - lo.year) / span;
      return lo.months + t * (hi.months - lo.months);
    }
  }
  return a.back().months;
}

// Mean tenure in DAYS at the calendar year containing `tp`. 30.4375 =
// 365.25/12, so a "month" here is the average Gregorian month and the
// anchors above stay readable in the unit trade reporting uses.
[[nodiscard]] inline double deviceTenureDays(time::TimePoint tp) {
  return deviceTenureMonths(time::toCalendarDate(tp).year) * 30.4375;
}

[[nodiscard]] inline double ipTenureDays() noexcept {
  return kIpTenureMonths * 30.4375;
}

/* ATTACKER INFRASTRUCTURE.
 *
 * Fraud infrastructure churns on a MUCH shorter clock than a customer's, for
 * adversarial rather than economic reasons: an endpoint is worked until it is
 * burned — declined, fingerprinted, blocklisted — and then replaced. On the
 * consumer replacement cycle attacker endpoints would outlive the campaigns
 * that use them, which is backwards. Both values are ERA-FLAT: the burn clock
 * is a property of the defence, not of the decade.
 *
 * BOTH MUST STAY ON THE SCALE OF A CAMPAIGN, not far below it. An operator's
 * endpoint tenure divided into the campaign length IS the number of endpoints
 * its case load is spread over, and cases-per-endpoint is precisely the
 * quantity the graph signal is made of. Tenure far shorter than the campaign
 * produces a busy inventory of single-use endpoints — the ghost-endpoint
 * defect rebranded.
 *
 * The genuinely non-reused, high-churn component of real attacker address
 * usage is modelled as the residential-proxy branch in the fraud planner,
 * where it belongs: those addresses are real consumer lines, not attacker
 * inventory.
 *
 * CLASS S, CALIBRATION UNCITED, like the customer table above. A named series
 * on carding-infrastructure lifetime would replace these and re-pin. */
inline constexpr double kAttackerDeviceTenureDays = 120.0;
inline constexpr double kAttackerIpTenureDays = 90.0;

[[nodiscard]] inline double attackerDeviceTenureDays() noexcept {
  return kAttackerDeviceTenureDays;
}

[[nodiscard]] inline double attackerIpTenureDays() noexcept {
  return kAttackerIpTenureDays;
}

/* PUBLIC INFRASTRUCTURE — endpoints held by an operator rather than by a
 * customer: a library or hotel terminal, a carrier's public address.
 *
 * Both are LONGER than their consumer counterparts and both are ERA-FLAT, for
 * the same reason: they are capital an institution refreshes on a budget cycle
 * rather than objects a consumer replaces on a fashion-and-subsidy cycle. A
 * terminal outliving several generations of the handsets around it is the
 * defensible direction; the magnitudes are not transcribed from any series.
 *
 * BOTH ARE CLASS S, CALIBRATION UNCITED, exactly like the two attacker
 * constants above. They set how many distinct exported identities one physical
 * public endpoint fragments into over a long window, which is the quantity a
 * fan-out measurement is made of, so anyone pinning them to a named refresh
 * series should re-pin the goldens in that arc and promote this block. */
inline constexpr double kPublicTerminalTenureMonths = 60.0;
inline constexpr double kCarrierNatTenureMonths = 18.0;

[[nodiscard]] inline double publicTerminalTenureDays() noexcept {
  return kPublicTerminalTenureMonths * 30.4375;
}

[[nodiscard]] inline double carrierNatTenureDays() noexcept {
  return kCarrierNatTenureMonths * 30.4375;
}

} // namespace PhantomLedger::synth::infra::tenure
