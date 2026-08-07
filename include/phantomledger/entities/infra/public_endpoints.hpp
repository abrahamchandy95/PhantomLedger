#pragma once

/*
  PUBLIC ENDPOINTS — the infrastructure a person PASSES THROUGH rather than
  HOLDS. A library or hotel terminal on the device side; a carrier-grade NAT
  address on the IP side.

  WHY THIS IS A SEPARATE POOL AND NOT ANOTHER ENTRY IN `byPerson`. The router's
  per-person pool answers "which of MY endpoints am I on right now", and its
  answer is a sticky index perturbed by `rng.coin(switchP)` toward a uniformly
  chosen concurrently-live alternative. That random walk has a UNIFORM
  stationary distribution over the live set, so any endpoint added to a
  person's pool immediately takes `1/liveCount` of their rows — a third to a
  half. That is the right law for a household device the person genuinely
  co-owns and a nonsensical one for a kiosk they use twice a year. Worse, an
  always-live extra entry takes `liveCount` from 1 to 2 for the 80% of people
  who hold a single line, which ADDS a coin to every routed row and moves the
  whole downstream draw stream.

  So the rule is: endpoints a person HOLDS live in `byPerson` with a tenure and
  an ownership edge; endpoints a person PASSES THROUGH live here, are chosen
  DRAW-FREE at a declared row share, and carry NO ownership edge — which is
  what they carry in reality too. A kiosk has no `Has_Device` row because no
  customer owns it.

  THE SHAPE IS CITED, THE LEVELS ARE DECLARED. Real endpoint fan-out is a
  smooth two-regime continuum with legitimate entities at every level, not a
  cliff:

    * Casado and Freedman, "Peering Through the Shroud: The Effect of Edge
      Opacity on IP-Based Client Identification", NSDI 2007 (~7M clients, 177M
      requests): 10% of NATs were traversed by 3 or more hosts against 50% of
      PROXIES, and blacklisting a proxy address hit more than 10 clients in
      over 17% of cases. Accessed 2026-08-07.
    * Richter, Wohlfart, Vallina-Rodriguez, Allman, Bush, Feldmann, Kreibich,
      Weaver, Paxson, "A Multi-perspective Analysis of Carrier-Grade NAT
      Deployment", IMC 2016: roughly 40% of ISPs deploy CGNAT; 64 subscribers
      per public IPv4 at a 1K port chunk and up to ~128 at 512 ports; 21% of
      CGNs use arbitrary pooling. Accessed 2026-08-07.

  Both are read for DIRECTION and for the ORDER OF MAGNITUDE of users per
  shared endpoint. Neither reports CARDS per endpoint, which is the quantity
  this generator needs and which no public source publishes, so every level
  below — the people-per-endpoint mean, the reach tail index, and the row share
  — is a DECLARED CHOICE and must be quoted as one.

  THE LAW IS ON USERS AND CARDS ARE EMERGENT. This is `merchant-selection`
  rule 1 one layer over: reach is not volume. Users per endpoint is
  population-invariant and has sources; cards per endpoint is the product of
  that and the cards-per-person law, which moved in this same round, so pinning
  it would pin a band to a superseded construction. Cards per endpoint is
  PRINTED, never banded.

  THE REACH TAIL IS THE POINT, NOT THE MEAN. A pool where every endpoint serves
  the same number of people would replace one cliff with another: a flat block
  of devices at fan-out N and nothing between it and the personal population.
  Reach weights are drawn from a bounded Pareto so the realized users-per-
  endpoint distribution spans tens to the declared ceiling, which is the
  continuum both sources describe.

  THE HEAD IS CAPPED IN USERS, NOT IN WEIGHT, and the difference is not
  cosmetic. A bound on the drawn weight bounds a RATIO, and the user count that
  ratio produces is `peoplePerLine * w / E[w]` — so the ratio bound of 40 this
  pool started with was a hidden ceiling of 1,008 users at a mean of 64, which
  no constant stated. Sizing the head that way put 17% (900 people) and 25%
  (1,800 people) of every card account in the world on ONE endpoint, against a
  largest real browser-fingerprint anonymity set of 5.27% of mobile and 0.077%
  of desktop populations (Gomez-Boix, Laperdrix and Baudry, "Hiding in the
  Crowd", WWW 2018, n = 2,067,942: the largest sets hold 13,241 mobile and
  1,394 desktop fingerprints; accessed 2026-08-07). See
  `synth/infra/public_pool.hpp` for the exponent that enforces the user
  ceiling.

  THAT ANONYMITY-SET FIGURE IS A COLLISION STATISTIC AND THIS POOL IS NOT
  MODELLING COLLISION — REGISTERED AS OUT OF SCOPE. A fingerprint shared by
  thousands of unrelated handsets is real and is far larger than anything here,
  but it is a DEVICE-MODEL class, not a linkage: `Transaction_Uses_Device` and
  `Has_Device` assert that one machine carried these rows, and a collision
  class asserts only that these machines look alike. Modelling collision as the
  node identity would turn `cf_Device` into a categorical demographic attribute
  and destroy the linkage signal this population exists to supply. If it is
  ever wanted it belongs as an ATTRIBUTE beside an identity id — which is what
  IEEE-CIS's `DeviceInfo` column is — and the device vertex has no such column
  today. What ships here is a SHARING model and must not be described as
  anything else.

  EVERY QUERY HERE IS DRAW-FREE, STATELESS AND POINT-IN-TIME CORRECT, for the
  same three reasons `Router::liveIpFor` is: a coin on the hot path would move
  every downstream amount and timestamp, hidden state would desynchronise the
  batch and windowed engines, and an endpoint may only be attributed to a row
  inside its own tenure.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/tenure.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace PhantomLedger::infra {

/* The `devices::Identity::ownerId` base for public terminals, mirroring
 * `kAttackerOwnerIdBase`. An internal discriminator only: every assigned
 * identity renders through one opaque fixed-width namespace, so neither this
 * value nor `OwnerType::publicTerminal` is observable downstream. */
inline constexpr std::uint64_t kPublicTerminalOwnerIdBase = 0x7E120000ULL;

namespace publicEndpoints {

/* Two domains, never one. The HOME domain fixes which endpoint a person is
 * local to; the USE domain decides whether a given row happens there. Sharing
 * bits between them would make "does this row use a terminal" a deterministic
 * function of "which terminal", so a terminal's user set and its traffic share
 * would be the same random variable. */
inline constexpr std::uint64_t kHomeDomain = 0x50'5542'4C00'0001ULL;
inline constexpr std::uint64_t kUseDomain = 0x50'5542'4C00'0002ULL;

/* And a THIRD axis, because two pools now exist. The domains above separate
 * "which endpoint" from "whether this row uses one" WITHIN a pool; this one
 * separates one pool from another. Without it the terminal pool and the
 * carrier-NAT pool would invert the same uniform, so the 2% of rows on a
 * terminal would be a strict subset of the 30% behind a carrier address and
 * the two populations' user sets would be rank-identical — a coupling neither
 * law claims and one that would make the smaller pool's traffic a deterministic
 * function of the larger one's. */
inline constexpr std::uint64_t kTerminalPoolDomain = 0x50'5542'4C00'0011ULL;
inline constexpr std::uint64_t kCarrierNatPoolDomain = 0x50'5542'4C00'0012ULL;

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

/* A stable uniform in [0,1) for an (a, b, domain) triple. Both words are mixed
 * before being combined, so (a,b) and (b,a) do not collide and neither does an
 * additive shift of either one. Same construction as
 * `market/commerce/affinity.hpp`, widened to 64-bit keys because a timestamp
 * is one of the inputs. */
[[nodiscard]] constexpr double unitFor(std::uint64_t a, std::uint64_t b,
                                       std::uint64_t domain) noexcept {
  const auto mixed =
      splitmix(domain ^ splitmix(a)) ^
      (splitmix(b + ::PhantomLedger::hashing::constants::fnv64_prime) *
       ::PhantomLedger::hashing::constants::golden64);
  return static_cast<double>(splitmix(mixed) >> 11U) * 0x1.0p-53;
}

} // namespace publicEndpoints

/* A pool of public endpoint LINES. A line is one physical terminal or one
 * carrier address; its `links` are that line's replacement chain, exactly as a
 * personal line's are, so a long window turns a terminal over without ever
 * leaving a person with nothing live.
 *
 * Flat storage with a (firstLink, linkCount) slice per line: the pool is built
 * once, read on the hot path and never mutated, so a vector of vectors would
 * buy nothing and cost an indirection per query. */
template <typename Endpoint> struct PublicEndpointPool {
  struct Link {
    Endpoint endpoint{};
    Tenure tenure{};
  };

  struct Line {
    std::uint32_t firstLink = 0;
    std::uint32_t linkCount = 0;
  };

  std::vector<Link> links;
  std::vector<Line> lines;

  /* PARALLEL to `lines`: the normalised cumulative reach weight, ascending,
   * with `back() == 1.0`. Inverting it with a per-person uniform is what makes
   * a person's local endpoint a stable, draw-free fact. */
  std::vector<double> reachCdf;

  /* DECLARED CHOICE: the share of a person's rows that happen on their local
   * public endpoint rather than on an endpoint they hold. */
  double rowShare = 0.0;

  /* The solved reach flattening exponent and the head share it achieved, both
   * carried so a gate can PRINT them. `reachGamma` at 0 means the users
   * ceiling was unreachable and the pool fell back to a flat reach — a real
   * condition of a roster small enough that the MEAN line already exceeds the
   * ceiling, not a bug. `maxLineShare * people` is the head's expected user
   * count and is the quantity the ceiling is stated in. */
  double reachGamma = 1.0;
  double maxLineShare = 0.0;

  /* Which pool this is, mixed into both hash domains. See
   * `kTerminalPoolDomain` for why two pools may not share a uniform. */
  std::uint64_t poolDomain = 0;

  [[nodiscard]] bool active() const noexcept {
    return !lines.empty() && reachCdf.size() == lines.size() && rowShare > 0.0;
  }

  /* The line this person is local to. A pure function of the person id, so a
   * terminal's user set is a property of the world and not of the order rows
   * were emitted in. */
  [[nodiscard]] std::size_t
  homeLineFor(entity::PersonId person) const noexcept {
    const auto u =
        publicEndpoints::unitFor(static_cast<std::uint64_t>(person), 0U,
                                 publicEndpoints::kHomeDomain ^ poolDomain);
    for (std::size_t i = 0; i + 1 < reachCdf.size(); ++i) {
      if (u < reachCdf[i]) {
        return i;
      }
    }
    return reachCdf.empty() ? 0U : reachCdf.size() - 1;
  }

  /* The link of `line` that was live at `ts`. Links tile the window, so at
   * most one qualifies. */
  [[nodiscard]] std::optional<Endpoint> liveOn(std::size_t line,
                                               std::int64_t ts) const noexcept {
    if (line >= lines.size()) {
      return std::nullopt;
    }
    const auto &slice = lines[line];
    for (std::uint32_t k = 0; k < slice.linkCount; ++k) {
      const auto &link = links[static_cast<std::size_t>(slice.firstLink) + k];
      if (link.tenure.contains(ts)) {
        return link.endpoint;
      }
    }
    return std::nullopt;
  }

  /* The link of `line` that covers the WHOLE half-open interval, or nothing.
   *
   * A case is not a row. A row happens at an instant and `liveOn` is the right
   * query for it; a compromise case spans hours and every one of its events
   * must be attributable to the SAME endpoint, or the exported session
   * contradicts itself mid-case. Links tile without overlapping, so requiring
   * one link to contain both ends is exactly "one endpoint for the whole
   * case" — the same rule `AttackerInfra`'s covering span enforces. */
  [[nodiscard]] std::optional<Endpoint>
  coveringOn(std::size_t line, std::int64_t fromTs,
             std::int64_t toTsExcl) const noexcept {
    if (line >= lines.size() || toTsExcl <= fromTs) {
      return std::nullopt;
    }
    const auto &slice = lines[line];
    for (std::uint32_t k = 0; k < slice.linkCount; ++k) {
      const auto &link = links[static_cast<std::size_t>(slice.firstLink) + k];
      if (link.tenure.contains(fromTs) && link.tenure.contains(toTsExcl - 1)) {
        return link.endpoint;
      }
    }
    return std::nullopt;
  }

  /* Does this row happen on a public endpoint, and if so which one?
   *
   * `rowSalt` must be a world-derived, row-stable word — the row's timestamp
   * is the obvious choice. It must NOT be a counter or an emission ordinal:
   * this has to be a pure function of world state so a short run stays a
   * byte-identical prefix of a longer one.
   *
   * Spends nothing and writes nothing back. */
  [[nodiscard]] std::optional<Endpoint>
  resolve(entity::PersonId person, std::int64_t ts,
          std::uint64_t rowSalt) const noexcept {
    if (!active()) {
      return std::nullopt;
    }
    const auto use =
        publicEndpoints::unitFor(static_cast<std::uint64_t>(person), rowSalt,
                                 publicEndpoints::kUseDomain ^ poolDomain);
    if (use >= rowShare) {
      return std::nullopt;
    }
    return liveOn(homeLineFor(person), ts);
  }
};

using TerminalPool = PublicEndpointPool<devices::Identity>;
using CarrierNatPool = PublicEndpointPool<network::Ipv4>;

} // namespace PhantomLedger::infra
