#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

// Victim-fraud rails (scam-fraud-2026-07, extended victimization-v3).
// card and ato are UNAUTHORIZED third-party fraud; giftCardScam and
// scamImpostor are victim-AUTHORIZED (the victim moves their own money
// under deception), which is why they get their own fraud_type labels
// and, unlike the card rail, never produce a reimbursement.
//
// The two authorized rails differ by PAYMENT METHOD, and the split is
// deliberate: a gift-card scam is a denomination lattice at a retail
// merchant, an impostor push is a continuous amount to a payee account.
// One label for both would have blurred the two mechanisms a model
// actually sees.
enum class Rail : std::uint8_t {
  card = 0,         // stolen-credential card compromise: tests + spends
  ato = 1,          // bank-rail account takeover: p2p drains to a drop
  giftCardScam = 2, // impostor scam: max-denomination gift-card burst
  scamImpostor = 3, // impostor scam: victim-authorized wire / app push
};

[[nodiscard]] constexpr bool authorizedRail(Rail rail) noexcept {
  return rail == Rail::giftCardScam || rail == Rail::scamImpostor;
}

struct CompromisePlan {
  entity::Key victimAccount;
  entity::Key dropAccount;
  devices::Identity device;
  network::Ipv4 ip;
  Rail rail = Rail::card;
  std::int64_t startTs = 0;
  std::int32_t spanSeconds = 0;
  std::int32_t targetEvents = 0;

  // Exclusive temporal horizon for every event in this case. Planning
  // resolves it as the earliest relevant participant boundary: victim
  // account closure for every rail, victim death for authorized rails,
  // and payee/drop closure when that endpoint is a customer. A production
  // plan is accepted only when its complete sampled span fits before this
  // horizon; the generator rejects malformed standalone plans rather than
  // compressing a multi-hour case into an artificial boundary burst.
  //
  // The permissive default keeps standalone/unit callers carrier-free.
  std::int64_t eventEndTsExclusive = std::numeric_limits<std::int64_t>::max();

  std::uint32_t seq = 0;

  // card-fraud-realism-v2 step b: the VICTIM'S HOME AREA — the anchor
  // the card-present branch of merchant selection decays away from, so
  // a stolen card is spent near where the cardholder lives rather than
  // uniformly across the country. invalidGeoArea (the default) means
  // "no local anchor": selection falls back to the geography-free
  // national draw, exactly as the spending session does for a person
  // with no home carrier.
  entity::geography::GeoAreaId homeArea = entity::geography::invalidGeoArea;

  // victimization-v3 (docs/card_fraud_victimization.md D2): the victim's
  // AGE-GRADED LOSS MULTIPLIER at the case date, from
  // susceptibility::scamAgeSeverity. FTC CSN median reported loss rises
  // monotonically with age — roughly 3x from the youngest band to the
  // oldest — and that is the strongest single fact about authorized
  // scams that the amount distribution can carry.
  //
  // IT APPLIES TO THE IMPOSTOR RAIL ONLY, where it scales the continuous
  // push amount (amounts.hpp scamWireAmount). The gift-card rail is
  // UNGRADED in both denomination and count, and the reasoning is worth
  // keeping because the obvious alternative was tried and reverted in
  // the same round: a rack caps a card at $500 whoever is buying, so the
  // denomination cannot carry severity, and grading the CARD COUNT
  // instead put an 80-year-old at 13 x $500 = $6,500 inside four hours
  // out of a retail checking account. Most of those rows are unfundable,
  // so the ledger discards them — the visible result is a decline burst,
  // not a larger loss. See injector.cpp's targetSpan comment and
  // authority U-12 (amended row).
  //
  // 1.0 — the default — is "no grading", which is what the gift-card
  // rail, both unauthorized rails, and every caller without the age
  // carrier get.
  double severity = 1.0;

  // venue-reuse-2026-08: the ATTACKER CAMPAIGN this case belongs to — an
  // index into `infra::AttackerInfra::operators`, which is that type's only
  // identity (it has no id field). Resolved DRAW-FREE in the planner from the
  // uniform already spent on endpoint attribution.
  //
  // It exists so a campaign's cases can converge on a shared cash-out venue.
  // kNoCampaign means "no campaign" and every slot is drawn independently,
  // which is what the AUTHORIZED rails, the victim-endpoint branch, and every
  // hand-built unit plan get — the sentinel default is what keeps those
  // callers carrier-free.
  static constexpr std::uint32_t kNoCampaign = 0xFFFFFFFFU;
  std::uint32_t campaign = kNoCampaign;
};

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, std::span<const CompromisePlan> plans,
         std::int32_t budget);

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
