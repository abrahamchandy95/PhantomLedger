#pragma once
//
// phantomledger/synth/infra/tenure_table.hpp
//
// HOW LONG A SESSION ENDPOINT STAYS WITH ONE PERSON, BY YEAR.
//
// This is the era axis for access infrastructure — the device/IP
// counterpart of the dated US EMV terminal mix that already makes
// `use_chip` causal, and of the macro-history-v1 per-year dollar scales.
// Before it existed, a person held the SAME device for an entire 30-year
// window (`secondDeviceP = 0.20`, so 80% of people had exactly one), and
// device replacement was not modelled at all.
//
// ============================================ CLASS AND CITATION STATUS
// CLASS S (dated exogenous series), **CALIBRATION UNCITED**.
//
// The SHAPE is sourced in the sense that the direction and the location
// of the turning point are well established in trade reporting on US
// consumer replacement cycles: long replacement in the desktop-PC and
// feature-phone era, a minimum around 2012-2014 when two-year carrier
// subsidy contracts were the dominant US purchase channel, then a
// steady lengthening as those contracts disappeared and handsets became
// more durable and more expensive.
//
// The ANCHOR NUMBERS BELOW ARE NOT TRANSCRIBED FROM A NAMED SERIES.
// They are a declared modelling choice consistent with that shape, and
// they are recorded as UNCITED rather than dressed in a citation this
// file cannot support — the same treatment the authority document gives
// the late-payment rate. Anyone replacing them with a named series
// (CIRP, Kantar, or a carrier upgrade-rate disclosure) should re-pin the
// goldens in that arc and promote this block to CITED.
//
// DO NOT read a benchmark claim off device tenure until that happens.
// What IS defensible today: tenure is finite, it varies by era in the
// documented direction, and every routed session falls inside the
// endpoint's own interval. That is the defect this table exists to fix.
// ======================================================================
//
// IPs are treated separately and much shorter. A residential address is
// a DHCP lease against an ISP pool, not a purchased object, so it churns
// on a scale of months regardless of era. The era axis for IP tenure is
// deliberately NOT modelled here (dial-up per-session addressing, sticky
// broadband DHCP and mobile CGNAT pull in different directions and the
// net effect is not something this file can defend); IP tenure is one
// declared flat mean with the same lognormal dispersion. Registered as
// follow-up, not silently era-flat.

#include "phantomledger/primitives/time/calendar.hpp"

#include <array>
#include <cstdint>

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

// ------------------------------------------- ATTACKER INFRASTRUCTURE
//
// Fraud infrastructure churns on a MUCH shorter clock than a
// customer's, and the reason is adversarial rather than economic: an
// endpoint is worked until it is burned — declined, fingerprinted,
// blocklisted — and then replaced. Modelling it on the consumer
// replacement cycle would have made attacker endpoints longer-lived
// than the campaigns that use them, which is backwards.
//
// The direction is what these anchor: both are far shorter than the
// customer equivalents, and both are ERA-FLAT (the burn clock is a
// property of the defence, not of the decade).
//
// AND BOTH ARE ON THE SCALE OF A CAMPAIGN, not far below it, which is a
// deliberate modelling decision rather than an accident of the numbers.
// An operator's endpoint tenure divided into the campaign length IS the
// number of endpoints its case load is spread over, and cases-per-
// endpoint is precisely the quantity the graph signal is made of. Tenure
// far shorter than the campaign would produce a busy inventory of
// single-use endpoints — a rebranding of the ghost-endpoint defect this
// arc exists to close.
//
// The high-churn, genuinely non-reused component of real attacker
// address usage IS modelled — as the residential-proxy branch in the
// fraud planner, where it belongs, because those addresses are real
// consumer lines rather than attacker inventory.
//
// CLASS S, CALIBRATION UNCITED, exactly like the customer table above.
// A named series on carding-infrastructure lifetime would replace these
// and re-pin.
inline constexpr double kAttackerDeviceTenureDays = 120.0;
inline constexpr double kAttackerIpTenureDays = 90.0;

[[nodiscard]] inline double attackerDeviceTenureDays() noexcept {
  return kAttackerDeviceTenureDays;
}

[[nodiscard]] inline double attackerIpTenureDays() noexcept {
  return kAttackerIpTenureDays;
}

} // namespace PhantomLedger::synth::infra::tenure
