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

// Victim-fraud rails (scam-fraud-2026-07). card and ato are
// UNAUTHORIZED third-party fraud; giftCardScam is victim-AUTHORIZED
// (impostor scams — the victim buys the cards themselves), which is
// why it gets its own fraud_type label and, unlike the card rail,
// never produces a reimbursement.
enum class Rail : std::uint8_t {
  card = 0,         // stolen-credential card compromise: tests + spends
  ato = 1,          // bank-rail account takeover: p2p drains to a drop
  giftCardScam = 2, // impostor scam: max-denomination gift-card burst
};

struct CompromisePlan {
  entity::Key victimAccount;
  entity::Key dropAccount;
  devices::Identity device;
  network::Ipv4 ip;
  Rail rail = Rail::card;
  std::int64_t startTs = 0;
  std::int32_t spanSeconds = 0;
  std::int32_t targetEvents = 0;

  std::uint32_t seq = 0;
};

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, std::span<const CompromisePlan> plans,
         std::int32_t budget);

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
