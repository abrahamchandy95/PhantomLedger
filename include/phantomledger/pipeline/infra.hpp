#pragma once

#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::pipeline {

struct Infra {
  std::unordered_map<std::uint32_t, synth::infra::RingPlan> ringPlans;
  synth::infra::devices::Output devices;
  synth::infra::ips::Output ips;

  ::PhantomLedger::infra::Router router;
  ::PhantomLedger::infra::SharedInfra ringInfra;

  /* The exogenous fraud-infrastructure pool the unauthorized card/ATO rails
   * transact from. It is WORLD state, not planner state, because it must
   * outlive a single injection: reuse across cases is the whole point, and a
   * per-inject pool would be a per-inject set of ownerless endpoints under a
   * new name. Read by the fraud injector (endpoint resolution) and by the
   * card-fraud exporter (ground-truth endpoint verdicts). */
  ::PhantomLedger::infra::AttackerInfra attackers;
};

} // namespace PhantomLedger::pipeline
