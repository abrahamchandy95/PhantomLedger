#pragma once

#include "phantomledger/taxonomies/channels/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::channels {
namespace detail {

struct NamedTag {
  std::string_view name;
  Tag tag;
};

inline constexpr std::array<NamedTag, 57> kNamedTags{{
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
}};

template <std::size_t N>
[[nodiscard]] consteval std::array<NamedTag, N>
sortByName(std::array<NamedTag, N> arr) {
  for (std::size_t i = 1; i < N; ++i) {
    auto key = arr[i];
    std::size_t j = i;
    while (j > 0 && arr[j - 1].name > key.name) {
      arr[j] = arr[j - 1];
      --j;
    }
    arr[j] = key;
  }
  return arr;
}

template <std::size_t N>
[[nodiscard]] consteval std::array<std::string_view, 256>
buildNames(const std::array<NamedTag, N> &arr) {
  std::array<std::string_view, 256> names{};

  for (const auto &entry : arr) {
    if (entry.name.empty()) {
      throw "empty channel name";
    }
    if (!names[entry.tag.value].empty()) {
      throw "duplicate channel tag";
    }
    names[entry.tag.value] = entry.name;
  }

  return names;
}

template <std::size_t N>
[[nodiscard]] consteval std::array<bool, 256>
buildKnownTagTable(const std::array<NamedTag, N> &tags) {
  std::array<bool, 256> known{};

  for (const auto &entry : tags) {
    known[entry.tag.value] = true;
  }

  return known;
}

[[nodiscard]] consteval std::array<bool, 256> buildPaydayInboundTable() {
  std::array<bool, 256> paydayInbound{};

  paydayInbound[tag(Legit::salary).value] = true;
  paydayInbound[tag(Legit::clientAchCredit).value] = true;
  paydayInbound[tag(Legit::cardSettlement).value] = true;
  paydayInbound[tag(Legit::platformPayout).value] = true;
  paydayInbound[tag(Legit::ownerDraw).value] = true;
  paydayInbound[tag(Legit::investmentInflow).value] = true;

  paydayInbound[tag(Government::socialSecurity).value] = true;
  paydayInbound[tag(Government::pension).value] = true;
  paydayInbound[tag(Government::disability).value] = true;

  return paydayInbound;
}

template <std::size_t N>
consteval void validateUniqueNames(const std::array<NamedTag, N> &arr) {
  auto sorted = sortByName(arr);
  for (std::size_t i = 1; i < N; ++i) {
    if (sorted[i - 1].name == sorted[i].name) {
      throw "duplicate channel name";
    }
  }
}

inline constexpr auto kNames = buildNames(kNamedTags);
inline constexpr auto kKnownTags = buildKnownTagTable(kNamedTags);
inline constexpr auto kPaydayInboundTags = buildPaydayInboundTable();
inline constexpr auto kNamedTagsByName = sortByName(kNamedTags);
inline constexpr bool kValidated = (validateUniqueNames(kNamedTags), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Tag t) noexcept {
  return detail::kNames[t.value];
}

template <ChannelEnum Enum>
[[nodiscard]] constexpr std::string_view name(Enum v) noexcept {
  return name(tag(v));
}

[[nodiscard]] constexpr std::optional<Tag> parse(std::string_view s) noexcept {
  std::size_t lo = 0;
  std::size_t hi = detail::kNamedTagsByName.size();

  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const auto &entry = detail::kNamedTagsByName[mid];

    if (entry.name < s) {
      lo = mid + 1;
    } else if (entry.name > s) {
      hi = mid;
    } else {
      return entry.tag;
    }
  }

  return std::nullopt;
}

} // namespace PhantomLedger::channels
