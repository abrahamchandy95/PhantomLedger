#pragma once

#include <cstdint>

namespace PhantomLedger::spending::simulator {

struct TransactionLoad {
  double txnsPerMonth = 0.0;
  std::uint32_t personDailyLimit = 0; // 0 = no cap
};

struct PaymentRouting {
  double preferBillersP = 0.85;
  std::uint16_t merchantRetryLimit = 6;
};

struct ExploreRate {
  double baseExploreP = 0.0;
};

struct DayVariation {
  double shockShape = 1.0;
};

struct WeekendExplore {
  double multiplier = 1.25;
};

struct BurstSpend {
  double multiplier = 3.25;
};

struct PayeePicking {
  std::uint16_t maxPickAttempts = 250;
};

struct ExplorePropensity {
  double alpha = 1.6;
  double beta = 9.5;
};

struct BurstWindow {
  double probability = 0.08;
  std::uint16_t minDays = 3;
  std::uint16_t maxDays = 9;
};
} // namespace PhantomLedger::spending::simulator
