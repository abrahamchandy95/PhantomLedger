#pragma once

namespace PhantomLedger::entities::behavior {

struct Cash {
  double rateMultiplier = 1.0;
  double amountMultiplier = 1.0;
  double initialBalance = 1200.0;
};

} // namespace PhantomLedger::entities::behavior
