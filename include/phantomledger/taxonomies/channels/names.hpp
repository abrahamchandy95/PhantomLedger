#pragma once

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::channels {
namespace detail {

// --- Name table --------------------------------------------------

inline constexpr std::array<lookup::Entry<Tag>, 59> kEntries{{
    {"salary", tag(Legit::salary)},
    {"merchant", tag(Legit::merchant)},
    {"card_purchase", tag(Legit::cardPurchase)},
    {"bill", tag(Legit::bill)},
    {"p2p", tag(Legit::p2p)},
    {"external_unknown", tag(Legit::externalUnknown)},
    {"atm_withdrawal", tag(Legit::atm)},
    {"self_transfer", tag(Legit::selfTransfer)},
    {"subscription", tag(Legit::subscription)},
    {"client_ach_credit", tag(Legit::clientAchCredit)},
    {"card_settlement", tag(Legit::cardSettlement)},
    {"platform_payout", tag(Legit::platformPayout)},
    {"owner_draw", tag(Legit::ownerDraw)},
    {"investment_inflow", tag(Legit::investmentInflow)},

    {"rent", tag(Rent::generic)},
    {"rent_ach", tag(Rent::ach)},
    {"rent_portal", tag(Rent::portal)},
    {"rent_p2p", tag(Rent::p2p)},
    {"rent_check", tag(Rent::check)},

    {"allowance", tag(Family::allowance)},
    {"tuition", tag(Family::tuition)},
    {"family_support", tag(Family::support)},
    {"spouse_transfer", tag(Family::spouseTransfer)},
    {"parent_gift", tag(Family::parentGift)},
    {"sibling_transfer", tag(Family::siblingTransfer)},
    {"grandparent_gift", tag(Family::grandparentGift)},
    {"inheritance", tag(Family::inheritance)},

    {"cc_payment", tag(Credit::payment)},
    {"cc_interest", tag(Credit::interest)},
    {"cc_late_fee", tag(Credit::lateFee)},
    {"cc_refund", tag(Credit::refund)},
    {"cc_chargeback", tag(Credit::chargeback)},

    {"mortgage_payment", tag(Product::mortgage)},
    {"auto_loan_payment", tag(Product::autoLoan)},
    {"student_loan_payment", tag(Product::studentLoan)},
    {"tax_estimated_payment", tag(Product::taxEstimated)},
    {"tax_balance_due", tag(Product::taxBalanceDue)},
    {"tax_refund", tag(Product::taxRefund)},

    {"gov_social_security", tag(Government::socialSecurity)},
    {"gov_pension", tag(Government::pension)},
    {"gov_disability", tag(Government::disability)},

    {"insurance_premium", tag(Insurance::premium)},
    {"insurance_claim", tag(Insurance::claim)},

    {"fraud_classic", tag(Fraud::classic)},
    {"fraud_cycle", tag(Fraud::cycle)},
    {"fraud_layering_in", tag(Fraud::layeringIn)},
    {"fraud_layering_hop", tag(Fraud::layeringHop)},
    {"fraud_layering_out", tag(Fraud::layeringOut)},
    {"fraud_funnel_in", tag(Fraud::funnelIn)},
    {"fraud_funnel_out", tag(Fraud::funnelOut)},
    {"fraud_structuring", tag(Fraud::structuring)},
    {"fraud_invoice", tag(Fraud::invoice)},
    {"fraud_mule_in", tag(Fraud::muleIn)},
    {"fraud_mule_forward", tag(Fraud::muleForward)},

    {"camouflage_bill", tag(Camouflage::bill)},
    {"camouflage_p2p", tag(Camouflage::p2p)},
    {"camouflage_salary", tag(Camouflage::salary)},

    {"overdraft_fee", tag(Liquidity::overdraftFee)},
    {"loc_interest", tag(Liquidity::locInterest)},
}};

inline constexpr auto kSorted = lookup::sorted(kEntries);

// Every byte value is a valid Tag slot.
inline constexpr auto kNames =
    lookup::reverseTable<256>(kEntries, [](Tag t) { return t.value; });

inline constexpr auto kKnown =
    lookup::presenceTable<256>(kEntries, [](Tag t) { return t.value; });

// Subset of tags that represent inbound payday events.
[[nodiscard]] consteval std::array<bool, 256> buildPaydayInboundTable() {
  std::array<bool, 256> out{};

  out[tag(Legit::salary).value] = true;
  out[tag(Legit::clientAchCredit).value] = true;
  out[tag(Legit::cardSettlement).value] = true;
  out[tag(Legit::platformPayout).value] = true;
  out[tag(Legit::ownerDraw).value] = true;
  out[tag(Legit::investmentInflow).value] = true;

  out[tag(Government::socialSecurity).value] = true;
  out[tag(Government::pension).value] = true;
  out[tag(Government::disability).value] = true;

  return out;
}

inline constexpr auto kPaydayInbound = buildPaydayInboundTable();

// Compile-time validation.
inline constexpr bool kValidated = (lookup::requireUniqueNames(kSorted), true);

} // namespace detail

// --- Public API --------------------------------------------------

[[nodiscard]] constexpr std::string_view name(Tag t) noexcept {
  return detail::kNames[t.value];
}

template <Enum E> [[nodiscard]] constexpr std::string_view name(E v) noexcept {
  return name(tag(v));
}

[[nodiscard]] constexpr std::optional<Tag> parse(std::string_view s) noexcept {
  return lookup::find(detail::kSorted, s);
}

} // namespace PhantomLedger::channels
