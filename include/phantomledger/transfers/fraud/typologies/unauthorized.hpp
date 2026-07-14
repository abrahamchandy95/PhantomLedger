#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

struct CompromisePlan {
  entity::Key victimAccount;
  entity::Key dropAccount;
  devices::Identity device;
  network::Ipv4 ip;
  bool cardRail = true;
  std::int64_t startTs = 0;
  std::int32_t spanSeconds = 0;
  std::int32_t targetEvents = 0;
};

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, std::span<const CompromisePlan> plans,
         std::int32_t budget);

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
