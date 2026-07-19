#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

// The billing half of the revolving-credit cycle, as the spending
// simulator sees it: the simulator feeds each day's purchases in, ticks
// the day boundary, and takes back whatever billing rows (statement
// payments, interest, disputes) the cycles emitted. The concrete engine
// is transfers/channels/credit_cards/card_cycle_driver.hpp, which
// implements this interface; the wiring sites live on the transfers side
// (legit/routines/spending.cpp, legit/routines/spending_session.cpp), so
// the dependency arrow points transfers -> activity only.
class CardCycleBilling {
public:
  virtual ~CardCycleBilling() = default;

  // False when card lifecycle generation is disabled; every other call
  // requires active() to be true.
  [[nodiscard]] virtual bool active() const noexcept = 0;

  virtual void
  ingestPurchases(std::span<const transactions::Transaction> txns) = 0;

  virtual void tickDay(std::uint32_t dayIndex, time::TimePoint dayStart) = 0;

  // Closes every remaining cycle at end of run.
  virtual void drainResidual() = 0;

  [[nodiscard]] virtual std::vector<transactions::Transaction>
  takeEmitted() = 0;

protected:
  // Copy/move stay protected: only concrete implementations may be
  // sliced-free copied or moved; consumers hold the interface by pointer.
  CardCycleBilling() = default;
  CardCycleBilling(const CardCycleBilling &) = default;
  CardCycleBilling &operator=(const CardCycleBilling &) = default;
  CardCycleBilling(CardCycleBilling &&) noexcept = default;
  CardCycleBilling &operator=(CardCycleBilling &&) noexcept = default;
};

} // namespace PhantomLedger::activity::spending::simulator
