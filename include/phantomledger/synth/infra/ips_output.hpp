#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::synth::infra::ips {

struct Record {
  network::Ipv4 address{};
  bool blacklisted = false;
};

struct Usage {
  entity::PersonId personId = entity::invalidPerson;
  network::Ipv4 ipAddress{};
  time::TimePoint firstSeen{};
  time::TimePoint lastSeen{};
};

struct Output {
  std::vector<Record> records;
  std::vector<Usage> usages;

  std::unordered_map<entity::PersonId, std::vector<network::Ipv4>> byPerson;

  std::unordered_map<std::uint32_t, network::Ipv4> ringMap;
};

} // namespace PhantomLedger::synth::infra::ips
