#pragma once
//
// phantomledger/entities/counterparties/remote_payees.hpp
//
// Key vocabulary for the counterparties that replaced the external-unknown
// catch-all (unknown-counterparty-2026-09). Three pools, one per channel of
// the retired flow:
//
//   * CHECK PAYEE BANKS. A paid check carries only the bank-of-first-deposit
//     routing number as a structured payee key, so a check to an unknown
//     payee is keyed by the payee's BANK: one account per external bank,
//     Role::business, serials 1,001,000,001 .. 1,001,000,000 + pool rank.
//   * P2P PLATFORMS. A P2P payment with no usable contact cannot ride Zelle
//     (it needs an enrolled email or US mobile number), so it goes through a
//     named app the bank sees as an ACH counterparty: Role::platform, serials
//     1,000,000,001 (Venmo) and 1,000,000,002 (Cash App).
//   * FUNERAL HOMES (MCC 7261), placed by city: Role::merchant, serial
//     1,100,000,000 + area * 10,000 + ordinal within the area.
//
// Which bank, platform or home a row pays is decided draw-free in
// synth/counterparties/remote_payees.hpp; this header is only the layout.
//

#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/counterparties/providers.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>

namespace PhantomLedger::counterparties::remote {

// --- Check payee banks -------------------------------------------------

inline constexpr std::uint64_t kCheckBankBaseSerial = 1'001'000'000ULL;
inline constexpr std::uint32_t kMaxCheckBankRank = 99'999U;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
checkBank(std::uint32_t rank) noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external, kCheckBankBaseSerial + rank);
}

// The 1-based pool rank of a check-bank key, or 0 when the key is not one.
[[nodiscard]] constexpr std::uint32_t
checkBankRank(::PhantomLedger::entity::Key account) noexcept {
  if (account.role != ::PhantomLedger::entity::Role::business ||
      account.bank != ::PhantomLedger::entity::Bank::external ||
      account.number <= kCheckBankBaseSerial ||
      account.number > kCheckBankBaseSerial + kMaxCheckBankRank) {
    return 0;
  }
  return static_cast<std::uint32_t>(account.number - kCheckBankBaseSerial);
}

[[nodiscard]] constexpr bool
isCheckBank(::PhantomLedger::entity::Key account) noexcept {
  return checkBankRank(account) != 0;
}

// --- P2P platforms -----------------------------------------------------

enum class P2pPlatform : std::uint8_t {
  venmo = 1,
  cashApp = 2,
};

inline constexpr std::uint32_t kP2pPlatformCount = 2;
inline constexpr std::uint64_t kP2pPlatformBaseSerial = 1'000'000'000ULL;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
p2pPlatform(P2pPlatform platform) noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::platform,
      ::PhantomLedger::entity::Bank::external,
      kP2pPlatformBaseSerial + static_cast<std::uint64_t>(platform));
}

[[nodiscard]] constexpr bool
isP2pPlatform(::PhantomLedger::entity::Key account) noexcept {
  return account.role == ::PhantomLedger::entity::Role::platform &&
         account.bank == ::PhantomLedger::entity::Bank::external &&
         account.number > kP2pPlatformBaseSerial &&
         account.number <= kP2pPlatformBaseSerial + kP2pPlatformCount;
}

// --- Funeral homes -----------------------------------------------------

inline constexpr std::uint64_t kFuneralHomeBaseSerial = 1'100'000'000ULL;
inline constexpr std::uint64_t kFuneralHomeAreaStride = 10'000ULL;
inline constexpr std::uint32_t kMaxFuneralHomesPerArea = 9'999U;
// Area 0 (no home area) is a valid block: a hand-built harness whose people
// carry no home resolves it, and registration resolves it the same way.
inline constexpr std::uint64_t kMaxFuneralHomeArea = 89'999ULL;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
funeralHome(::PhantomLedger::entity::geography::GeoAreaId area,
            std::uint32_t ordinal) noexcept {
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::merchant,
      ::PhantomLedger::entity::Bank::external,
      kFuneralHomeBaseSerial +
          static_cast<std::uint64_t>(area) * kFuneralHomeAreaStride + ordinal);
}

[[nodiscard]] constexpr bool
isFuneralHome(::PhantomLedger::entity::Key account) noexcept {
  if (account.role != ::PhantomLedger::entity::Role::merchant ||
      account.bank != ::PhantomLedger::entity::Bank::external ||
      account.number <= kFuneralHomeBaseSerial) {
    return false;
  }
  const auto offset = account.number - kFuneralHomeBaseSerial;
  const auto ordinal = offset % kFuneralHomeAreaStride;
  return ordinal != 0 && offset / kFuneralHomeAreaStride <= kMaxFuneralHomeArea;
}

// The geo area a funeral-home key sits in.
[[nodiscard]] constexpr ::PhantomLedger::entity::geography::GeoAreaId
funeralHomeArea(::PhantomLedger::entity::Key account) noexcept {
  return static_cast<::PhantomLedger::entity::geography::GeoAreaId>(
      (account.number - kFuneralHomeBaseSerial) / kFuneralHomeAreaStride);
}

// --- Any of the three --------------------------------------------------

[[nodiscard]] constexpr bool
isRemotePayee(::PhantomLedger::entity::Key account) noexcept {
  return isCheckBank(account) || isP2pPlatform(account) ||
         isFuneralHome(account);
}

// The pools sit clear of every other key block. Catalogue merchants and the
// population-scaled business and platform pools start at serial 1 and would
// need a population in the billions to reach 1e9; the retired catch-all is
// 1,000,000,001 in the merchant role, below the first funeral-home block; the
// provider blocks end at 1,000,699,999 and the billers start at 2e9.
static_assert(!isFuneralHome(retiredExternalUnknown()));
static_assert(
    checkBank(1).number >
    providers::key(Market::lifeInsurance, providers::kMaxOrdinal).number);
static_assert(!providers::isProvider(checkBank(1)));
static_assert(!providers::isProvider(checkBank(kMaxCheckBankRank)));
static_assert(checkBank(kMaxCheckBankRank).number < cash::kBillerBaseSerial);
static_assert(
    funeralHome(static_cast<::PhantomLedger::entity::geography::GeoAreaId>(
                    kMaxFuneralHomeArea),
                kMaxFuneralHomesPerArea)
        .number < cash::kAtmTerminalBaseSerial);
static_assert(funeralHomeArea(funeralHome(86, 12)) == 86);
static_assert(isP2pPlatform(p2pPlatform(P2pPlatform::cashApp)));
static_assert(!isP2pPlatform(cash::cryptoVenue(1)));

} // namespace PhantomLedger::counterparties::remote
