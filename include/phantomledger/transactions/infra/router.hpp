#pragma once
/*
 * Routes a transaction to a specific (device, ip) pair based on the
 * account owner. Uses a "sticky current device/ip" per person that
 * occasionally switches with a configured probability.
 *
 * Design notes vs. the previous revision:
 *
 *   1. routeSource() is split into ownerOf() + routeDeviceFor() +
 *      routeIpFor(). The transaction factory is now free to skip the
 *      IP resolution when shared ring IP already covered it (and vice
 *      versa), saving one RNG draw and one hash lookup per such
 *      transaction. For a workload that routes both legs on every
 *      call the two shapes are equivalent; for the fraud-ring path
 *      that typically resolves one leg via shared infra, the split
 *      is a clear win.
 *
 *   2. The sticky "current" pointer stores an *index* into the pool
 *      vector, not the item value. This fixes a silent fallthrough
 *      where the 8-try switch loop could exit without switching
 *      while having consumed RNG draws. The new switch path draws
 *      one index uniformly from the n-1 alternatives and adjusts,
 *      which is both correct and cheaper.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/devices/identity.hpp"
#include "phantomledger/transactions/network/ipv4.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::infra {

struct InfraAttribution {
  std::optional<devices::Identity> device;
  std::optional<network::Ipv4> ip;
};

class Router {
public:
  Router() = default;

  /// Build from pre-populated ownership and infra pools.
  ///
  /// ownerOf:          account Key -> person ID
  /// devicesByPerson:  person ID -> list of device identities
  /// ipsByPerson:      person ID -> list of IP addresses
  /// switchP:          probability of switching to an alternate device/ip
  static Router
  build(double switchP,
        std::unordered_map<entity::Key, entity::PersonId> ownerOf,
        std::unordered_map<entity::PersonId, std::vector<devices::Identity>>
            devicesByPerson,
        std::unordered_map<entity::PersonId, std::vector<network::Ipv4>>
            ipsByPerson) {
    Router r;
    r.switchP_ = switchP;
    r.ownerOf_ = std::move(ownerOf);
    r.devicesByPerson_ = std::move(devicesByPerson);
    r.ipsByPerson_ = std::move(ipsByPerson);

    // Sticky state starts at index 0 for every person that has a
    // non-empty pool. Indices are stable because the pools themselves
    // are immutable after build().
    for (const auto &[person, devices] : r.devicesByPerson_) {
      if (!devices.empty()) {
        r.currentDeviceIdx_[person] = 0;
      }
    }
    for (const auto &[person, ips] : r.ipsByPerson_) {
      if (!ips.empty()) {
        r.currentIpIdx_[person] = 0;
      }
    }

    return r;
  }

  /// Resolve the owner of a source account without drawing any RNG.
  /// Callers that need both device and IP can resolve once and reuse.
  [[nodiscard]] std::optional<entity::PersonId>
  ownerOf(const entity::Key &srcAcct) const {
    const auto it = ownerOf_.find(srcAcct);
    if (it == ownerOf_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  /// Route a device for a pre-resolved person.
  [[nodiscard]] std::optional<devices::Identity>
  routeDeviceFor(random::Rng &rng, entity::PersonId person) const {
    return routeFromPool(rng, person, devicesByPerson_, currentDeviceIdx_);
  }

  /// Route an IP for a pre-resolved person.
  [[nodiscard]] std::optional<network::Ipv4>
  routeIpFor(random::Rng &rng, entity::PersonId person) const {
    return routeFromPool(rng, person, ipsByPerson_, currentIpIdx_);
  }

  /// Legacy convenience: resolve owner and route both legs. Retained
  /// for callers that genuinely need both every time and benefit from
  /// the single hash lookup. Prefer the split API when possible.
  [[nodiscard]] InfraAttribution routeSource(random::Rng &rng,
                                             const entity::Key &srcAcct) const {
    const auto person = ownerOf(srcAcct);
    if (!person.has_value()) {
      return {};
    }
    return {
        routeDeviceFor(rng, *person),
        routeIpFor(rng, *person),
    };
  }

private:
  template <typename T>
  [[nodiscard]] std::optional<T> routeFromPool(
      random::Rng &rng, entity::PersonId person,
      const std::unordered_map<entity::PersonId, std::vector<T>> &pool,
      std::unordered_map<entity::PersonId, std::size_t> &current) const {
    const auto poolIt = pool.find(person);
    if (poolIt == pool.end() || poolIt->second.empty()) {
      return std::nullopt;
    }

    const auto &items = poolIt->second;
    auto curIt = current.find(person);

    if (curIt == current.end()) {
      // First use for this person: anchor on index 0 and cache it.
      current[person] = 0;
      return items[0];
    }

    // Maybe switch to an alternate. Store-as-index lets us draw from
    // the n-1 non-current slots in one go and map back via a shift,
    // which is both guaranteed-different and cheaper than a rejection
    // loop.
    if (items.size() > 1 && rng.coin(switchP_)) {
      const auto curIdx = curIt->second;
      auto pickIdx = rng.choiceIndex(items.size() - 1);
      if (pickIdx >= curIdx) {
        ++pickIdx;
      }
      curIt->second = pickIdx;
    }

    return items[curIt->second];
  }

  double switchP_ = 0.05;

  std::unordered_map<entity::Key, entity::PersonId> ownerOf_;
  std::unordered_map<entity::PersonId, std::vector<devices::Identity>>
      devicesByPerson_;
  std::unordered_map<entity::PersonId, std::vector<network::Ipv4>> ipsByPerson_;

  // Sticky state as indices into the owning pool vectors. Mutable so
  // routing can stay logically const while updating the cache.
  mutable std::unordered_map<entity::PersonId, std::size_t> currentDeviceIdx_;
  mutable std::unordered_map<entity::PersonId, std::size_t> currentIpIdx_;
};

} // namespace PhantomLedger::infra
