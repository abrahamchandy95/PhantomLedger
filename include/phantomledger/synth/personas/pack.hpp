#pragma once

#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::personas {

struct Pack {
  entity::behavior::Assignment assignment;
  entity::behavior::Table table;

  // H3 part 3c-ii (macro-history-v1): the JOIN-COHORT carrier — each
  // person's join-day offset from window start (PersonId-1 indexed;
  // 0 = member from the start). Sized against the EMBEDDED BEA
  // population series and drawn on the isolated {"join-cohort",
  // personId} lanes (synth/personas/join.hpp). Filled FIRST at the
  // entities stage: the dob and timeline carriers below anchor
  // joiners at their JOIN DATE through it, and
  // join_cohort::membershipOf derives the exporters' membership view
  // [joinTs, closeTs) from it.
  std::vector<std::uint32_t> joinDays;

  // H2 step 2a (macro-history-v1): the SINGLE AGE AXIS — per-person
  // birth dates (PersonId-1 indexed) drawn on the isolated
  // {"dob", personId} lanes (synth/personas/dob.hpp). The pack rides
  // the blueprint into the transfer fold, so everything age-derived
  // (PII rendering, SSA deposit cohorts, the persona timeline)
  // reads ONE age per person. Filled by the entities stage
  // (buildPersonas) and by the blueprint's standalone fallback pack.
  // Since H3 3c-ii the ages anchor at sim start for the seed roster
  // and at the JOIN DATE for the join cohort.
  std::vector<time::CalendarDate> birthDates;

  // H2 step 2b: the persona TIMELINE per person (PersonId-1 indexed),
  // derived from the seed assignment + birthDates on the isolated
  // {"persona-era", personId} lanes (timeline::deriveAll). Salary
  // selection/spans, SSA recipient selection/onset and revenue month
  // gating read personaAt through this carrier; the AML exporter's
  // end-of-window persona (step 2c) does too. H3: it also carries
  // death/male (the {"mortality"} lane), anchored — like the persona
  // clamps — at sim start for seeds and at the join date for joiners.
  std::vector<timeline::Timeline> timelines;
};

} // namespace PhantomLedger::synth::personas
