#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstddef>
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

  static Router
  build(RoutingRules rules,
        std::unordered_map<entity::Key, entity::PersonId> ownerOf,
        std::unordered_map<entity::PersonId, std::vector<devices::Identity>>
            devicesByPerson,
        std::unordered_map<entity::PersonId, std::vector<network::Ipv4>>
            ipsByPerson) {
    Router r;
    r.rules_ = rules;
    r.ownerOf_ = std::move(ownerOf);
    r.devicesByPerson_ = std::move(devicesByPerson);
    r.ipsByPerson_ = std::move(ipsByPerson);
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
    if (items.size() > 1 && rng.coin(rules_.switchP)) {
      const auto curIdx = curIt->second;
      auto pickIdx = rng.choiceIndex(items.size() - 1);
      if (pickIdx >= curIdx) {
        ++pickIdx;
      }
      curIt->second = pickIdx;
    }

    return items[curIt->second];
  }

  RoutingRules rules_{};

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
