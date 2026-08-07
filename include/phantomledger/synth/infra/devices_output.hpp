#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/entities/infra/tenure.hpp"
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

  // Is this (party, device) association ON FILE with the institution?
  // A DRAW-FREE fact — `infra::enrollment::deviceEnrolled` hashes the
  // pair — so filling it perturbs no downstream draw, and it is a pure
  // function of world state, which is what lets the card-fraud export
  // publish `Has_Device` and still satisfy the world-derived
  // full-vs-prefix identity gate.
  //
  // WHY THE FLAG EXISTS AT ALL is the whole argument in
  // entities/infra/enrollment.hpp: a registry with 100% coverage made
  // "no Party edge" an exact synonym for "attacker endpoint", which is
  // why these ownership tables shipped header-only for four rounds.
  bool enrolled = false;
};

struct Output {
  std::vector<Record> records;
  std::vector<Usage> usages;

  std::unordered_map<entity::PersonId,
                     std::vector<::PhantomLedger::devices::Identity>>
      byPerson;

  // PARALLEL to byPerson: index i is the interval during which index i
  // of that person's pool belongs to them. The access router consumes
  // this to keep session attribution point-in-time honest; its sticky
  // state is a pool index, so the two arrays share one coordinate
  // system. Every push to byPerson must push here in the same step.
  std::unordered_map<entity::PersonId,
                     std::vector<::PhantomLedger::infra::Tenure>>
      tenureByPerson;

  std::unordered_map<std::uint32_t, ::PhantomLedger::devices::Identity> ringMap;

  /* PUBLIC TERMINALS, and they are deliberately NOT in `byPerson`.
   *
   * A terminal is passed through, not held: nobody owns it, so it has no
   * `Usage` row and no ownership edge, and the router must never treat it as
   * one of a person's own endpoints — see the head of
   * `entities/infra/public_endpoints.hpp` for why an entry in the per-person
   * pool would take a third to a half of that person's rows and add a coin to
   * every routed row besides.
   *
   * Its links ARE in `records`, so a terminal is a device vertex like any
   * other. Resolution is a draw-free point query on the pool itself. */
  ::PhantomLedger::infra::TerminalPool terminals;
};

} // namespace PhantomLedger::synth::infra::devices
