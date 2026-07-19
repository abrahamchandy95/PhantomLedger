#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PhantomLedger::synth::infra {

enum class DeviceKind : std::uint8_t {
  android = 0,
  ios = 1,
  web = 2,
  desktop = 3,
};

inline constexpr std::array<DeviceKind, 4> kAllDeviceKinds{
    DeviceKind::android,
    DeviceKind::ios,
    DeviceKind::web,
    DeviceKind::desktop,
};

[[nodiscard]] constexpr std::string_view name(DeviceKind k) noexcept {
  switch (k) {
  case DeviceKind::android:
    return "android";
  case DeviceKind::ios:
    return "ios";
  case DeviceKind::web:
    return "web";
  case DeviceKind::desktop:
    return "desktop";
  }
  return "<unknown>";
}

struct RingPlan {
  std::uint32_t ringId = 0;
  time::TimePoint firstSeen{};
  time::TimePoint lastSeen{};

  std::vector<entity::PersonId> sharedDeviceMembers;

  std::vector<entity::PersonId> sharedIpMembers;
};

} // namespace PhantomLedger::synth::infra
