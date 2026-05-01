#pragma once

#include "phantomledger/taxonomies/counterparties/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace PhantomLedger::counterparties {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

struct AccountId {
  std::string_view value{};

  constexpr std::strong_ordering operator<=>(const AccountId &) const = default;
};

struct Entry {
  AccountId id;
  std::string_view name;
};

inline constexpr AccountId none{};

template <class T> using Bare = std::remove_cvref_t<T>;

template <class T>
concept AccountEnum =
    enumTax::OneOf<Bare<T>, Government, Insurance, Lending, Tax> &&
    enumTax::ByteEnum<Bare<T>>;

namespace detail {

template <std::size_t N>
[[nodiscard]] consteval std::array<lookup::Entry<AccountId>, N>
byNameEntries(const std::array<Entry, N> &entries) {
  std::array<lookup::Entry<AccountId>, N> out{};

  for (std::size_t i = 0; i < N; ++i) {
    out[i] = lookup::Entry<AccountId>{
        .name = entries[i].name,
        .value = entries[i].id,
    };
  }

  return out;
}

template <std::size_t N>
[[nodiscard]] consteval std::array<lookup::Entry<std::string_view>, N>
byIdEntries(const std::array<Entry, N> &entries) {
  std::array<lookup::Entry<std::string_view>, N> out{};

  for (std::size_t i = 0; i < N; ++i) {
    out[i] = lookup::Entry<std::string_view>{
        .name = entries[i].id.value,
        .value = entries[i].name,
    };
  }

  return out;
}

template <std::size_t N>
consteval void validateEntries(const std::array<Entry, N> &entries) {
  lookup::requireUniqueNames(lookup::sorted(byNameEntries(entries)));
  lookup::requireUniqueNames(lookup::sorted(byIdEntries(entries)));
}

template <class Enum> struct Tables;

} // namespace detail

inline constexpr std::array<Entry, 2> kGovernment{{
    {AccountId{"XGOV00000001"}, "gov_ssa"},
    {AccountId{"XGOV00000002"}, "gov_disability"},
}};

inline constexpr std::array<Entry, 3> kInsurance{{
    {AccountId{"XINS00000001"}, "insurance_auto"},
    {AccountId{"XINS00000002"}, "insurance_home"},
    {AccountId{"XINS00000003"}, "insurance_life"},
}};

inline constexpr std::array<Entry, 3> kLending{{
    {AccountId{"XLND00000001"}, "lender_mortgage"},
    {AccountId{"XLND00000002"}, "lender_auto"},
    {AccountId{"XLND00000003"}, "servicer_student"},
}};

inline constexpr std::array<Entry, 1> kTax{{
    {AccountId{"XIRS00000001"}, "irs_treasury"},
}};

static_assert(kGovernment.size() == kGovernmentAccountCount);
static_assert(kInsurance.size() == kInsuranceAccountCount);
static_assert(kLending.size() == kLendingAccountCount);
static_assert(kTax.size() == kTaxAccountCount);

inline constexpr std::array<AccountId, 9> kAll{
    kGovernment[0].id, kGovernment[1].id, kInsurance[0].id,
    kInsurance[1].id,  kInsurance[2].id,  kLending[0].id,
    kLending[1].id,    kLending[2].id,    kTax[0].id,
};

static_assert(kAll.size() == kGovernmentAccountCount + kInsuranceAccountCount +
                                 kLendingAccountCount + kTaxAccountCount);

namespace detail {

template <> struct Tables<Government> {
  [[nodiscard]] static constexpr const auto &entries() noexcept {
    return kGovernment;
  }
};

template <> struct Tables<Insurance> {
  [[nodiscard]] static constexpr const auto &entries() noexcept {
    return kInsurance;
  }
};

template <> struct Tables<Lending> {
  [[nodiscard]] static constexpr const auto &entries() noexcept {
    return kLending;
  }
};

template <> struct Tables<Tax> {
  [[nodiscard]] static constexpr const auto &entries() noexcept { return kTax; }
};

inline constexpr bool kValidated =
    (validateEntries(kGovernment), validateEntries(kInsurance),
     validateEntries(kLending), validateEntries(kTax), true);

} // namespace detail

template <AccountEnum Enum>
[[nodiscard]] constexpr AccountId id(Enum value) noexcept {
  return detail::Tables<Bare<Enum>>::entries()[enumTax::toIndex(value)].id;
}

template <AccountEnum Enum>
[[nodiscard]] constexpr std::string_view name(Enum value) noexcept {
  return detail::Tables<Bare<Enum>>::entries()[enumTax::toIndex(value)].name;
}

template <AccountEnum Enum>
[[nodiscard]] constexpr bool is(AccountId account, Enum value) noexcept {
  return account == id(value);
}

} // namespace PhantomLedger::counterparties
