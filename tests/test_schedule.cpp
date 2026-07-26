#include "phantomledger/pipeline/chunk/schedule.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace PhantomLedger;
using pipeline::chunk::Schedule;
using pipeline::chunk::Strategy;

namespace {

time::TimePoint at(int y, unsigned m, unsigned d) {
  return time::makeTime({y, m, d});
}

void expectPartition(const Schedule &s, time::Window run) {
  assert(!s.empty());
  assert(s[0].activeWindow.start == run.start);
  assert(s[0].first());
  int total = 0;
  for (std::size_t i = 0; i < s.size(); ++i) {
    total += s[i].activeWindow.days;
    assert(s[i].activeWindow.days > 0);
    assert(s[i].index == i);
    if (i + 1 < s.size()) {
      assert(s[i].activeWindow.endExcl() == s[i + 1].activeWindow.start);
      // interior boundaries are month starts
      assert(time::monthStart(s[i + 1].activeWindow.start) ==
             s[i + 1].activeWindow.start);
    }
    assert(s[i].lookaheadBoundExcl >= s[i].activeWindow.endExcl());
    assert(s[i].lookaheadBoundExcl <= run.endExcl());
    assert(s[i].lookahead().start == s[i].activeWindow.endExcl());
    assert(s[i].lookahead().endExcl == s[i].lookaheadBoundExcl);
  }
  assert(total == run.days);
  assert(s[s.size() - 1].activeWindow.endExcl() == run.endExcl());
  assert(s.isLast(s[s.size() - 1]));
}

} // namespace

int main() {
  // 1. Mid-month start, monthly chunks, 400 days.
  {
    time::Window run{at(2025, 1, 15), 400};
    const auto s =
        Schedule::partition(run, {.monthsPerChunk = 1, .lookaheadDays = 6});
    expectPartition(s, run);
    assert(s[0].activeWindow.days == 17); // Jan 15 -> Feb 1
    assert(s[1].activeWindow.start == at(2025, 2, 1));
    assert(s[1].activeWindow.days == 28); // Feb 2025
    assert(s[0].firstDayIndex == 0);
    assert(s[1].firstDayIndex == 17);
    assert(s[0].lookaheadBoundExcl == at(2025, 2, 7)); // +6 days
    assert(s[0].lookaheadDayCount() == 6);
    const auto &lastSpan = s[s.size() - 1];
    assert(lastSpan.lookaheadBoundExcl == run.endExcl()); // clipped
    assert(lastSpan.lookaheadDayCount() == 0);
  }

  // 2. Month-start run, quarterly chunks: 365d ends exactly on a
  //    quarter boundary (2025 is not a leap year), so 4 spans.
  {
    time::Window run{at(2025, 1, 1), 365};
    const auto s =
        Schedule::partition(run, {.monthsPerChunk = 3, .lookaheadDays = 6});
    expectPartition(s, run);
    assert(s.size() == 4);
    assert(s[0].activeWindow.days == 90); // Jan+Feb+Mar 2025
    assert(s[1].activeWindow.start == at(2025, 4, 1));
    assert(s[3].activeWindow.start == at(2025, 10, 1));
    assert(s[3].activeWindow.days == 92); // Oct 1 -> Jan 1
  }

  // 3. Chunk larger than the run collapses to one span.
  {
    time::Window run{at(2025, 1, 1), 400};
    const auto s =
        Schedule::partition(run, {.monthsPerChunk = 24, .lookaheadDays = 6});
    expectPartition(s, run);
    assert(s.size() == 1);
    assert(s[0].activeWindow.days == 400);
  }

  // 4. unpartitioned() bridge: one span, zero lookahead.
  {
    time::Window run{at(2025, 1, 1), 365};
    const auto s = Schedule::unpartitioned(run);
    assert(s.size() == 1);
    assert(s[0].activeWindow.days == 365);
    assert(s[0].lookaheadDayCount() == 0);
    expectPartition(s, run);
  }

  // 5. Empty window -> empty schedule.
  {
    const auto s = Schedule::partition({at(2025, 1, 1), 0}, {});
    assert(s.empty());
    assert(Schedule::unpartitioned({at(2025, 1, 1), 0}).empty());
  }

  // 6. Requested card-fraud horizon: 10,592 days covers the complete
  //    1991-through-2019 calendar interval [1991-01-01, 2020-01-01).
  //    This is 348 monthly spans with no stub. It is intentionally not
  //    described as IBM's exact released-artifact interval, whose observed
  //    final transaction is 2020-02-28.
  {
    time::Window run{at(1991, 1, 1), 10592};
    const auto s = Schedule::partition(run, {});
    expectPartition(s, run);
    assert(run.endExcl() == at(2020, 1, 1));
    assert(s.size() == 348);
  }

  // 7. Invalid strategy surfaces through the validate framework.
  {
    bool threw = false;
    try {
      (void)Schedule::partition({at(2025, 1, 1), 30},
                                {.monthsPerChunk = 0, .lookaheadDays = 6});
    } catch (const std::invalid_argument &e) {
      threw = true;
      assert(std::strstr(e.what(), "monthsPerChunk") != nullptr);
    }
    assert(threw);
  }

  // 8. Non-midnight run start is rejected, not silently truncated.
  {
    bool threw = false;
    try {
      time::Window run{time::makeTime({2025, 1, 15}, {12, 0, 0}), 60};
      (void)Schedule::partition(run, {});
    } catch (const std::invalid_argument &e) {
      threw = true;
      assert(std::strstr(e.what(), "midnight-aligned") != nullptr);
    }
    assert(threw);
  }

  std::puts("chunk schedule: all assertions passed");
  return 0;
}
