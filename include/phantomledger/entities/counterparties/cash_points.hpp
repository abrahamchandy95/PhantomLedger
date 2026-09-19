#pragma once

/*
  Cash-service endpoints in the customer-transaction projection.

  These keys identify observable ATM/acceptor and cash-depository locations;
  they are NOT balance-bearing general-ledger accounts.  Every key is external
  to the modeled customer ledger, so clearing mutates only the internal leg.
  The centralized ATM-cash/network-settlement GL remains outside this
  projection instead of being impersonated by a customer deposit account.

  The high serial ranges keep these endpoints disjoint from the ordinary
  population-scaled processor/business pools, whose serials start at one.
*/

#include "phantomledger/entities/identifiers.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::counterparties::cash {

inline constexpr std::uint64_t kAtmTerminalBaseSerial = 2'000'000'000ULL;
inline constexpr std::uint64_t kDepositoryBaseSerial = 3'000'000'000ULL;
inline constexpr std::uint64_t kCheckCaptureBaseSerial = 4'000'000'000ULL;
inline constexpr std::uint64_t kCryptoVenueBaseSerial = 4'000'000'000ULL;
inline constexpr std::uint64_t kBillerBaseSerial = 2'000'000'000ULL;

inline constexpr std::uint64_t kIssuerSerial = 3'000'000'001ULL;
inline constexpr std::uint64_t kFallbackEmployerSerial = 2'000'000'001ULL;
inline constexpr std::uint64_t kFallbackLandlordSerial = 2'000'000'001ULL;

[[nodiscard]] constexpr entity::Key
atmTerminal(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kAtmTerminalBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
depository(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kDepositoryBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
checkCapture(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kCheckCaptureBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
cryptoVenue(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::platform, entity::Bank::external,
                         kCryptoVenueBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
biller(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         kBillerBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key cardIssuer() noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         kIssuerSerial);
}

[[nodiscard]] constexpr entity::Key fallbackEmployer() noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         kFallbackEmployerSerial);
}

[[nodiscard]] constexpr entity::Key fallbackLandlord() noexcept {
  return entity::makeKey(entity::Role::landlord, entity::Bank::external,
                         kFallbackLandlordSerial);
}

namespace detail {

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

inline constexpr std::uint64_t kHomeTerminalDomain = 0x4154'4D48'4F4D'4501ULL;
inline constexpr std::uint64_t kVisitTerminalDomain = 0x4154'4D56'4953'4954ULL;
inline constexpr std::uint64_t kDepositoryDomain = 0x4341'5348'494E'0001ULL;
inline constexpr std::uint64_t kCheckCaptureDomain = 0x4348'4543'4B49'4E01ULL;
inline constexpr std::uint64_t kCryptoVenueDomain = 0x4352'5950'544F'0001ULL;

[[nodiscard]] constexpr std::uint64_t accountWord(entity::Key account) {
  return splitmix(account.number) ^
         splitmix(static_cast<std::uint64_t>(account.role) << 8U |
                  static_cast<std::uint64_t>(account.bank));
}

} // namespace detail

/*
  Draw-free terminal selection.  A customer has a stable primary terminal;
  82% of visits use it and the remainder use one of at most three neighboring
  service points.  This produces the repeated-location behavior of real cash
  access without making transaction order, thread count, or RNG state part of
  endpoint identity.
*/
[[nodiscard]] inline entity::Key
terminalFor(std::span<const entity::Key> terminals, entity::Key account,
            std::int64_t timestamp) noexcept {
  if (terminals.empty()) {
    return {};
  }
  if (terminals.size() == 1U) {
    return terminals.front();
  }

  const auto accountMix = detail::accountWord(account);
  const auto homeMix =
      detail::splitmix(accountMix ^ detail::kHomeTerminalDomain);
  const auto home = static_cast<std::size_t>(homeMix % terminals.size());

  const auto visitMix =
      detail::splitmix(accountMix ^ static_cast<std::uint64_t>(timestamp) ^
                       detail::kVisitTerminalDomain);
  if (visitMix % 100U < 82U) {
    return terminals[home];
  }

  const auto neighborCount = std::min<std::size_t>(3U, terminals.size() - 1U);
  const auto offset =
      1U + static_cast<std::size_t>((visitMix >> 8U) % neighborCount);
  return terminals[(home + offset) % terminals.size()];
}

/* A business keeps a stable cash-deposit location. */
[[nodiscard]] inline entity::Key
depositoryFor(std::span<const entity::Key> depositories,
              entity::Key account) noexcept {
  if (depositories.empty()) {
    return {};
  }
  const auto mixed = detail::splitmix(detail::accountWord(account) ^
                                      detail::kDepositoryDomain);
  return depositories[static_cast<std::size_t>(mixed % depositories.size())];
}

/* A customer keeps stable check-capture and crypto-service relationships.
 * These select an observed service context, not a balance-bearing account. */
[[nodiscard]] inline entity::Key
checkCaptureFor(std::span<const entity::Key> captures,
                entity::Key account) noexcept {
  if (captures.empty()) {
    return {};
  }
  const auto mixed = detail::splitmix(detail::accountWord(account) ^
                                      detail::kCheckCaptureDomain);
  return captures[static_cast<std::size_t>(mixed % captures.size())];
}

[[nodiscard]] inline entity::Key
cryptoVenueFor(std::span<const entity::Key> venues,
               entity::Key account) noexcept {
  if (venues.empty()) {
    return {};
  }
  const auto mixed = detail::splitmix(detail::accountWord(account) ^
                                      detail::kCryptoVenueDomain);
  return venues[static_cast<std::size_t>(mixed % venues.size())];
}

} // namespace PhantomLedger::counterparties::cash
