#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::pii {

struct IdentityContext {
  const PoolSet *pools = nullptr;
  time::TimePoint simStart{};
  LocaleMix localeMix{};

  // geo-causal-v1: the raw run seed. Home geography is placed per
  // HOUSEHOLD from the EMBEDDED catalogue (synth::geo::geography()) on
  // named RNG lanes derived from this seed — the same seed the transfer
  // stage feeds the family household partition, so the home grouping is
  // byte-identical to the family graph's households (coresidents share
  // one address) while never perturbing the shared entity stream.
  std::uint64_t worldSeed = 0;

  // H3 part 3c-ii (macro-history-v1): the simulation window length in
  // days. buildPersonas derives the JOIN COHORT (Pack::joinDays) from
  // (worldSeed, simStart, windowDays) — BEA-sized joiner count, one
  // {"join-cohort", personId} draw per joiner — and anchors joiners'
  // ages at their join date. 0 (the default) = no join cohort: the
  // pre-3c-ii shape direct entity-stage callers (unit harnesses) keep.
  // The production pipeline always sets it from its window.
  int windowDays = 0;

  // H2 step 2a (macro-history-v1): the single-age-axis carrier
  // (Pack::birthDates, drawn on the isolated {"dob", personId} lanes).
  // The Generator RENDERS each record's Dob from this carrier instead
  // of drawing it mid-record on the shared entity stream — one age per
  // person across PII, SSA cohorts and the persona timeline. Set by
  // the entities stage (buildPii) from the personas pack; must cover
  // every person in the assignment.
  const std::vector<time::CalendarDate> *birthDates = nullptr;
};

} // namespace PhantomLedger::synth::pii
