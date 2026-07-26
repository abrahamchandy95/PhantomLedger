#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::government {

struct Population {
  std::uint32_t count = 0;
  const entity::behavior::Assignment *personas = nullptr;
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;

  // H2 step 2a (single age axis, authority U-7): the personas pack's
  // birth-date carrier. The SSA deposit cohort derives from the REAL
  // birth day-of-month per the SSA payment schedule (1-10 / 11-20 /
  // 21-31 -> Wednesdays 2/3/4), replacing the retired blake2b
  // syntheticBirthDay — the deposit day now agrees with the exported
  // PII Dob.
  const std::vector<time::CalendarDate> *birthDates = nullptr;

  // H2 step 2b: the persona timelines (Pack::timelines). Retirement
  // recipient selection keys on RETIRED-BY-WINDOW-END (personaAt),
  // with deposits beginning at each person's claiming date. H3: the
  // timeline also carries the DEATH date — deposits end there.
  const std::vector<synth::personas::timeline::Timeline> *timelines = nullptr;
};

struct Recipient {
  entity::PersonId person{};
  entity::Key account{};
  double amount = 0.0;
  int ssaCohort = 0;
  // H2 step 2b: deposits before this point are skipped — the window
  // start for static programs (disability), the person's claiming
  // date for timeline-selected retirement.
  time::TimePoint onset{};
  // H3: deposits at or after this point are skipped — the person's
  // death (benefits die with the beneficiary; survivor benefits are a
  // registered upgrade). Sentinel TimePoint{} = no bound (hand-built
  // recipients in tests).
  time::TimePoint end{};
};

namespace detail {

[[nodiscard]] inline bool hasAccount(const Population &pop,
                                     entity::PersonId pid) noexcept {
  const auto &own = *pop.ownership;
  return own.byPersonOffset[pid - 1] != own.byPersonOffset[pid];
}

[[nodiscard]] inline entity::Key primaryAccount(const Population &pop,
                                                entity::PersonId pid) noexcept {
  const auto idx = pop.ownership->primaryIndex(pid);
  return pop.accounts->records[idx].id;
}

[[nodiscard]] inline double sampleAmount(random::Rng &rng, double median,
                                         double sigma, double floor) {
  const auto raw =
      probability::distributions::lognormalByMedian(rng, median, sigma);
  return primitives::utils::floorAndRound(raw, floor);
}

[[nodiscard]] inline int birthDayOf(const Population &pop,
                                    entity::PersonId pid) {
  if (pop.birthDates == nullptr || pop.birthDates->size() < pop.count) {
    throw std::invalid_argument(
        "government::select requires the birth-date carrier "
        "(Pack::birthDates, H2 step 2a)");
  }
  return static_cast<int>((*pop.birthDates)[pid - 1].day);
}

[[nodiscard]] inline const synth::personas::timeline::Timeline &
timelineOf(const Population &pop, entity::PersonId pid) {
  if (pop.timelines == nullptr || pop.timelines->size() < pop.count) {
    throw std::invalid_argument(
        "government::select requires the persona-timeline carrier "
        "(Pack::timelines, H2 step 2b)");
  }
  return (*pop.timelines)[pid - 1];
}

} // namespace detail

// Selection semantics (H2 step 2b):
//   byTimelineRetirement == false  — the static persona programs
//     (disability): `matches(seed persona)`, deposits from the window
//     start (Recipient.onset = window.start).
//   byTimelineRetirement == true   — retirement: eligible = RETIRED BY
//     WINDOW END per the timeline (seed retirees AND workers whose
//     claiming date lands in-window); deposits begin at the claiming
//     date (onset = max(window.start, tl.retirement)); the persona
//     filter is ignored. eligibleP still gates (the declared
//     never-claims share).
// H3: every recipient's deposits END at their death (Recipient.end =
// tl.death, both modes). Selection itself is unchanged — the
// eligibleP coin and the amount draw fire for the same people as
// before, so the rng stream is byte-identical; only emitted deposits
// after a death disappear.
template <class Terms, class PersonaFilter>
[[nodiscard]] std::vector<Recipient>
select(const Population &pop, random::Rng &rng, const Terms &terms,
       PersonaFilter matches, const time::Window &window,
       bool byTimelineRetirement) {
  std::vector<Recipient> out;
  out.reserve(pop.count / 4);

  for (entity::PersonId pid = 1; pid <= pop.count; ++pid) {
    if (!detail::hasAccount(pop, pid)) {
      continue;
    }

    time::TimePoint onset = window.start;
    if (byTimelineRetirement) {
      const auto &tl = detail::timelineOf(pop, pid);
      if (!(tl.retirement < window.endExcl())) {
        continue;
      }
      onset = std::max(window.start, tl.retirement);
    } else {
      const auto persona = pop.personas->byPerson[pid - 1];
      if (!matches(persona)) {
        continue;
      }
    }

    if (!rng.coin(terms.eligibleP)) {
      continue;
    }

    out.push_back(Recipient{
        pid,
        detail::primaryAccount(pop, pid),
        detail::sampleAmount(rng, terms.median, terms.sigma, terms.floor),
        time::ssaCohort(detail::birthDayOf(pop, pid)),
        onset,
        detail::timelineOf(pop, pid).death,
    });
  }

  std::sort(out.begin(), out.end(),
            [](const auto &a, const auto &b) { return a.person < b.person; });
  return out;
}

} // namespace PhantomLedger::transfers::government
