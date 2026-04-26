#include "phantomledger/spending/actors/day.hpp"

#include "phantomledger/probability/distributions/gamma.hpp"

#include <algorithm>

namespace PhantomLedger::spending::actors {

Day buildDay(time::TimePoint windowStart, double dayShockShape,
             random::Rng &rng, std::uint32_t dayIndex) {
  Day day{};
  day.dayIndex = dayIndex;
  day.start = time::addDays(windowStart, static_cast<int>(dayIndex));

  // TODO(integration): time::weekday(TimePoint) -> int 0-6 (Mon-Sun).
  // If your calendar primitive exposes a different name (e.g.
  // time::dayOfWeek) substitute here.
  const int wd = time::weekday(day.start);
  day.weekday = static_cast<std::uint8_t>(wd);
  day.isWeekend = wd >= 5;

  // Gamma(shape, scale = 1/shape) has mean = 1.0, so the shock is
  // multiplicative noise centered at 1. Lower shape => fatter tail.
  const double safeShape = std::max(1e-6, dayShockShape);
  day.shock =
      probability::distributions::gamma(rng, safeShape, 1.0 / safeShape);
  return day;
}

} // namespace PhantomLedger::spending::actors
