#include "phantomledger/pipeline/chunk/schedule.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <stdexcept>

namespace PhantomLedger::pipeline::chunk {

namespace {

[[nodiscard]] int wholeDaysBetween(time::TimePoint a, time::TimePoint b) {
  // Chrono comparison detects sub-day remainders: duration_cast
  // truncates, so diff == days holds only for whole-day gaps.
  const auto diff = b - a;
  const auto days = std::chrono::duration_cast<time::Days>(diff);
  if (diff != days) {
    throw std::invalid_argument(
        "chunk::Schedule: window boundaries must be midnight-aligned");
  }
  return static_cast<int>(days.count());
}

} // namespace

Schedule Schedule::unpartitioned(time::Window runWindow) {
  Schedule out;
  out.runWindow_ = runWindow;
  if (runWindow.days <= 0) {
    return out;
  }

  Span span;
  span.index = 0;
  span.activeWindow = runWindow;
  span.lookaheadBoundExcl = runWindow.endExcl();
  span.firstDayIndex = 0;
  out.spans_.push_back(span);

  return out;
}

Schedule Schedule::partition(time::Window runWindow, const Strategy &strategy) {
  {
    primitives::validate::Report report;
    strategy.validate(report);
    report.throwIfFailed();
  }

  Schedule out;
  out.runWindow_ = runWindow;
  if (runWindow.days <= 0) {
    return out;
  }

  const auto runEnd = runWindow.endExcl();
  const auto anchor = time::monthStart(runWindow.start);

  std::vector<time::TimePoint> bounds;
  bounds.push_back(runWindow.start);

  auto futureBounds =
      std::views::iota(1) | std::views::transform([&](int k) {
        return time::addMonths(anchor, k * strategy.monthsPerChunk);
      }) |
      std::views::take_while([runEnd](auto b) { return b < runEnd; }) |
      std::views::filter(
          [runStart = runWindow.start](auto b) { return b > runStart; });

  std::ranges::copy(futureBounds, std::back_inserter(bounds));
  bounds.push_back(runEnd);

  out.spans_.reserve(bounds.size() - 1);
  for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
    Span span;
    span.index = static_cast<std::uint32_t>(i);
    span.activeWindow.start = bounds[i];
    span.activeWindow.days = wholeDaysBetween(bounds[i], bounds[i + 1]);
    span.firstDayIndex = static_cast<std::uint32_t>(
        wholeDaysBetween(runWindow.start, bounds[i]));

    const auto la = time::addDays(bounds[i + 1], strategy.lookaheadDays);
    span.lookaheadBoundExcl = (la < runEnd) ? la : runEnd;

    out.spans_.push_back(span);
  }
  return out;
}

} // namespace PhantomLedger::pipeline::chunk
