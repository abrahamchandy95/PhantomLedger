#pragma once
//
// phantomledger/entities/counterparties/institutional_accounts.hpp
//
// The institutional account catalog: fixed entity::Key constants for
// the external counterparties every simulation shares (the government
// benefit payers SSA and disability, and the IRS), with enum-keyed
// constexpr lookup. Lives in entities because it IS entity-key vocabulary
// (untangling round, 2026-07-19: the former home under taxonomies/ made
// the lowest vocabulary layer depend on entities above it).
//
// Lenders and insurers are NOT here any more. They were one key per
// product for the whole population; they are now per-contract provider
// pools (providers.hpp, institutional-providers-2026-09). The same round
// gave the external-unknown catch-all its own reserved key below, and
// unknown-counterparty-2026-09 retired it (remote_payees.hpp).
//

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
    enumTax::OneOf<Bare<T>, Government, Tax> &&
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

// Institutional offsets 1, 2, 3, 5, 6 and 7 are RETIRED and must never be
// reused. They were the population-wide mortgage (1), auto-loan (2),
// student-servicer (3), auto-carrier (5), home-carrier (6) and life-carrier
// (7) keys before institutional-providers-2026-09. An export written before
// that round still carries them, and a reused offset would make it misread.
inline constexpr std::array<::PhantomLedger::entity::Key, kTaxAccountCount>
    kTax{{
        /* [Tax::irsTreasury] */ detail::institutional(4),
    }};

static_assert(kGovernment.size() == kGovernmentAccountCount);
static_assert(kTax.size() == kTaxAccountCount);

inline constexpr std::array<::PhantomLedger::entity::Key,
                            kGovernmentAccountCount + kTaxAccountCount>
    kAll{{
        kGovernment[enumTax::toIndex(Government::ssa)],
        kGovernment[enumTax::toIndex(Government::disability)],
        kTax[enumTax::toIndex(Tax::irsTreasury)],
    }};

// --- The RETIRED external-unknown catch-all ---------------------------
//
// Until unknown-counterparty-2026-09 this was the one destination for the
// spending router's external-unknown slot, for P2P with no usable contact,
// and for every funeral: 3.8M payments from 62% of deposit accounts on the
// 200,000-person corpus. Each flow now pays the counterparty a bank records
// for it (remote_payees.hpp). The key is kept, never registered, so the gates
// can assert that no row names it, and validateTransactionAccounts throws if
// one ever does. Never reuse the serial: an export written before the round
// still carries it.
//
// It was makeKey(merchant, external, 1) before institutional-providers-2026-09,
// which IS catalogue merchant serial 1 whenever that merchant banks externally
// (merchant-churn-2026-07 rule 6), so that key is retired from this role too.
inline constexpr std::uint64_t kRetiredExternalUnknownSerial =
    1'000'000'001ULL;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
retiredExternalUnknown() noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::merchant,
      ::PhantomLedger::entity::Bank::external, kRetiredExternalUnknownSerial);
}

static_assert(retiredExternalUnknown() !=
                  ::PhantomLedger::entity::makeKey(
                      ::PhantomLedger::entity::Role::merchant,
                      ::PhantomLedger::entity::Bank::external, 1ULL),
              "the retired catch-all never shared catalogue merchant 1's key");

// --- Enum-keyed lookup -------------------------------------------------

namespace detail {

template <class Enum> struct Tables;

template <> struct Tables<Government> {
  [[nodiscard]] static constexpr const auto &keys() noexcept {
    return kGovernment;
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
