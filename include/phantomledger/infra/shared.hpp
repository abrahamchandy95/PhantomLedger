#pragma once
/*
 * Shared fraud-ring infrastructure state.
 *
 * Maps ring_id → (shared_device_id, shared_ip).
 *
 * During fraud transactions (ring_id >= 0), the TransactionFactory will
 * use the shared ring device/IP with high probability instead of the
 * person's normal infrastructure. This creates the device/IP clustering
 * signal that GraphSAGE needs to detect mule accounts.
 */

#include "phantomledger/devices/identity.hpp"
#include "phantomledger/network/ipv4.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PhantomLedger::infra {

struct SharedInfra {
  std::unordered_map<std::uint32_t, devices::Identity> ringDevice;
  std::unordered_map<std::uint32_t, network::Ipv4> ringIp;

  double useSharedDeviceP = 0.85;
  double useSharedIpP = 0.80;

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
