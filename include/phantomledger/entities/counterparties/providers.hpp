#pragma once
//
// phantomledger/entities/counterparties/providers.hpp
//
// Key vocabulary for the per-contract provider pools
// (institutional-providers-2026-09): the mortgage servicers, auto lenders,
// student-loan servicers and the auto, home and life insurers a loan or policy
// pays. Each market owns one block of 99,999 serials above the institutional
// base, so a key says which market it belongs to without any lookup table:
//
//   serial = 1,000,000,000 + (market index + 1) * 100,000 + ordinal
//
// so mortgage ordinals start at 1,000,100,001, auto loan at 1,000,200,001 and
// so on up to life insurance at 1,000,600,001. The market-share tables that
// decide WHICH ordinal a contract gets live in synth/products/providers.hpp;
// this header is only the key layout.
//

#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/counterparties/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <cstdint>
#include <optional>

namespace PhantomLedger::counterparties::providers {

inline constexpr std::uint64_t kBaseSerial = 1'000'000'000ULL;
inline constexpr std::uint64_t kBlockSize = 100'000ULL;
inline constexpr std::uint32_t kMaxOrdinal = 99'999U;

[[nodiscard]] constexpr ::PhantomLedger::entity::Key
key(Market market, std::uint32_t ordinal) noexcept {
  const auto block = static_cast<std::uint64_t>(enumTax::toIndex(market)) + 1U;
  return ::PhantomLedger::entity::makeKey(
      ::PhantomLedger::entity::Role::business,
      ::PhantomLedger::entity::Bank::external,
      kBaseSerial + block * kBlockSize + ordinal);
}

[[nodiscard]] constexpr std::optional<Market>
marketOf(::PhantomLedger::entity::Key account) noexcept {
  if (account.role != ::PhantomLedger::entity::Role::business ||
      account.bank != ::PhantomLedger::entity::Bank::external ||
      account.number <= kBaseSerial + kBlockSize) {
    return std::nullopt;
  }
  const auto offset = account.number - kBaseSerial;
  const auto block = offset / kBlockSize;
  const auto ordinal = offset % kBlockSize;
  if (ordinal == 0 || block == 0 || block > kMarketCount) {
    return std::nullopt;
  }
  return kMarkets[block - 1U];
}

[[nodiscard]] constexpr bool
isProvider(::PhantomLedger::entity::Key account) noexcept {
  return marketOf(account).has_value();
}

// The provider blocks sit strictly between the institutional singletons
// (1,000,000,001..1,000,000,007, of which only 4 is still live) and the
// biller range at 2e9, so no key another pool mints can land in one. The
// population-scaled business pools start at serial 1 and would need a
// population of 5e10 to reach the first block.
static_assert(key(Market::mortgage, 1).number > kBaseSerial + 7U);
static_assert(key(Market::lifeInsurance, kMaxOrdinal).number <
              cash::kBillerBaseSerial);
static_assert(marketOf(key(Market::studentLoan, 1)) == Market::studentLoan);
static_assert(marketOf(key(Market::lifeInsurance, kMaxOrdinal)) ==
              Market::lifeInsurance);
static_assert(!isProvider(counterparties::key(Tax::irsTreasury)));
static_assert(!isProvider(retiredExternalUnknown()));

} // namespace PhantomLedger::counterparties::providers
