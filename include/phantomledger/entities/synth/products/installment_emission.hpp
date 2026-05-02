#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

struct DelinquencyKnobs {
  double lateP = 0.0;
  double missP = 0.0;
  double partialP = 0.0;
  double cureP = 0.0;
  double clusterMult = 1.0;

  std::int32_t lateDaysMin = 0;
  std::int32_t lateDaysMax = 0;

  double partialMinFrac = 0.0;
  double partialMaxFrac = 0.0;
};

template <class Delinquency>
[[nodiscard]] constexpr DelinquencyKnobs
delinquencyKnobs(const Delinquency &d) noexcept {
  return {
      .lateP = d.lateP,
      .missP = d.missP,
      .partialP = d.partialP,
      .cureP = d.cureP,
      .clusterMult = d.clusterMult,
      .lateDaysMin = d.lateDaysMin,
      .lateDaysMax = d.lateDaysMax,
      .partialMinFrac = d.partialMinFrac,
      .partialMaxFrac = d.partialMaxFrac,
  };
}

void addInstallmentProduct(
    ::PhantomLedger::entity::product::PortfolioRegistry &out,
    ::PhantomLedger::entity::PersonId person,
    ::PhantomLedger::entity::product::ProductType productType,
    ::PhantomLedger::entity::Key counterparty,
    ::PhantomLedger::time::TimePoint start, std::int32_t termMonths,
    std::int32_t paymentDay, double monthlyPayment,
    ::PhantomLedger::time::Window window, DelinquencyKnobs knobs);

} // namespace PhantomLedger::entities::synth::products
