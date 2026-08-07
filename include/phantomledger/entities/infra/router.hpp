#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/entities/infra/tenure.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::infra {

struct InfraAttribution {
  std::optional<devices::Identity> device;
  std::optional<network::Ipv4> ip;
};

struct RoutingRules {
  double switchP = 0.05;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("switchP", switchP, 0.0, 1.0); });
  }
};

class Router {
public:
  Router() = default;

  using DeviceTenures =
      std::unordered_map<entity::PersonId, std::vector<Tenure>>;
  using IpTenures = std::unordered_map<entity::PersonId, std::vector<Tenure>>;

  static Router
  build(RoutingRules rules,
        std::unordered_map<entity::Key, entity::PersonId> ownerOf,
        std::unordered_map<entity::PersonId, std::vector<devices::Identity>>
            devicesByPerson,
        std::unordered_map<entity::PersonId, std::vector<network::Ipv4>>
            ipsByPerson,
        DeviceTenures deviceTenures = {}, IpTenures ipTenures = {},
        /* The PASS-THROUGH pool, carried BY VALUE for the same reason every
         * pool above is: the Router owns its endpoint state outright, and a
         * pointer into the infra stage's output would add a lifetime coupling
         * to an object the gate harness copies twice. Default-constructed is
         * inactive, so a Router built without one routes exactly as before. */
        TerminalPool terminals = {},
        /* The IP-side companion, and the stronger of the two: a terminal moves
         * ~2% of rows, a carrier address ~30%. Wiring only the terminal pool
         * would close the device axis and leave the IP degree shortcut — which
         * measures at essentially the same strength — untouched. */
        CarrierNatPool carrierNat = {}) {
    Router r;
    r.rules_ = rules;
    r.ownerOf_ = std::move(ownerOf);
    r.devicesByPerson_ = std::move(devicesByPerson);
    r.ipsByPerson_ = std::move(ipsByPerson);
    r.deviceTenures_ = std::move(deviceTenures);
    r.ipTenures_ = std::move(ipTenures);
    r.terminals_ = std::move(terminals);
    r.carrierNat_ = std::move(carrierNat);
    r.currentDeviceIdx_.assign(poolBound(r.devicesByPerson_), kUnanchored);
    r.currentIpIdx_.assign(poolBound(r.ipsByPerson_), kUnanchored);
    return r;
  }

  [[nodiscard]] std::optional<entity::PersonId>
  ownerOf(const entity::Key &srcAcct) const {
    const auto it = ownerOf_.find(srcAcct);
    if (it == ownerOf_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  /// Route a device for a pre-resolved person, AS OF `ts`.
  [[nodiscard]] std::optional<devices::Identity>
  routeDeviceFor(random::Rng &rng, entity::PersonId person,
                 std::int64_t ts) const {
    return routeFromPool(rng, person, ts, devicesByPerson_, deviceTenures_,
                         currentDeviceIdx_);
  }

  /// Route an IP for a pre-resolved person, AS OF `ts`.
  [[nodiscard]] std::optional<network::Ipv4>
  routeIpFor(random::Rng &rng, entity::PersonId person, std::int64_t ts) const {
    return routeFromPool(rng, person, ts, ipsByPerson_, ipTenures_,
                         currentIpIdx_);
  }

  /// DRAW-FREE, STICKY-FREE point query: an IP that belonged to
  /// `person` at `ts`, choosing among concurrently-live addresses by
  /// `salt`. Returns nullopt when the person has no pool or nothing was
  /// live.
  ///
  /// THIS IS NOT AN ALTERNATIVE TO `routeIpFor` FOR SESSION ROUTING.
  /// It exists for the residential-proxy branch of the fraud planner
  /// (attacker-infra-2026-07), where a THIRD PARTY exits through this
  /// customer's home address. Two properties are load-bearing there and
  /// neither is available from `routeIpFor`:
  ///
  ///   * It must not advance `currentIpIdx_`. The proxied customer is
  ///     not transacting; perturbing their sticky index would make their
  ///     own later legitimate rows depend on whether an unrelated
  ///     attacker borrowed their address, and that dependence would
  ///     differ between the batch and windowed engines.
  ///   * It must not draw. The planner's per-plan RNG footprint is
  ///     pinned at four uniforms so this round's golden movement stays
  ///     confined to the device/IP columns.
  ///
  /// It is safe against the "a fixed slot-0 pick IS the label" trap for
  /// the same reason the pick is salted: the exported value is the
  /// customer's real address either way, and which of their concurrent
  /// lines it came from is not observable downstream.
  /// `endTsExcl` requires the address to have been held for the WHOLE
  /// half-open interval, for the same reason `AttackerInfra::ipAt` does:
  /// a compromise attributes ONE endpoint to every event in the case, so
  /// an address that changed hands mid-case would be attributed to a row
  /// after it was reassigned.
  [[nodiscard]] std::optional<network::Ipv4>
  liveIpFor(entity::PersonId person, std::int64_t ts, std::int64_t endTsExcl,
            std::uint64_t salt) const {
    return liveFromPool(person, ts, endTsExcl, salt, ipsByPerson_, ipTenures_);
  }

  /* The DEVICE-SIDE twin of `liveIpFor`, and the same three properties are
   * load-bearing for the same reasons.
   *
   *   * DRAW-FREE. The fraud planner's per-plan footprint is pinned at four
   *     uniforms; a coin here would change the lane's draw count and unpin
   *     every amount and timestamp downstream of it.
   *   * STICKY-FREE. It must not advance `currentDeviceIdx_`. The borrowed
   *     customer is not transacting, and perturbing their sticky index would
   *     make their own later legitimate rows depend on whether an attacker
   *     passed through — differently in the batch and windowed engines
   *     (attacker-infra rule 3).
   *   * POINT-IN-TIME CORRECT. `endTsExcl` demands the device was held for
   *     the WHOLE half-open case interval, because a compromise attributes
   *     ONE endpoint to every event in the case.
   *
   * IT IS NOT AN ALTERNATIVE TO `routeDeviceFor` FOR SESSION ROUTING.
   * `routeDeviceFor` answers "which of MY devices am I on right now",
   * anchors and succeeds a retired line positionally, and may spend a
   * switch coin. This answers "was this device SOMEONE ELSE's for the
   * whole of this interval", spends nothing, and writes nothing back. */
  [[nodiscard]] std::optional<devices::Identity>
  liveDeviceFor(entity::PersonId person, std::int64_t ts,
                std::int64_t endTsExcl, std::uint64_t salt) const {
    return liveFromPool(person, ts, endTsExcl, salt, devicesByPerson_,
                        deviceTenures_);
  }

  /* THE PASS-THROUGH ENDPOINT for a row, or nullopt when this row happens on
   * an endpoint the person holds.
   *
   * DRAW-FREE, STICKY-FREE, POINT-IN-TIME CORRECT — the same three properties
   * `liveDeviceFor` carries, and here they matter for an additional reason:
   * this is on the LEGITIMATE hot path, once per routed row. A coin here
   * would spend one uniform on every routed row in the corpus.
   *
   * `rowSalt` must be world-derived and row-stable — the row's timestamp — and
   * never an emission ordinal, or a short run stops being a byte-identical
   * prefix of a long one.
   *
   * A terminal is NOT one of the person's own endpoints and must never be
   * added to `devicesByPerson_`: an always-live extra entry there would take
   * `1/liveCount` of the person's rows by the switch walk's stationary
   * distribution AND would raise `liveCount` from 1 to 2 for the ~80% of
   * people holding a single line, adding a coin to every routed row. The full
   * argument is at the head of `entities/infra/public_endpoints.hpp`. */
  [[nodiscard]] std::optional<devices::Identity>
  publicDeviceFor(entity::PersonId person, std::int64_t ts,
                  std::uint64_t rowSalt) const {
    return terminals_.resolve(person, ts, rowSalt);
  }

  [[nodiscard]] bool hasPublicDevices() const noexcept {
    return terminals_.active();
  }

  /* The public terminal `person` is local to, held for a WHOLE case interval.
   *
   * DELIBERATELY IGNORES THE POOL'S ROW SHARE, and that is the difference from
   * `publicDeviceFor`. The row share answers "does this ordinary row happen at
   * a terminal", which is a per-row coin the pool owns. A compromise case is
   * not an ordinary row: the caller has already decided this case is operated
   * from a terminal, and the only question left is which one and whether it is
   * live throughout. Re-applying the row share here would silently multiply
   * the caller's declared share by 0.02 and the branch would look wired while
   * firing almost never — the exact shape of the pre-round `legitShared`
   * defect, where two independent suppressors multiplied to 0.0065%. */
  [[nodiscard]] std::optional<devices::Identity>
  publicDeviceForCase(entity::PersonId person, std::int64_t fromTs,
                      std::int64_t toTsExcl) const {
    if (!terminals_.active()) {
      return std::nullopt;
    }
    return terminals_.coveringOn(terminals_.homeLineFor(person), fromTs,
                                 toTsExcl);
  }

  /* The carrier-grade NAT address this row exits through, if any.
   *
   * DELIBERATELY IP-ONLY, and that is the difference from a terminal. A
   * terminal is a machine the person does not own, so its own address travels
   * with it and both legs move together. A carrier address is the person's own
   * handset behind their ISP's translator: the device is theirs and only the
   * address is shared. Overriding the device here would invent a machine the
   * subscriber does not have. */
  [[nodiscard]] std::optional<network::Ipv4>
  publicIpFor(entity::PersonId person, std::int64_t ts,
              std::uint64_t rowSalt) const {
    return carrierNat_.resolve(person, ts, rowSalt);
  }

  [[nodiscard]] bool hasPublicIps() const noexcept {
    return carrierNat_.active();
  }

  /// Legacy convenience: resolve owner and route both legs. Retained
  /// for callers that genuinely need both every time and benefit from
  /// the single hash lookup. Prefer the split API when possible.
  [[nodiscard]] InfraAttribution routeSource(random::Rng &rng,
                                             const entity::Key &srcAcct,
                                             std::int64_t ts) const {
    const auto person = ownerOf(srcAcct);
    if (!person.has_value()) {
      return {};
    }
    return {
        routeDeviceFor(rng, *person, ts),
        routeIpFor(rng, *person, ts),
    };
  }

private:
  // Sticky-index sentinel: the person has not routed yet.
  static constexpr std::size_t kUnanchored =
      std::numeric_limits<std::size_t>::max();

  // Smallest vector size that can be indexed by every person key in the
  // pool, so a successful pool lookup guarantees an in-bounds sticky slot.
  template <typename T>
  [[nodiscard]] static std::size_t
  poolBound(const std::unordered_map<entity::PersonId, std::vector<T>> &pool) {
    std::size_t bound = 0;
    for (const auto &[person, items] : pool) {
      bound =
          std::max<std::size_t>(bound, static_cast<std::size_t>(person) + 1);
    }
    return bound;
  }

  // POINT-IN-TIME ROUTING (device-ip-lifecycle, 2026-07-27).
  //
  // Endpoints have TENURES, and an endpoint may only be attributed to a
  // transaction inside its own. Before this, routing ignored the
  // intervals the generator had already recorded and 53.51% of device
  // sessions / 52.09% of IP sessions landed outside them — the exported
  // `HAS_USED` interval and the exported `transactions.device_id`
  // contradicted each other on the same row.
  //
  // THE LIVE SET IS NEVER EMPTY for an in-window `ts`, because personal
  // endpoints are generated as TILING replacement chains
  // (synth/infra/timeline.hpp sampleChain). This routine still handles
  // an empty live set by returning nullopt rather than asserting: a
  // caller may legitimately ask about a `ts` outside the generation
  // window, and the card-fraud sink already refuses a sessionless row
  // loudly at the export boundary.
  //
  // SUCCESSION IS POSITIONAL, NOT A SEARCH. Each device line is emitted
  // CONTIGUOUSLY into the pool, so when the current endpoint's tenure
  // ends its replacement is the next live index at a HIGHER position.
  // Scanning forward from `cur` therefore walks the line the person was
  // already on, and only wraps when that line is exhausted. This is why
  // the generator's contiguous-per-line emission order is a contract and
  // not an incidental detail.
  //
  // Replacement DRAWS NOTHING. A person whose phone dies moves to its
  // successor deterministically; only a voluntary switch among
  // concurrently-live endpoints costs randomness. Keeping the draw
  // pattern tied to liveness (not to raw pool size, which now grows with
  // window length) is what keeps both engines in lockstep.
  template <typename T>
  [[nodiscard]] std::optional<T> routeFromPool(
      random::Rng &rng, entity::PersonId person, std::int64_t ts,
      const std::unordered_map<entity::PersonId, std::vector<T>> &pool,
      const std::unordered_map<entity::PersonId, std::vector<Tenure>> &tenures,
      std::vector<std::size_t> &current) const {
    const auto poolIt = pool.find(person);
    if (poolIt == pool.end() || poolIt->second.empty()) {
      return std::nullopt;
    }

    const auto &items = poolIt->second;

    // In-bounds by construction: `current` was sized from this pool's keys
    // and `person` is one of them. Distinct persons touch distinct
    // elements, so concurrent emission workers — which own disjoint person
    // shards and synchronize at day boundaries — never race here. The
    // former unordered_map caches raced on the shared bucket array even
    // with disjoint keys.
    auto &cur = current[person];

    // No tenure table supplied for this person: fall back to the
    // lifetime-pool behaviour. This is the shape a hand-built test
    // Router has, and it keeps such fixtures meaningful.
    const auto tenureIt = tenures.find(person);
    if (tenureIt == tenures.end() || tenureIt->second.size() != items.size()) {
      if (cur == kUnanchored) {
        cur = 0;
        return items[0];
      }
      if (items.size() > 1 && rng.coin(rules_.switchP)) {
        auto pickIdx = rng.choiceIndex(items.size() - 1);
        if (pickIdx >= cur) {
          ++pickIdx;
        }
        cur = pickIdx;
      }
      return items[cur];
    }

    const auto &spans = tenureIt->second;
    const auto live = [&](std::size_t i) { return spans[i].contains(ts); };

    std::size_t liveCount = 0;
    std::size_t firstLive = items.size();
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (live(i)) {
        ++liveCount;
        if (firstLive == items.size()) {
          firstLive = i;
        }
      }
    }
    if (liveCount == 0) {
      return std::nullopt;
    }

    if (cur == kUnanchored || cur >= items.size() || !live(cur)) {
      // Anchor, or succeed a retired endpoint by walking forward from
      // where the person already was. Draws nothing.
      std::size_t next = items.size();
      const std::size_t from =
          (cur == kUnanchored || cur >= items.size()) ? 0 : cur + 1;
      for (std::size_t i = from; i < items.size(); ++i) {
        if (live(i)) {
          next = i;
          break;
        }
      }
      cur = next != items.size() ? next : firstLive;
    }

    // A voluntary switch is only meaningful between endpoints the person
    // holds AT THE SAME TIME. Draw once, then map the choice onto the
    // live set, skipping the current position so the pick is guaranteed
    // different without a rejection loop.
    if (liveCount > 1 && rng.coin(rules_.switchP)) {
      auto pick = rng.choiceIndex(liveCount - 1);
      for (std::size_t i = 0; i < items.size(); ++i) {
        if (i == cur || !live(i)) {
          continue;
        }
        if (pick == 0) {
          cur = i;
          break;
        }
        --pick;
      }
    }

    return items[cur];
  }

  // The draw-free, state-free companion to routeFromPool. Same liveness
  // rule, same fallback for a tenure-less hand-built fixture (index 0),
  // but the choice among live entries comes from `salt` instead of a
  // coin and nothing is written back.
  template <typename T>
  [[nodiscard]] std::optional<T>
  liveFromPool(entity::PersonId person, std::int64_t ts, std::int64_t endTsExcl,
               std::uint64_t salt,
               const std::unordered_map<entity::PersonId, std::vector<T>> &pool,
               const std::unordered_map<entity::PersonId, std::vector<Tenure>>
                   &tenures) const {
    const auto poolIt = pool.find(person);
    if (poolIt == pool.end() || poolIt->second.empty()) {
      return std::nullopt;
    }
    const auto &items = poolIt->second;

    const auto tenureIt = tenures.find(person);
    if (tenureIt == tenures.end() || tenureIt->second.size() != items.size()) {
      return items[static_cast<std::size_t>(salt % items.size())];
    }

    const auto &spans = tenureIt->second;
    const auto covers = [&](std::size_t i) {
      return spans[i].contains(ts) && spans[i].lastEpochExcl >= endTsExcl;
    };

    std::size_t live = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (covers(i)) {
        ++live;
      }
    }
    if (live == 0) {
      return std::nullopt;
    }

    auto want = static_cast<std::size_t>(salt % live);
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (!covers(i)) {
        continue;
      }
      if (want == 0) {
        return items[i];
      }
      --want;
    }
    return std::nullopt;
  }

  RoutingRules rules_{};

  std::unordered_map<entity::Key, entity::PersonId> ownerOf_;
  std::unordered_map<entity::PersonId, std::vector<devices::Identity>>
      devicesByPerson_;
  std::unordered_map<entity::PersonId, std::vector<network::Ipv4>> ipsByPerson_;

  // PARALLEL to the pools above: index i is when pool entry i belonged to
  // that person. Empty (or size-mismatched) means "no tenure model", the
  // hand-built-fixture shape.
  std::unordered_map<entity::PersonId, std::vector<Tenure>> deviceTenures_;
  std::unordered_map<entity::PersonId, std::vector<Tenure>> ipTenures_;

  // Endpoints the person PASSES THROUGH, deliberately outside the pools
  // above and outside the sticky walk. Inactive by default.
  TerminalPool terminals_;
  CarrierNatPool carrierNat_;

  // Sticky routing state as pre-sized, sentinel-initialized vectors
  // indexed by PersonId. Mutable so routing stays logically const. The
  // semantics match the previous map-based caches exactly (first use
  // anchors on index 0 without drawing; later uses may switch), so output
  // is byte-identical — this layout exists to make disjoint-person writes
  // touch disjoint memory locations.
  mutable std::vector<std::size_t> currentDeviceIdx_;
  mutable std::vector<std::size_t> currentIpIdx_;
};

} // namespace PhantomLedger::infra
