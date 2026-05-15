#pragma once

#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PhantomLedger::infra {

struct SharedInfraRules {
  double useSharedDeviceP = 0.85;
  double useSharedIpP = 0.80;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("useSharedDeviceP", useSharedDeviceP, 0.0, 1.0); });
    r.check([&] { v::between("useSharedIpP", useSharedIpP, 0.0, 1.0); });
  }
};

struct SharedInfra {
  std::unordered_map<std::uint32_t, devices::Identity> ringDevice;
  std::unordered_map<std::uint32_t, network::Ipv4> ringIp;

  // Defaults derive from SharedInfraRules{} so there is exactly one source
  // of truth for shared-infra defaults; changing the rule struct is enough.
  double useSharedDeviceP = SharedInfraRules{}.useSharedDeviceP;
  double useSharedIpP = SharedInfraRules{}.useSharedIpP;

  void apply(SharedInfraRules rules) noexcept {
    useSharedDeviceP = rules.useSharedDeviceP;
    useSharedIpP = rules.useSharedIpP;
  }

  [[nodiscard]] std::optional<devices::Identity>
  deviceForRing(std::int32_t ringId) const {
    if (ringId < 0) {
      return std::nullopt;
    }
    auto it = ringDevice.find(static_cast<std::uint32_t>(ringId));
    if (it == ringDevice.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  [[nodiscard]] std::optional<network::Ipv4>
  ipForRing(std::int32_t ringId) const {
    if (ringId < 0) {
      return std::nullopt;
    }
    auto it = ringIp.find(static_cast<std::uint32_t>(ringId));
    if (it == ringIp.end()) {
      return std::nullopt;
    }
    return it->second;
  }
};

} // namespace PhantomLedger::infra
