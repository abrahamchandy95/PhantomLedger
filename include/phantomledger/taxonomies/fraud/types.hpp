#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::fraud {

enum class Typology : std::uint8_t {
  classic = 0,
  layering = 1,
  funnel = 2,
  structuring = 3,
  invoice = 4,
  mule = 5,
  cycle = 6,
  scatterGather = 7,
  bipartite = 8,
};

inline constexpr auto kTypologies = std::to_array<Typology>({
    Typology::classic,
    Typology::layering,
    Typology::funnel,
    Typology::structuring,
    Typology::invoice,
    Typology::mule,
    Typology::cycle,
    Typology::scatterGather,
    Typology::bipartite,
});

inline constexpr std::size_t kTypologyCount = kTypologies.size();

enum class FraudType : std::uint8_t {
  none = 0,
  launderRing = 1,
  launderSolo = 2,
  txnFraudSolo = 3,
  txnFraudRing = 4,
  // Victim-AUTHORIZED impostor scam (the victim is manipulated into
  // buying gift cards) — a different label class from unauthorized
  // txn fraud: legit card rail, round denominations, near-zero
  // recovery (scam-fraud-2026-07; docs/fraud_model_audit.md F-4).
  scamGiftCard = 5,
  // Victim-AUTHORIZED impostor scam paid by PUSH TRANSFER — the victim
  // wires or app-pushes their own money to the attacker's payee
  // account. Gift cards are one payment method; the transfer rail is
  // the other, and in the corpus era it is the larger one by reported
  // loss. Separate from scamGiftCard because the payment mechanism is
  // what a model sees: different channel, continuous (not
  // denomination-lattice) amounts, age-graded severity, and — like
  // every authorized push — NO reimbursement
  // (victimization-v3; docs/card_fraud_victimization.md D2).
  scamImpostor = 6,
};

[[nodiscard]] constexpr std::string_view fraudTypeName(FraudType t) noexcept {
  switch (t) {
  case FraudType::none:
    return {};
  case FraudType::launderRing:
    return "launder_ring";
  case FraudType::launderSolo:
    return "launder_solo";
  case FraudType::txnFraudSolo:
    return "txn_fraud_solo";
  case FraudType::txnFraudRing:
    return "txn_fraud_ring";
  case FraudType::scamGiftCard:
    return "scam_gift_card";
  case FraudType::scamImpostor:
    return "scam_impostor";
  }
  return {};
}

} // namespace PhantomLedger::fraud
