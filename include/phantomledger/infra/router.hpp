#pragma once
/*
 * Routes a transaction to a specific (device, ip) pair based on the
 * account owner. Uses a "sticky current device/ip" per person that
 * occasionally switches with a configured probability.
 */

#include "phantomledger/devices/identity.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/network/ipv4.hpp"
#include "phantomledger/random/rng.hpp"

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
  /// ownerOf:        account Key → person ID
  /// devicesByPerson: person ID → list of device identities
  /// ipsByPerson:     person ID → list of IP addresses
  /// switchP:         probability of switching to an alternate device/ip
  static Router build(double switchP,
                      std::unordered_map<entities::identifier::Key,
                                         entities::identifier::PersonId>
                          ownerOf,
                      std::unordered_map<entities::identifier::PersonId,
                                         std::vector<devices::Identity>>
                          devicesByPerson,
                      std::unordered_map<entities::identifier::PersonId,
                                         std::vector<network::Ipv4>>
                          ipsByPerson) {
    Router r;
    r.switchP_ = switchP;
    r.ownerOf_ = std::move(ownerOf);
    r.devicesByPerson_ = std::move(devicesByPerson);
    r.ipsByPerson_ = std::move(ipsByPerson);

    // Seed sticky state with the first device/ip per person.
    for (const auto &[person, devices] : r.devicesByPerson_) {
      if (!devices.empty()) {
        r.currentDevice_[person] = devices.front();
      }
    }
    for (const auto &[person, ips] : r.ipsByPerson_) {
      if (!ips.empty()) {
        r.currentIp_[person] = ips.front();
      }
    }

    return r;
  }

  /// Look up the owner and route to their current active infra.
  [[nodiscard]] InfraAttribution
  routeSource(random::Rng &rng,
              const entities::identifier::Key &srcAcct) const {
    auto ownerIt = ownerOf_.find(srcAcct);
    if (ownerIt == ownerOf_.end()) {
      return {};
    }

    const auto person = ownerIt->second;
    return {
        routeFromPool(rng, person, devicesByPerson_, currentDevice_),
        routeFromPool(rng, person, ipsByPerson_, currentIp_),
    };
  }

private:
  template <typename T>
  [[nodiscard]] std::optional<T> routeFromPool(
      random::Rng &rng, entities::identifier::PersonId person,
      const std::unordered_map<entities::identifier::PersonId, std::vector<T>>
          &pool,
      std::unordered_map<entities::identifier::PersonId, T> &current) const {
    auto poolIt = pool.find(person);
    if (poolIt == pool.end() || poolIt->second.empty()) {
      return std::nullopt;
    }

    const auto &items = poolIt->second;
    auto curIt = current.find(person);

    if (curIt == current.end()) {
      // First use: pick the first item.
      auto chosen = items.front();
      current[person] = chosen;
      return chosen;
    }

    // Possibly switch to an alternate.
    if (items.size() > 1 && rng.coin(switchP_)) {
      // Pick a different one.
      for (int tries = 0; tries < 8; ++tries) {
        const auto idx = rng.choiceIndex(items.size());
        if (!(items[idx] == curIt->second)) {
          curIt->second = items[idx];
          return curIt->second;
        }
      }
    }

    return curIt->second;
  }

  double switchP_ = 0.05;

  std::unordered_map<entities::identifier::Key, entities::identifier::PersonId>
      ownerOf_;
  std::unordered_map<entities::identifier::PersonId,
                     std::vector<devices::Identity>>
      devicesByPerson_;
  std::unordered_map<entities::identifier::PersonId, std::vector<network::Ipv4>>
      ipsByPerson_;

  // Mutable sticky state. Marked mutable so routeSource can remain
  // logically const — the sticky state is a cache, not semantic state.
  mutable std::unordered_map<entities::identifier::PersonId, devices::Identity>
      currentDevice_;
  mutable std::unordered_map<entities::identifier::PersonId, network::Ipv4>
      currentIp_;
};

} // namespace PhantomLedger::infra
