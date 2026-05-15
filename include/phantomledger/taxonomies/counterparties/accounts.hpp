#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/counterparties/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace PhantomLedger::counterparties {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

template <class T> using Bare = std::remove_cvref_t<T>;

template <class T>
concept AccountEnum =
    enumTax::OneOf<Bare<T>, Government, Insurance, Lending, Tax> &&
    enumTax::ByteEnum<Bare<T>>;

namespace detail {

inline constexpr std::uint64_t kInstitutionalBaseSerial = 1'000'000'000ULL;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
institutional(std::uint64_t offset) noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external,
      kInstitutionalBaseSerial + offset);
}

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
governmentEmployer(std::uint64_t number) noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::employer,
      ::PhantomLedger::entity::Bank::external, number);
}

} // namespace detail

// --- Per-category tables ----------------------------------------------
// Each array's index is the enum's underlying byte value.

inline constexpr std::array<::PhantomLedger::entity::Key,
                            kGovernmentAccountCount>
    kGovernment{{
        /* [Government::ssa] */ detail::governmentEmployer(9'000'001ULL),
        /* [Government::disability] */ detail::governmentEmployer(9'000'002ULL),
    }};

inline constexpr std::array<::PhantomLedger::entity::Key,
                            kInsuranceAccountCount>
    kInsurance{{
        /* [Insurance::autoCarrier] */ detail::institutional(5),
        /* [Insurance::homeCarrier] */ detail::institutional(6),
        /* [Insurance::lifeCarrier] */ detail::institutional(7),
    }};

inline constexpr std::array<::PhantomLedger::entity::Key, kLendingAccountCount>
    kLending{{
        /* [Lending::mortgage] */ detail::institutional(1),
        /* [Lending::autoLoan] */ detail::institutional(2),
        /* [Lending::studentServicer] */ detail::institutional(3),
    }};

inline constexpr std::array<::PhantomLedger::entity::Key, kTaxAccountCount>
    kTax{{
        /* [Tax::irsTreasury] */ detail::institutional(4),
    }};

static_assert(kGovernment.size() == kGovernmentAccountCount);
static_assert(kInsurance.size() == kInsuranceAccountCount);
static_assert(kLending.size() == kLendingAccountCount);
static_assert(kTax.size() == kTaxAccountCount);

inline constexpr std::array<::PhantomLedger::entity::Key,
                            kGovernmentAccountCount + kInsuranceAccountCount +
                                kLendingAccountCount + kTaxAccountCount>
    kAll{{
        kGovernment[enumTax::toIndex(Government::ssa)],
        kGovernment[enumTax::toIndex(Government::disability)],
        kLending[enumTax::toIndex(Lending::mortgage)],
        kLending[enumTax::toIndex(Lending::autoLoan)],
        kLending[enumTax::toIndex(Lending::studentServicer)],
        kTax[enumTax::toIndex(Tax::irsTreasury)],
        kInsurance[enumTax::toIndex(Insurance::autoCarrier)],
        kInsurance[enumTax::toIndex(Insurance::homeCarrier)],
        kInsurance[enumTax::toIndex(Insurance::lifeCarrier)],
    }};

// --- Enum-keyed lookup -------------------------------------------------

namespace detail {

template <class Enum> struct Tables;

template <> struct Tables<Government> {
  [[nodiscard]] static constexpr const auto &keys() noexcept {
    return kGovernment;
  }
};

template <> struct Tables<Insurance> {
  [[nodiscard]] static constexpr const auto &keys() noexcept {
    return kInsurance;
  }
};

template <> struct Tables<Lending> {
  [[nodiscard]] static constexpr const auto &keys() noexcept {
    return kLending;
  }
};

template <> struct Tables<Tax> {
  [[nodiscard]] static constexpr const auto &keys() noexcept { return kTax; }
};

} // namespace detail

template <AccountEnum Enum>
[[nodiscard]] constexpr ::PhantomLedger::entity::Key key(Enum value) noexcept {
  return detail::Tables<Bare<Enum>>::keys()[enumTax::toIndex(value)];
}

template <AccountEnum Enum>
[[nodiscard]] constexpr bool is(::PhantomLedger::entity::Key account,
                                Enum value) noexcept {
  return account == key(value);
}

} // namespace PhantomLedger::counterparties
