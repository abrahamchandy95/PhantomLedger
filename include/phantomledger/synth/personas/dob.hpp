#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// macro-history-v1 H2 step 2a: THE SINGLE AGE AXIS (authority U-7).
//
// Birth dates move OFF the shared sequential PII stream onto the
// isolated {"dob", personId} lane (world-seed factory), into the
// compact carrier `Pack::birthDates`. One age per person feeds
// EVERYTHING age-derived: the PII roster renders its Dob FROM this
// carrier (synth/pii/make.hpp), the SSA deposit cohort derives from
// the REAL birth day-of-month (transfers/channels/government/
// recipients.hpp; the blake2b syntheticBirthDay is RETIRED), and the
// H2 persona timeline (step 2b) derives transitions from it.
//
// The draw SHAPE is the retired in-stream draw exactly —
// pii::sampleDob's persona age band at THE PERSON'S ANCHOR plus a
// 0-364 day offset — only the rng lane moved.
//
// H3 part 3c-ii (authority U-8 addendum): the anchor is SIM START for
// the seed roster and the JOIN DATE for the join cohort (`joinDays`,
// Pack::joinDays) — a 2015 joiner draws 2015-appropriate ages. This
// CLOSES the declared JOINER AGE AXIS ERROR; seed-roster draws are
// byte-identical (same lane, same anchor as before).
namespace PhantomLedger::synth::personas {

[[nodiscard]] inline time::CalendarDate
birthDateFor(const random::RngFactory &factory, entity::PersonId person,
             ::PhantomLedger::personas::Type type, time::TimePoint anchor) {
  auto rng = factory.rng({"dob", std::to_string(person)});
  const auto d = pii::sampleDob(rng, type, anchor);
  return time::CalendarDate{
      .year = static_cast<int>(d.year),
      .month = static_cast<unsigned>(d.month),
      .day = static_cast<unsigned>(d.day),
  };
}

/// Every person's birth date. `joinDays` (Pack::joinDays; optional —
/// empty means everyone anchors at sim start, the pre-3c-ii shape)
/// moves each joiner's age anchor to their join date.
[[nodiscard]] inline std::vector<time::CalendarDate>
birthDates(std::uint64_t worldSeed, time::TimePoint simStart,
           const entity::behavior::Assignment &assignment,
           std::span<const std::uint32_t> joinDays = {}) {
  const random::RngFactory factory{worldSeed};
  const bool anchored = joinDays.size() == assignment.byPerson.size();
  std::vector<time::CalendarDate> out;
  out.reserve(assignment.byPerson.size());
  for (std::size_t idx = 0; idx < assignment.byPerson.size(); ++idx) {
    const auto anchor =
        anchored ? simStart + time::Days{static_cast<int>(joinDays[idx])}
                 : simStart;
    out.push_back(birthDateFor(factory,
                               static_cast<entity::PersonId>(idx + 1),
                               assignment.byPerson[idx], anchor));
  }
  return out;
}

} // namespace PhantomLedger::synth::personas
