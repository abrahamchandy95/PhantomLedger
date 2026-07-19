#pragma once

#include "phantomledger/entities/products/insurance_ledger.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/insurance/premiums.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"

#include <vector>

namespace PhantomLedger::transfers::insurance {

class ClaimScheduler {
public:
  ClaimScheduler(const ClaimRates &rates, random::Rng &timestampRng,
                 const transactions::Factory &txf,
                 const random::RngFactory &claimRngs) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           const entity::product::InsuranceLedger &insurance,
           const Population &population) const;

private:
  const ClaimRates *rates_ = nullptr;
  random::Rng *timestampRng_ = nullptr;
  const transactions::Factory *txf_ = nullptr;
  const random::RngFactory *claimRngs_ = nullptr;
};

} // namespace PhantomLedger::transfers::insurance
