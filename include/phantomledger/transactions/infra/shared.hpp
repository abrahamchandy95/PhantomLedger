#pragma once

#include "phantomledger/transactions/devices/identity.hpp"
#include "phantomledger/transactions/network/ipv4.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PhantomLedger::infra {

struct SharedInfraRules {
  double deviceP = 0.85;
  double ipP = 0.80;
};

struct SharedInfra {
  std::unordered_map<std::uint32_t, devices::Identity> ringDevice;
  std::unordered_map<std::uint32_t, network::Ipv4> ringIp;

  double useSharedDeviceP = 0.85;
  double useSharedIpP = 0.80;

  void apply(SharedInfraRules rules) noexcept {
    useSharedDeviceP = rules.deviceP;
    useSharedIpP = rules.ipP;
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
