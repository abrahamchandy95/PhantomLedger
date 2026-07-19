#pragma once

#include <cstdint>

namespace PhantomLedger::entity::product {

struct InstallmentTerms {
  double lateP = 0.0;

  std::int32_t lateDaysMin = 0;
  std::int32_t lateDaysMax = 0;

  double missP = 0.0;

  double partialP = 0.0;

  double cureP = 0.0;

  double partialMinFrac = 0.0;
  double partialMaxFrac = 0.0;

  double clusterMult = 1.0;
};

} // namespace PhantomLedger::entity::product
