#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/detail/session.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::credit_cards {

struct DriverInputs {
  const entity::card::Registry *cards = nullptr;
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
  entity::Key issuerAccount{};
  time::Window window{};
};

class CardCycleDriver {
public:
  CardCycleDriver() = default;

  CardCycleDriver(const LifecycleRules &rules,
                  const transactions::Factory &factory,
                  random::RngFactory rngFactory, DriverInputs inputs,
                  ::PhantomLedger::clearing::Ledger *ledger);

  CardCycleDriver(const CardCycleDriver &) = delete;
  CardCycleDriver &operator=(const CardCycleDriver &) = delete;
  CardCycleDriver(CardCycleDriver &&) noexcept = default;
  CardCycleDriver &operator=(CardCycleDriver &&) noexcept = default;

  void ingestPurchases(std::span<const transactions::Transaction> txns);

  void tickDay(std::uint32_t dayIndex, time::TimePoint dayStart);

  void drainResidual();

  [[nodiscard]] std::vector<transactions::Transaction> takeEmitted();

  [[nodiscard]] bool active() const noexcept { return active_; }

private:
  struct PerCard {
    std::vector<transactions::Transaction> purchases;
    std::vector<std::uint32_t> indices;
    std::vector<time::TimePoint> closes;
    std::size_t closeCursor = 0;
    std::uint8_t cycleDay = 1;
    time::TimePoint priorClose{};
    bool sessionReady = false;
    std::unique_ptr<detail::Session> session;
  };

  void ensureSession(const entity::Key &cardKey, PerCard &card);
  void closeCyclesUpTo(PerCard &card, time::TimePoint hardLimit);

  const LifecycleRules *rules_ = nullptr;
  const transactions::Factory *factory_ = nullptr;
  std::uint64_t rngBase_ = 0;
  DriverInputs inputs_{};
  ::PhantomLedger::clearing::Ledger *ledger_ = nullptr;

  std::unique_ptr<detail::Environment> env_;
  std::unordered_map<entity::Key, PerCard> cards_;
  std::vector<transactions::Transaction> emitted_;
  bool active_ = false;
};

} // namespace PhantomLedger::transfers::credit_cards
