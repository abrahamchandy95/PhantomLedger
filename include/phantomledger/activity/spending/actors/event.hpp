#pragma once

#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <span>

namespace PhantomLedger::activity::spending::actors {

struct Event {
  const Spender *spender = nullptr;
  time::TimePoint ts{};
  double exploreP = 0.0;
  double availableCash = 0.0;

  /* Per-CREDIT-SLOT available liquidity, index-aligned to
   * `Spender::instruments.credits()`. It is a span and not a scalar because
   * the credit instrument is chosen PER ROW from (person, merchant) while
   * this is read once per person-day: a scalar could only describe one card.
   * Changing the TYPE rather than adding a field is what makes the compiler
   * find both readers. */
  std::span<const double> cardAvailable{};
  double amountFactor = 1.0;
  // H1 step 2b (class P): the event day's CPI level multiplier
  // (synth/econ/nominal.hpp priceScale), computed ONCE per day frame
  // by the RateSampler — pure derived data, no draws. The payment
  // router multiplies it into every ticket draw.
  double priceScale = 1.0;
  // H2 step 2c: the retirement consumption step — kRetiredSpendScale
  // once the day frame reaches the spender's claiming day, 1.0 before
  // (and always 1.0 for exempt seeds). Pure derived data, no draws;
  // the payment router multiplies it alongside priceScale.
  double consumptionScale = 1.0;
};

} // namespace PhantomLedger::activity::spending::actors
