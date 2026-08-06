#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::pii {

/* The inputs `pii::Generator` needs to render one person's identity. */
struct IdentityContext {
  const PoolSet *pools = nullptr;
  time::TimePoint simStart{};
  LocaleMix localeMix{};

  /* The raw run seed. Home geography is placed per HOUSEHOLD from the embedded
   * catalogue (`synth::geo::geography()`) on named RNG lanes derived from this
   * seed — the SAME seed the transfer stage feeds the family household
   * partition, so the home grouping is byte-identical to the family graph's
   * households (coresidents share one address) while never perturbing the
   * shared entity stream. */
  std::uint64_t worldSeed = 0;

  /* Simulation window length in days. `buildPersonas` derives the JOIN COHORT
   * (`Pack::joinDays`) from (worldSeed, simStart, windowDays) — BEA-sized
   * joiner count, one {"join-cohort", personId} draw per joiner — and anchors
   * joiners' ages at their join date. 0 (the default) means NO join cohort,
   * which is the shape direct entity-stage callers (unit harnesses) keep; the
   * production pipeline always sets it from its window. */
  int windowDays = 0;

  /* The single-age-axis carrier (`Pack::birthDates`, drawn on the isolated
   * {"dob", personId} lanes). The Generator RENDERS each record's Dob from
   * this carrier instead of drawing it mid-record on the shared entity stream,
   * giving one age per person across PII, SSA cohorts and the persona
   * timeline. Set by the entities stage (`buildPii`) from the personas pack;
   * MUST cover every person in the assignment. */
  const std::vector<time::CalendarDate> *birthDates = nullptr;
};

} // namespace PhantomLedger::synth::pii
