#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/entities/infra/tenure.hpp"
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

  // Registry coverage for this (party, address) pair; see the same field
  // on devices_output.hpp Usage and entities/infra/enrollment.hpp for
  // why partial coverage is the fix rather than a compromise. Draw-free.
  bool enrolled = false;
};

struct Output {
  std::vector<Record> records;
  std::vector<Usage> usages;

  std::unordered_map<entity::PersonId, std::vector<network::Ipv4>> byPerson;

  // PARALLEL to byPerson; see devices_output.hpp for the contract.
  std::unordered_map<entity::PersonId,
                     std::vector<::PhantomLedger::infra::Tenure>>
      tenureByPerson;

  std::unordered_map<std::uint32_t, network::Ipv4> ringMap;

  /* CARRIER-GRADE NAT ADDRESSES, and they are deliberately NOT in `byPerson`
   * — the same rule and the same reason as the terminal pool on the device
   * side. A subscriber does not HOLD the carrier's public address; they exit
   * through it. It has no `Usage` row and therefore no ownership edge, which
   * is also what the exported graph should say about it.
   *
   * The addresses ARE in `records`, so each is an IP vertex like any other. */
  ::PhantomLedger::infra::CarrierNatPool carrierNat;
};

} // namespace PhantomLedger::synth::infra::ips
