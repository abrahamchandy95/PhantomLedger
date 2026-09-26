#pragma once
//
// phantomledger/entities/holdings/general_ledger.hpp
//
// The bank's own income ledgers (bank-gl-2026-09). A fee or interest charge
// is a double-entry posting: the customer's account is debited and an income
// GL the bank owns is credited (FLEXCUBE CHG_BOOK debit / CHG_INCOME credit
// for charges, ICDB-BOOK / ICDB-PNL for debit interest). The contra is
// neither a customer nor an external party, so it has its own role,
// `Role::ledger`, internal only and rendered `GL` plus 8 digits.
//
// One GL per posting kind:
//
//   GL00000001  card interest income         cc_interest
//   GL00000002  card fee income              cc_late_fee
//   GL00000003  deposit fee income           overdraft_fee
//   GL00000004  credit-line interest income  loc_interest
//
// The GLs are registered ownerless and internal, so every clearing book gives
// them a slot; nothing seeds them and nothing debits them, so a GL's balance
// is the income booked to it. They are never a fraud, mule, victim or
// camouflage account.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::entity::gl {

enum class Income : std::uint8_t {
  cardInterest = 1,
  cardFees = 2,
  depositFees = 3,
  creditLineInterest = 4,
};

[[nodiscard]] constexpr Key account(Income kind) noexcept {
  return makeKey(Role::ledger, Bank::internal,
                 static_cast<std::uint64_t>(kind));
}

inline constexpr std::array<Key, 4> kIncomeAccounts{
    account(Income::cardInterest),
    account(Income::cardFees),
    account(Income::depositFees),
    account(Income::creditLineInterest),
};

[[nodiscard]] constexpr bool isGeneralLedger(Key key) noexcept {
  return key.role == Role::ledger;
}

// True for the four bank-originated posting channels.
[[nodiscard]] constexpr bool isPosting(channels::Tag channel) noexcept {
  return channels::is(channel, channels::Credit::interest) ||
         channels::is(channel, channels::Credit::lateFee) ||
         channels::is(channel, channels::Liquidity::overdraftFee) ||
         channels::is(channel, channels::Liquidity::locInterest);
}

// The income GL a posting of this channel credits, or an invalid key when the
// channel is not a bank-originated posting.
[[nodiscard]] constexpr Key incomeAccountFor(channels::Tag channel) noexcept {
  if (channels::is(channel, channels::Credit::interest)) {
    return account(Income::cardInterest);
  }
  if (channels::is(channel, channels::Credit::lateFee)) {
    return account(Income::cardFees);
  }
  if (channels::is(channel, channels::Liquidity::overdraftFee)) {
    return account(Income::depositFees);
  }
  if (channels::is(channel, channels::Liquidity::locInterest)) {
    return account(Income::creditLineInterest);
  }
  return Key{};
}

} // namespace PhantomLedger::entity::gl
