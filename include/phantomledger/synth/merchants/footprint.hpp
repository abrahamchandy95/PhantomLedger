#pragma once
//
// phantomledger/synth/merchants/footprint.hpp
//
// Category-conditioned merchant Footprint (geo-causal-v1, G1c). A
// Merchant record is an acceptance location; its Footprint governs which
// customers can plausibly reach it during causal selection (G2):
// everyday physical categories are reached LOCALLY, billed services reach
// NATIONALLY/regionally, ecommerce is ONLINE (geography-free), and general
// retail is MIXED. Drawn on an isolated per-merchant RNG lane so it never
// perturbs the shared entity stream.
//
// The per-category mix below is PROVISIONAL [Likely — owner citation /
// calibration pass]; it is UNEXPORTED until the merchant exporter round,
// so it moves no golden yet. Do NOT treat these as calibrated targets.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

namespace PhantomLedger::synth::merchants {

// A merchant's commercial reach, conditioned on its category. One uniform
// draw per merchant on the isolated footprint lane (taken even for
// single-outcome categories so the lane advances uniformly).
[[nodiscard]] inline entity::merchant::Footprint
footprintFor(::PhantomLedger::merchants::Category category, random::Rng &rng) {
  using F = entity::merchant::Footprint;
  using C = ::PhantomLedger::merchants::Category;

  const double u = rng.nextDouble();

  switch (category) {
  case C::grocery: // everyday, walk-in — overwhelmingly local
    return u < 0.90 ? F::localOutlet : F::regionalOutlet;
  case C::fuel: // pumped where you drive — local
    return u < 0.95 ? F::localOutlet : F::regionalOutlet;
  case C::restaurant: // local, some chains, a little delivery-only
    return u < 0.90   ? F::localOutlet
           : u < 0.98 ? F::regionalOutlet
                      : F::online;
  case C::pharmacy: // local counter, some regional chains + mail-order
    return u < 0.85   ? F::localOutlet
           : u < 0.95 ? F::regionalOutlet
                      : F::online;
  case C::ecommerce: // online by definition — geography-free
    return F::online;
  case C::utilities: // billed nationwide; some regional providers
    return u < 0.70 ? F::nationalService : F::regionalOutlet;
  case C::telecom: // national carriers dominate
    return u < 0.85 ? F::nationalService : F::regionalOutlet;
  case C::insurance: // national carriers, some regional
    return u < 0.80 ? F::nationalService : F::regionalOutlet;
  case C::retailOther: // genuinely mixed: boutiques, big-box, online
    return u < 0.45   ? F::localOutlet
           : u < 0.80 ? F::regionalOutlet
                      : F::online;
  case C::education: // regional campus mostly; some local; some online
    return u < 0.70   ? F::regionalOutlet
           : u < 0.90 ? F::localOutlet
                      : F::online;
  }
  return F::localOutlet;
}

} // namespace PhantomLedger::synth::merchants
