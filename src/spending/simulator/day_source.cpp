#include "phantomledger/spending/simulator/day_source.hpp"

#include "phantomledger/math/counts.hpp"

namespace PhantomLedger::spending::simulator {

DaySource::DaySource()
    : DaySource(Variation{}, math::seasonal::kDefaultConfig) {}

DaySource::DaySource(Variation variation)
    : DaySource(variation, math::seasonal::kDefaultConfig) {}

DaySource::DaySource(Variation variation, math::seasonal::Config seasonal)
    : variation_(variation), seasonal_(seasonal) {}

actors::DayFrame DaySource::build(const market::Bounds &bounds,
                                  random::Rng &rng,
                                  std::uint32_t dayIndex) const {
  const auto day =
      actors::buildDay(bounds.startDate, variation_.shockShape, rng, dayIndex);

  return actors::DayFrame{
      .day = day,
      .weekdayMult = math::counts::weekdayMultiplier(day.start),
      .seasonalMult = math::seasonal::dailyMultiplier(day.start, seasonal_),
  };
}

} // namespace PhantomLedger::spending::simulator
