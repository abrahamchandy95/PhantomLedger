#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::synth::infra::devices {

struct Record {
  ::PhantomLedger::devices::Identity identity{};
  DeviceKind kind = DeviceKind::android;
  bool flagged = false;
};

struct Usage {
  entity::PersonId personId = entity::invalidPerson;
  ::PhantomLedger::devices::Identity deviceId{};
  time::TimePoint firstSeen{};
  time::TimePoint lastSeen{};
};

struct Output {
  std::vector<Record> records;
  std::vector<Usage> usages;

  std::unordered_map<entity::PersonId,
                     std::vector<::PhantomLedger::devices::Identity>>
      byPerson;

  std::unordered_map<std::uint32_t, ::PhantomLedger::devices::Identity> ringMap;
};

} // namespace PhantomLedger::synth::infra::devices
