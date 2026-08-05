#pragma once
//
// phantomledger/synth/infra/attackers.hpp
//
// GENERATION of the exogenous fraud-infrastructure pool the card and
// ATO rails transact from. The queryable shape, the model it encodes,
// and why lookups against it must stay draw-free all live in
// `entities/infra/attackers.hpp`; this header owns the SAMPLING.

#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>

namespace PhantomLedger::synth::infra::attackers {

using Output = ::PhantomLedger::infra::AttackerInfra;

struct AssignmentRules {
  // POPULATION SIZING IS PER PERSON-YEAR, not per person.
  //
  // Compromise volume scales with both the customer base and the length
  // of the window, and so must the attacker population: a campaign is
  // months long, so a 30-year corpus is worked by many successive
  // operators rather than by one immortal cohort. Sizing on population
  // alone would have made a long run's sharing degree grow without
  // bound, which is the mirror image of the defect being fixed.
  //
  // The constant is calibrated against the realized victims-per-endpoint
  // distribution, which `tests/test_card_endpoint_graph.cpp` measures
  // and bands. Raising it thins sharing; lowering it concentrates it.
  double operatorsPerTenKPersonYears = 30.0;

  // COVERAGE FLOOR, in mean CONCURRENT campaigns.
  //
  // The population term above is the realistic scaling axis, but it is
  // also the thing that makes a small world uncoverable: at 900 people
  // it plans a handful of operators across a multi-year window, so most
  // case dates would have NO campaign running and the planner would fall
  // back to victim-endpoint attribution on most cases — measuring a
  // regime production never enters.
  //
  // This floor is expressed in concurrency rather than count precisely so
  // that it is WINDOW-INDEPENDENT: a longer run needs proportionally more
  // successive campaigns to hold the same number live at once. Mean live
  // ~= count x meanCampaignDays / windowDays, so the count implied by the
  // floor scales with window length and the concurrency does not.
  //
  // At 7 concurrent campaigns the chance that a given instant has none is
  // well under a percent, and the residual is absorbed by the
  // victim-endpoint branch and BANDED by the gate rather than hidden.
  double minConcurrentOperators = 7.0;

  // Effective mean campaign length in days, used ONLY to convert the
  // concurrency floor above into an operator count.
  //
  // MEASURED, NOT DERIVED, and the distinction cost a round of the gate
  // going red. The analytic mean of the length distribution is
  // `campaignMedianDays * exp(sigma^2/2)` = 110 * 1.570 = 172.7, and
  // ~169 after the max clamp. Using that number under-delivered the
  // concurrency floor by 15-19% at both gate shapes — the realized mean
  // CLIPPED span implies an effective length near 137-144, not 169.
  //
  // The gate prints the realized clipped span beside the realized
  // concurrency for exactly this reason, and its band brackets the
  // OUTCOME. Anyone who changes `campaignMedianDays` or `campaignSigma`
  // must re-read that print and move this constant with them; the band
  // will say so if they do not.
  double effectiveCampaignDays = 140.0;

  // Concurrent DEVICE LINES per operator — how many machines the
  // campaign runs at once. Each line is a replacement chain, so the
  // inventory over a campaign is larger than the line count.
  //
  // KEPT DELIBERATELY LEAN. Every extra line divides the operator's case
  // load across more endpoints, and it is CASES PER ENDPOINT — not
  // endpoints per operator — that the graph signal is made of. A rich
  // inventory would look busier and carry less.
  double secondDeviceLineP = 0.45;
  double thirdDeviceLineP = 0.15;

  // Concurrent IP LINES. Slightly richer than devices, but only
  // slightly, for the same reason. The high-churn, low-reuse component
  // of a real operator's address usage is modelled where it actually
  // belongs — the residential-proxy branch in the planner — rather than
  // by shredding the sticky infrastructure that carries the signal.
  double secondIpLineP = 0.55;
  double thirdIpLineP = 0.25;
  double fourthIpLineP = 0.0;

  // Campaign length. Lognormal by median — most campaigns are a few
  // months, a thin tail runs for years.
  double campaignMedianDays = 110.0;
  double campaignSigma = 0.95;
  std::int32_t campaignMinDays = 14;
  std::int32_t campaignMaxDays = 1'095;

  // CASE-LOAD WEIGHT: Pareto tail index. Real carding output is
  // extremely unequal — a small number of operators account for most
  // attempted volume — and a flat weight would produce a Poisson-flat
  // degree distribution in which every endpoint looks alike. Smaller
  // alpha = heavier tail.
  double weightAlpha = 1.35;
  double weightMax = 80.0;

  // Defensive ceiling on the operator count so a pathological
  // window/population combination cannot allocate without bound. Not a
  // modelling parameter; if a real config ever reaches it the sizing
  // rule above is what should change.
  std::uint32_t maxOperators = 250'000;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] {
      v::between("operatorsPerTenKPersonYears", operatorsPerTenKPersonYears,
                 0.0, 1.0e6);
    });
    r.check([&] {
      v::between("minConcurrentOperators", minConcurrentOperators, 0.0, 1.0e5);
    });
    r.check([&] {
      v::between("effectiveCampaignDays", effectiveCampaignDays, 1.0, 1.0e5);
    });
    r.check([&] { v::between("secondDeviceLineP", secondDeviceLineP, 0.0, 1.0); });
    r.check([&] { v::between("thirdDeviceLineP", thirdDeviceLineP, 0.0, 1.0); });
    r.check([&] { v::between("secondIpLineP", secondIpLineP, 0.0, 1.0); });
    r.check([&] { v::between("thirdIpLineP", thirdIpLineP, 0.0, 1.0); });
    r.check([&] { v::between("fourthIpLineP", fourthIpLineP, 0.0, 1.0); });
    r.check([&] {
      v::between("campaignMedianDays", campaignMedianDays, 1.0, 1.0e5);
    });
    r.check([&] { v::between("campaignSigma", campaignSigma, 0.0, 5.0); });
    r.check([&] { v::between("weightAlpha", weightAlpha, 0.2, 10.0); });
    r.check([&] { v::between("weightMax", weightMax, 1.0, 1.0e6); });
  }

  [[nodiscard]] Output build(random::Rng &rng, time::Window window,
                             std::uint32_t population) const;
};

} // namespace PhantomLedger::synth::infra::attackers
