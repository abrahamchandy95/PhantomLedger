// tests/test_session_point_in_time.cpp
//
// THE SESSION ENDPOINT A ROW CARRIES MUST HAVE EXISTED AT THAT ROW'S
// TIMESTAMP. Hard invariant, not a band (device-ip-lifecycle, 2026-07-27).
//
// WHAT THIS GATE EXISTS FOR. `synth::infra::devices` and `ips` record a
// usage interval for every (person, endpoint) pair, and the standard
// exporter publishes those intervals as HAS_USED / HAS_IP. The access
// router used to choose endpoints WITHOUT a timestamp, so it answered
// from the person's whole-window pool. Measured before the fix, at 2,000
// people over 730 days: 53.51% of routed device sessions and 52.09% of
// routed IP sessions fell OUTSIDE the endpoint's own interval — 26.96%
// of device sessions were attributed to a device before its own
// first_seen. `public.transactions.device_id` and `HAS_USED` therefore
// contradicted each other on the same row, roughly half the time.
//
// WHY ZERO AND NOT A BAND. Nothing here is sampled. Personal endpoints
// are generated as TILING replacement chains, so at every in-window
// instant at least one endpoint per line is live; a routed endpoint
// outside its own tenure is a logic error, not an unlucky draw. A band
// would be a licence for the defect to come back at low frequency.
//
// PART B GATES THIS GATE'S OWN COVERAGE. A world where nobody ever
// replaces a device would satisfy part A vacuously — one endpoint, one
// interval covering everything, nothing to get wrong. So part B pins
// that turnover actually happens, and that it happens in the direction
// the era table declares: replacement is FASTER in 2019 than in 1991.
// If part B ever reads flat, part A has stopped being a test.

#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/devices.hpp"
#include "phantomledger/synth/infra/ips.hpp"

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace entities = ::PhantomLedger::pipeline::stages::entities;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

constexpr std::uint64_t kSeed = 20260727ULL;
constexpr std::uint32_t kPopulation = 800;

struct Leg {
  std::uint64_t routed = 0;
  std::uint64_t deviceOutside = 0;
  std::uint64_t ipOutside = 0;
  double meanDevicesPerPerson = 0.0;
  double meanIpsPerPerson = 0.0;
};

// Route one session per person per week across the window and check each
// answer against the endpoint's own recorded interval.
[[nodiscard]] Leg runLeg(int startYear, std::int32_t days) {
  auto rng = pl::random::Rng::fromSeed(kSeed);

  const pl::time::Window window{
      .start = pl::time::makeTime({startYear, 1, 1}),
      .days = days,
  };

  const auto people = entities::buildPeople(rng, kPopulation, {});
  const std::unordered_map<std::uint32_t, pl::synth::infra::RingPlan> noRings;

  const auto devices = pl::synth::infra::devices::AssignmentRules{}.build(
      rng, window, people.roster, noRings);
  const auto ips = pl::synth::infra::ips::AssignmentRules{}.build(
      rng, window, people.roster, noRings);

  std::unordered_map<pl::entity::Key, pl::entity::PersonId> ownerOf;
  std::vector<pl::entity::Key> keyOf(people.roster.count + 1);
  for (pl::entity::PersonId p = 1; p <= people.roster.count; ++p) {
    const auto k = pl::entity::makeKey(pl::entity::Role::account,
                                       pl::entity::Bank::internal, p);
    ownerOf.emplace(k, p);
    keyOf[p] = k;
  }

  auto router = pl::infra::Router::build(pl::infra::RoutingRules{}, ownerOf,
                                         devices.byPerson, ips.byPerson,
                                         devices.tenureByPerson,
                                         ips.tenureByPerson);

  // The generator's own record, keyed the way the exporters read it.
  std::map<std::pair<pl::entity::PersonId, pl::devices::Identity>,
           pl::infra::Tenure>
      deviceSpan;
  for (const auto &u : devices.usages) {
    deviceSpan.emplace(
        std::make_pair(u.personId, u.deviceId),
        pl::infra::Tenure{pl::time::toEpochSeconds(u.firstSeen),
                          pl::time::toEpochSeconds(u.lastSeen) + 86'400});
  }
  std::map<std::pair<pl::entity::PersonId, pl::network::Ipv4>, pl::infra::Tenure>
      ipSpan;
  for (const auto &u : ips.usages) {
    ipSpan.emplace(
        std::make_pair(u.personId, u.ipAddress),
        pl::infra::Tenure{pl::time::toEpochSeconds(u.firstSeen),
                          pl::time::toEpochSeconds(u.lastSeen) + 86'400});
  }

  Leg leg;
  std::uint64_t deviceCount = 0;
  std::uint64_t ipCount = 0;
  for (pl::entity::PersonId p = 1; p <= people.roster.count; ++p) {
    const auto d = devices.byPerson.find(p);
    if (d != devices.byPerson.end()) {
      deviceCount += d->second.size();
    }
    const auto i = ips.byPerson.find(p);
    if (i != ips.byPerson.end()) {
      ipCount += i->second.size();
    }
  }
  leg.meanDevicesPerPerson =
      static_cast<double>(deviceCount) / static_cast<double>(kPopulation);
  leg.meanIpsPerPerson =
      static_cast<double>(ipCount) / static_cast<double>(kPopulation);

  const auto windowStart = pl::time::toEpochSeconds(window.start);
  for (std::int32_t day = 0; day < days; day += 7) {
    const auto ts = windowStart + static_cast<std::int64_t>(day) * 86'400;
    for (pl::entity::PersonId p = 1; p <= people.roster.count; ++p) {
      const auto dev = router.routeDeviceFor(rng, p, ts);
      const auto ip = router.routeIpFor(rng, p, ts);
      // Tiling chains mean an in-window instant ALWAYS has a live
      // endpoint. A nullopt here is itself the defect.
      check(dev.has_value(), "router returns a device for an in-window ts");
      check(ip.has_value(), "router returns an IP for an in-window ts");
      if (!dev.has_value() || !ip.has_value()) {
        return leg;
      }
      ++leg.routed;

      const auto dit = deviceSpan.find(std::make_pair(p, *dev));
      if (dit == deviceSpan.end() || !dit->second.contains(ts)) {
        ++leg.deviceOutside;
      }
      const auto iit = ipSpan.find(std::make_pair(p, *ip));
      if (iit == ipSpan.end() || !iit->second.contains(ts)) {
        ++leg.ipOutside;
      }
    }
  }

  return leg;
}

} // namespace

int main() {
  std::printf("test_session_point_in_time: routed endpoints must be live at "
              "the row's timestamp\n");

  // Two eras, same seed and population, so the only difference is the
  // dated replacement rate.
  const auto leg91 = runLeg(1991, 1461);
  const auto leg19 = runLeg(2019, 1461);

  for (const auto &[name, leg] :
       {std::pair{"1991", leg91}, std::pair{"2019", leg19}}) {
    std::printf("  %s  routed=%llu  device-outside=%llu  ip-outside=%llu  "
                "devices/person=%.2f  ips/person=%.2f\n",
                name, static_cast<unsigned long long>(leg.routed),
                static_cast<unsigned long long>(leg.deviceOutside),
                static_cast<unsigned long long>(leg.ipOutside),
                leg.meanDevicesPerPerson, leg.meanIpsPerPerson);
  }

  // ---- PART A: the invariant. Zero, not a band. --------------------
  check(leg91.routed > 100'000 && leg19.routed > 100'000,
        "both legs routed a substantial number of sessions (precondition)");
  check(leg91.deviceOutside == 0,
        "1991: every routed device is live at the row timestamp");
  check(leg91.ipOutside == 0,
        "1991: every routed IP is live at the row timestamp");
  check(leg19.deviceOutside == 0,
        "2019: every routed device is live at the row timestamp");
  check(leg19.ipOutside == 0,
        "2019: every routed IP is live at the row timestamp");

  // ---- PART B: this gate's own coverage. ---------------------------
  // Turnover must exist, or part A is vacuous.
  check(leg91.meanDevicesPerPerson > 1.2,
        "devices turn over within a 4-year window, so part A has something "
        "to catch (1991)");
  check(leg19.meanDevicesPerPerson > 1.2,
        "devices turn over within a 4-year window (2019)");
  check(leg91.meanIpsPerPerson > 3.0,
        "IP leases turn over on a months scale, so part A has something to "
        "catch");

  // The era axis must MOVE, and move the declared way: replacement is
  // faster in 2019 (33-month mean) than in 1991 (60-month mean), so the
  // same window holds MORE devices per person in 2019.
  const double eraRatio = leg19.meanDevicesPerPerson / leg91.meanDevicesPerPerson;
  std::printf("  device turnover ratio 2019/1991 %.4f (declared means 33mo vs "
              "60mo -> expect > 1)\n",
              eraRatio);
  check(eraRatio > 1.15,
        "device replacement is FASTER in 2019 than 1991 (era axis is wired "
        "and points the declared way)");

  // IP tenure carries NO era axis by declaration (tenure_table.hpp), so
  // it must NOT move. Asserting the direction it must not move is the
  // standing law for an entering gradient.
  const double ipRatio = leg19.meanIpsPerPerson / leg91.meanIpsPerPerson;
  std::printf("  ip turnover ratio 2019/1991 %.4f (declared era-flat -> "
              "expect ~1)\n",
              ipRatio);
  check(ipRatio > 0.9 && ipRatio < 1.1,
        "IP tenure is era-FLAT as declared, so it must not drift with the "
        "device axis");

  if (g_failures != 0) {
    std::fprintf(stderr, "test_session_point_in_time: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("All session point-in-time gates passed.\n");
  return 0;
}
