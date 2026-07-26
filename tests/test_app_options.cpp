#include "phantomledger/app/options.hpp"

#include "phantomledger/synth/econ/catalog.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace pl = ::PhantomLedger;

int main() {
  static_assert(std::string_view{pl::app::kDefaultPgConninfo} ==
                "dbname=phantomledger");

  const pl::app::RunOptions options;
  assert(options.pgConninfo == pl::app::kDefaultPgConninfo);
  assert(pl::app::pgConninfoWithOverride(options.pgConninfo, nullptr) ==
         pl::app::kDefaultPgConninfo);
  assert(pl::app::pgConninfoWithOverride(options.pgConninfo, "") ==
         pl::app::kDefaultPgConninfo);
  assert(pl::app::pgConninfoWithOverride(
             options.pgConninfo, "dbname=alternate") == "dbname=alternate");

  // ---------------------------------------------------------------
  // Era lock (macro-history-v1 H0.6): windowInsideEra is the pure
  // predicate cli::parse enforces for --usecase card-fraud (parse
  // itself exits on failure, so the boundary cases are pinned here).
  // Coverage bounds come from the REAL pinned series — since the H1
  // coverage extension that means 1990 through at least 2024 (the
  // last fully MEASURED year; the AWI's ~Oct N+1 publication lag is
  // the binding constraint), and appending a published year widens
  // the lock with no engine change.
  // ---------------------------------------------------------------
  const auto &era = pl::synth::econ::macroSeries();
  assert(era.firstYear() <= 1990 && era.lastYear() >= 2024);

  pl::app::RunOptions cf;
  cf.usecase = pl::app::UseCase::cardFraud;

  // Canonical card-fraud window [1991-01-01, 2020-01-01) is inside.
  cf.startDate = {1991, 1, 1};
  cf.days = 10592;
  assert(pl::app::lastSimulatedDay(cf).year == 2019);
  assert(pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // The artifact-endpoint variant (10651 days) ends 2020-02-28 —
  // still inside a coverage that includes 2020.
  cf.days = 10651;
  {
    const auto last = pl::app::lastSimulatedDay(cf);
    assert(last.year == 2020 && last.month == 2 && last.day == 28);
  }
  assert(pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // The DEFAULT start (2025-01-01) is STILL out of era: 2025 is not a
  // fully measured year (AWI 2025 publishes ~2026-10; the Oct-2025 CPI
  // and CPS were never collected). DELIBERATE TRIPWIRE: the refresh
  // round that appends the 2025 row makes the default card-fraud start
  // legal and must consciously flip this assertion.
  {
    pl::app::RunOptions def;
    def.usecase = pl::app::UseCase::cardFraud;
    assert(def.startDate.year == 2025);
    assert(!pl::app::windowInsideEra(def, era.firstYear(), era.lastYear()));
  }

  // The H1 coverage extension WIDENED the lock: an in-era start whose
  // window runs into 2021 is now inside (it was rejected when coverage
  // ended at 2020).
  cf.startDate = {2019, 1, 1};
  cf.days = 800;
  assert(pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // A window that starts in-era but runs past the coverage frontier
  // fails, whatever the frontier currently is.
  cf.startDate = {era.lastYear(), 1, 1};
  cf.days = 800;
  assert(!pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // A start before coverage fails, even by one day.
  cf.startDate = {1989, 12, 31};
  cf.days = 30;
  assert(!pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // Exact edges pass: first day of the first covered year, and a
  // window ending on the last day of the last covered year.
  cf.startDate = {era.firstYear(), 1, 1};
  cf.days = 1;
  assert(pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));
  cf.startDate = {era.lastYear(), 12, 31};
  cf.days = 1;
  assert(pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));
  cf.days = 2; // one day over the edge
  assert(!pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // Degenerate/absurd windows are rejected, not overflowed.
  cf.startDate = {1991, 1, 1};
  cf.days = 0;
  assert(!pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));
  cf.days = 10'000'000; // ~27k years: clamped arithmetic, clean reject
  assert(!pl::app::windowInsideEra(cf, era.firstYear(), era.lastYear()));

  // ---------------------------------------------------------------
  // Frozen-era declared notice (macro-history-v1 H1 step 2b):
  // frozenEraNotice is the pure helper main.cpp prints ONCE to stderr
  // when the window touches years outside coverage — the H1 nominal
  // scales FREEZE there (never extrapolate). Pinned like the lock:
  // real bounds, boundary cases in-process.
  // ---------------------------------------------------------------
  {
    // The default standard run (2025-01-01, 365d) touches the frozen
    // frontier: the notice fires and names the coverage bounds.
    pl::app::RunOptions def;
    const auto notice =
        pl::app::frozenEraNotice(def, era.firstYear(), era.lastYear());
    assert(notice.has_value());
    assert(notice->find(std::to_string(era.firstYear())) !=
           std::string::npos);
    assert(notice->find(std::to_string(era.lastYear())) != std::string::npos);
    assert(notice->find("FREEZE") != std::string::npos);

    // Fully measured windows are silent — including the canonical
    // card-fraud window and both coverage edges.
    pl::app::RunOptions inEra;
    inEra.startDate = {1991, 1, 1};
    inEra.days = 10592;
    assert(!pl::app::frozenEraNotice(inEra, era.firstYear(), era.lastYear())
                .has_value());
    inEra.startDate = {era.firstYear(), 1, 1};
    inEra.days = 1;
    assert(!pl::app::frozenEraNotice(inEra, era.firstYear(), era.lastYear())
                .has_value());
    inEra.startDate = {era.lastYear(), 12, 31};
    inEra.days = 1;
    assert(!pl::app::frozenEraNotice(inEra, era.firstYear(), era.lastYear())
                .has_value());

    // One day past the frontier fires it.
    inEra.days = 2;
    assert(pl::app::frozenEraNotice(inEra, era.firstYear(), era.lastYear())
               .has_value());

    // A pre-coverage start fires it too (backward freeze).
    pl::app::RunOptions pre;
    pre.startDate = {1985, 1, 1};
    pre.days = 30;
    const auto preNotice =
        pl::app::frozenEraNotice(pre, era.firstYear(), era.lastYear());
    assert(preNotice.has_value());
    assert(preNotice->find(std::to_string(era.firstYear())) !=
           std::string::npos);

    // Degenerate windows are the lock's problem, not the notice's.
    pl::app::RunOptions zero;
    zero.days = 0;
    assert(!pl::app::frozenEraNotice(zero, era.firstYear(), era.lastYear())
                .has_value());
  }

  return 0;
}
