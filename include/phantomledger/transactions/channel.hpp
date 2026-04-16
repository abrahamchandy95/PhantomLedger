#pragma once

#include <cstdint>

namespace PhantomLedger::transactions {

enum class Channel : std::uint8_t {
  unknown = 0,
  selfTransfer,
  p2p,
  billPay,
  cardPurchase,
  cashWithdrawal,
  salary,
};

} // namespace PhantomLedger::transactions
