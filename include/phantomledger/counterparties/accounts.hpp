#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace PhantomLedger::counterparties {

struct AccountId {
  std::string_view value{};
  constexpr std::strong_ordering operator<=>(const AccountId &) const = default;
};

struct Entry {
  AccountId id;
  std::string_view name;
};

inline constexpr AccountId none{};

enum class Government : std::uint8_t {
  ssa = 0,
  disability = 1,
};

enum class Insurance : std::uint8_t {
  autoCarrier = 0,
  homeCarrier = 1,
  lifeCarrier = 2,
};

enum class Lending : std::uint8_t {
  mortgage = 0,
  autoLoan = 1,
  studentServicer = 2,
};

enum class Tax : std::uint8_t {
  irsTreasury = 0,
};

template <class T, class... Ts>
concept AccountType = (std::same_as<std::remove_cvref_t<T>, Ts> || ...);

template <class T>
concept AccountEnum = AccountType<T, Government, Insurance, Lending, Tax>;

namespace detail {

template <class Enum>
[[nodiscard]] constexpr std::size_t toIndex(Enum v) noexcept {
  return static_cast<std::size_t>(v);
}

template <std::size_t N>
consteval void validateEntries(const std::array<Entry, N> &entries) {
  for (std::size_t i = 0; i < N; ++i) {
    if (entries[i].id.value.empty()) {
      throw "empty counterparty account id";
    }
    if (entries[i].name.empty()) {
      throw "empty counterparty account name";
    }

    for (std::size_t j = i + 1; j < N; ++j) {
      if (entries[i].id == entries[j].id) {
        throw "duplicate counterparty account id";
      }
      if (entries[i].name == entries[j].name) {
        throw "duplicate counterparty account name";
      }
    }
  }
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

inline constexpr std::array<AccountId, 9> kAll{
    kGovernment[0].id, kGovernment[1].id, kInsurance[0].id,
    kInsurance[1].id,  kInsurance[2].id,  kLending[0].id,
    kLending[1].id,    kLending[2].id,    kTax[0].id,
};

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
[[nodiscard]] constexpr AccountId id(Enum v) noexcept {
  return detail::Tables<std::remove_cvref_t<Enum>>::entries()[detail::toIndex(
                                                                  v)]
      .id;
}

template <AccountEnum Enum>
[[nodiscard]] constexpr std::string_view name(Enum v) noexcept {
  return detail::Tables<std::remove_cvref_t<Enum>>::entries()[detail::toIndex(
                                                                  v)]
      .name;
}

template <AccountEnum Enum>
[[nodiscard]] constexpr bool is(AccountId account, Enum v) noexcept {
  return account == id(v);
}

} // namespace PhantomLedger::counterparties
