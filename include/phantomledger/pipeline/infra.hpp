#pragma once

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
};

} // namespace PhantomLedger::pipeline
