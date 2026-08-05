#include "phantomledger/activity/spending/actors/day.hpp"

#include "phantomledger/primitives/random/distributions/gamma.hpp"

#include <algorithm>

namespace PhantomLedger::activity::spending::actors {

Day buildDay(time::TimePoint windowStart, double dayShockShape,
             random::Rng &rng, std::uint32_t dayIndex) {
  Day day{};
  day.dayIndex = dayIndex;
  day.start = time::addDays(windowStart, static_cast<int>(dayIndex));

  // `time::weekday` is Mon=0..Sun=6 (calendar.cpp converts off the ISO
  // Mon=1..Sun=7 encoding), which is what makes `>= 5` select Sat/Sun. The
  // convention matters more than it looks: an off-by-one would move the
  // weekend to Fri/Sat and silently mis-weight `calculateExploreP`'s 1.25x
  // weekend multiplier across the whole corpus.
  //
  // `isWeekend` is `weekday(tp) >= 5`, so this delegates rather than
  // restating the threshold in a second place.
  day.weekday = static_cast<std::uint8_t>(time::weekday(day.start));
  day.isWeekend = time::isWeekend(day.start);

  // Gamma(shape, scale = 1/shape) has mean = 1.0, so the shock is
  // multiplicative noise centered at 1. Lower shape => fatter tail.
  const double safeShape = std::max(1e-6, dayShockShape);
  day.shock =
      probability::distributions::gamma(rng, safeShape, 1.0 / safeShape);
  return day;
}

} // namespace PhantomLedger::activity::spending::actors
