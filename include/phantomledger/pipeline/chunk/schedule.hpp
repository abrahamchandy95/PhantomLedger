#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::pipeline::chunk {

struct Strategy {
  int monthsPerChunk = 1;
  int lookaheadDays = 6;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::positive("chunk.monthsPerChunk", monthsPerChunk); });
    r.check([&] { v::between("chunk.lookaheadDays", lookaheadDays, 0, 60); });
  }
};

struct Span {
  std::uint32_t index = 0;
  time::Window activeWindow{};
  time::TimePoint lookaheadBoundExcl{};

  std::uint32_t firstDayIndex = 0;

  [[nodiscard]] bool first() const noexcept { return index == 0; }

  [[nodiscard]] time::HalfOpenInterval lookahead() const noexcept {
    return {activeWindow.endExcl(), lookaheadBoundExcl};
  }

  [[nodiscard]] int lookaheadDayCount() const noexcept {
    return static_cast<int>(std::chrono::duration_cast<time::Days>(
                                lookaheadBoundExcl - activeWindow.endExcl())
                                .count());
  }
};

class Schedule {
public:
  // Partition `runWindow` per `strategy`. Throws (see file comment).
  [[nodiscard]] static Schedule partition(time::Window runWindow,
                                          const Strategy &strategy);

  // Single span covering the entire window, zero lookahead.
  [[nodiscard]] static Schedule unpartitioned(time::Window runWindow);

  [[nodiscard]] std::size_t size() const noexcept { return spans_.size(); }
  [[nodiscard]] bool empty() const noexcept { return spans_.empty(); }

  [[nodiscard]] const Span &operator[](std::size_t i) const noexcept {
    return spans_[i];
  }

  [[nodiscard]] auto begin() const noexcept { return spans_.begin(); }
  [[nodiscard]] auto end() const noexcept { return spans_.end(); }

  [[nodiscard]] const time::Window &totalWindow() const noexcept {
    return runWindow_;
  }

  [[nodiscard]] bool isLast(const Span &s) const noexcept {
    return s.index + 1 == spans_.size();
  }

private:
  time::Window runWindow_{};
  std::vector<Span> spans_;
};

} // namespace PhantomLedger::pipeline::chunk
