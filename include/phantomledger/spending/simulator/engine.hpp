#pragma once

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

struct EmissionThreads {
  random::RngFactory *rngFactory = nullptr;
  std::uint32_t count = 1;

  [[nodiscard]] bool parallel() const noexcept { return count > 1; }
};

class RunResources {
public:
  RunResources(random::Rng &rng, const transactions::Factory &factory,
               clearing::Ledger *ledger = nullptr,
               EmissionThreads threads = {}) noexcept
      : rng_(rng), factory_(factory), ledger_(ledger), threads_(threads) {}

  [[nodiscard]] random::Rng &rng() const noexcept { return rng_; }

  [[nodiscard]] const transactions::Factory &factory() const noexcept {
    return factory_;
  }

  [[nodiscard]] clearing::Ledger *ledger() const noexcept { return ledger_; }

  [[nodiscard]] const EmissionThreads &threads() const noexcept {
    return threads_;
  }

private:
  random::Rng &rng_;
  const transactions::Factory &factory_;
  clearing::Ledger *ledger_ = nullptr;
  EmissionThreads threads_{};
};

} // namespace PhantomLedger::spending::simulator
