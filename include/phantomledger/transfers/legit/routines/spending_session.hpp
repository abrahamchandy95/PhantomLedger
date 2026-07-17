#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/simulator/session.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace plSimulator = ::PhantomLedger::activity::spending::simulator;

namespace plMarketNs = ::PhantomLedger::activity::spending::market;

namespace plObligationsNs = ::PhantomLedger::activity::spending::obligations;

namespace plCreditNs = ::PhantomLedger::transfers::credit_cards;

using CardLifecycleConfig = SpendingRoutine::CardLifecycleConfig;

struct SessionInputs {
  RunPlanning planning{};
  DayPattern day{};
  DynamicsProfile dynamics{};
  EmissionProfile emission{};
  CardLifecycleConfig cardLifecycle{};

  std::optional<std::uint32_t> threadCount{};
};

class SessionBundle {
public:
  [[nodiscard]] static std::unique_ptr<SessionBundle>
  make(std::uint64_t seed, random::Rng &rng, const transactions::Factory &txf,
       plMarketNs::Market &market, const plObligationsNs::Snapshot &obligations,
       clearing::Ledger *screenBook, SessionInputs inputs);

  SessionBundle(const SessionBundle &) = delete;
  SessionBundle &operator=(const SessionBundle &) = delete;
  SessionBundle(SessionBundle &&) = delete;
  SessionBundle &operator=(SessionBundle &&) = delete;

  [[nodiscard]] plSimulator::Session &session() noexcept { return *session_; }

  [[nodiscard]] const plSimulator::Session &session() const noexcept {
    return *session_;
  }

  // Null when credit-card lifecycle generation is inactive.
  [[nodiscard]] plCreditNs::CardCycleDriver *cardDriver() noexcept {
    return cardDriver_.get();
  }

  [[nodiscard]] const plCreditNs::CardCycleDriver *cardDriver() const noexcept {
    return cardDriver_.get();
  }

private:
  explicit SessionBundle(std::uint64_t seed) : rngFactory_(seed) {}

  random::RngFactory rngFactory_;
  SessionInputs inputs_{};
  std::unique_ptr<plCreditNs::CardCycleDriver> cardDriver_;
  std::unique_ptr<plSimulator::Session> session_;
};

} // namespace PhantomLedger::transfers::legit::routines::spending
