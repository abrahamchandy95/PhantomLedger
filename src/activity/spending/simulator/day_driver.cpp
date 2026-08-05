#include "phantomledger/activity/spending/simulator/day_driver.hpp"

#include "phantomledger/transactions/clearing/screening.hpp"

#include <stdexcept>
#include <utility>

namespace PhantomLedger::activity::spending::simulator {

DayDriver::DayDriver(DaySource days, CommerceEvolver commerce,
                     PopulationDynamics dynamics,
                     SpenderEmissionDriver emission)
    : days_(std::move(days)), commerce_(std::move(commerce)),
      dynamics_(std::move(dynamics)), emission_(std::move(emission)) {}

void DayDriver::resetFor(const market::Market &market) {
  dynamics_.resetFor(market);
  cardIngestCursor_ = 0;
}

void DayDriver::bindMarket(market::Market &value) noexcept {
  market_ = &value;
  rebindEmitter();
}

void DayDriver::bindRng(random::Rng &value) noexcept {
  rng_ = &value;
  rebindEmitter();
}

void DayDriver::bindLedger(clearing::Ledger *value) noexcept {
  ledger_ = value;
  rebindEmitter();
}

void DayDriver::bindFactory(const transactions::Factory &value) noexcept {
  factory_ = &value;
  rebindEmitter();
}

void DayDriver::bindCardCycleBilling(CardCycleBilling *cards) noexcept {
  cards_ = cards;
}

void DayDriver::rebindEmitter() noexcept {
  if (market_ == nullptr || rng_ == nullptr || factory_ == nullptr) {
    return;
  }

  emission_.bindMarket(*market_)
      .bindRng(*rng_)
      .bindFactory(*factory_)
      .bindLedger(ledger_);
}

void DayDriver::emissionThreads(
    SpenderEmissionDriver::Threads threads) noexcept {
  emission_.threads(threads);
}

void DayDriver::prepareEmission(double txnsPerMonth) {
  emission_.prepare(txnsPerMonth);
}

void DayDriver::bindEmission(const PreparedRun::Budget &budget,
                             const PreparedRun::Routing &routing) noexcept {
  emission_.bindRun(budget, routing);
}

std::span<const double> DayDriver::sensitivities() const noexcept {
  return dynamics_.sensitivities();
}

market::Market &DayDriver::boundMarket() const {
  if (market_ == nullptr) {
    throw std::logic_error("DayDriver requires bindMarket before runDay");
  }

  return *market_;
}

random::Rng &DayDriver::boundRng() const {
  if (rng_ == nullptr) {
    throw std::logic_error("DayDriver requires bindRng before runDay");
  }

  return *rng_;
}

void DayDriver::runDay(const PreparedRun &run, RunState &state,
                       std::uint32_t dayIndex) {
  auto &market = boundMarket();
  auto &rng = boundRng();

  commerce_.evolveIfNeeded(market, rng, dayIndex);

  const auto frame = days_.build(market.bounds(), rng, dayIndex);

  advanceLedgerToDay(run.ledgerReplay(), state, frame);

  const auto paydayPersons = updatePaydayState(run.paydays(), state, dayIndex);

  dynamics_.advance(rng, paydayPersons);

  emission_.emitDay(run.population(), state, frame,
                    dynamics_.dailyMultipliers());

  if (cards_ != nullptr && cards_->active()) {
    ingestEmittedTo(cards_, state);
    const auto nextDayStart = frame.day.start + time::Days{1};

    // The spending engine screens one day at a time. Apply that day's
    // previously scheduled lifecycle rows only after spending has finished,
    // so a noon payment can never fund or suppress an earlier transaction.
    // Do this before statement close so the statement sees only payments
    // the ledger actually accepted.
    cards_->advanceLedgerTo(nextDayStart);
    cards_->tickDay(dayIndex, frame.day.start);

    // A 23:30 close may enqueue 23:45 interest. Apply that same-day posting
    // after the close; future payments and fees remain queued.
    cards_->advanceLedgerTo(nextDayStart);
  }
}

void DayDriver::ingestEmittedTo(CardCycleBilling *cards,
                                const RunState &state) {
  const auto &txns = state.txns();

  if (cardIngestCursor_ > txns.size()) {
    throw std::logic_error(
        "DayDriver card-ingest cursor exceeds emitted transaction count");
  }

  if (cardIngestCursor_ == txns.size()) {
    return;
  }

  const auto newTxns = std::span<const transactions::Transaction>(
      txns.data() + cardIngestCursor_, txns.size() - cardIngestCursor_);

  cards->ingestPurchases(newTxns);
  cardIngestCursor_ = txns.size();
}

std::vector<transactions::Transaction> DayDriver::takeEmitted(RunState &state) {
  if (cards_ != nullptr && cards_->active()) {
    ingestEmittedTo(cards_, state);

    if (cardIngestCursor_ != state.txns().size()) {
      throw std::logic_error(
          "DayDriver card-ingest cursor does not cover emitted rows");
    }
  } else if (cardIngestCursor_ != 0) {
    throw std::logic_error(
        "DayDriver has a card-ingest cursor without an active card driver");
  }

  auto out = std::move(state.txns());
  state.txns().clear();
  cardIngestCursor_ = 0;
  return out;
}

void DayDriver::advanceLedgerToDay(const PreparedRun::LedgerReplay &replay,
                                   RunState &state,
                                   const actors::DayFrame &frame) const {
  // RAM R2.4b-1: the replay feed is a seam (replay_source.hpp); the
  // resident-span adapter makes the exact advanceBookThrough call this
  // site always made. The cursor position stays caller-owned.
  const auto newBaseIdx = replay.resolved().postThrough(
      ledger_, state.baseIdx(),
      clearing::TimeBound{.until = time::toEpochSeconds(frame.day.start),
                          .inclusive = false});

  state.setBaseIdx(newBaseIdx);
}

std::span<const std::uint32_t>
DayDriver::updatePaydayState(const PreparedRun::Paydays &paydays,
                             RunState &state, std::uint32_t dayIndex) const {
  state.bumpAllDaysSincePayday();

  const auto paydayPersons = paydays.index.personsOn(dayIndex);
  for (const auto idx : paydayPersons) {
    state.resetDaysSincePayday(idx);
  }

  return paydayPersons;
}

void DayDriver::finish(RunState &state) { emission_.finish(state); }

std::vector<transactions::Transaction> DayDriver::drainCardCycles() {
  if (cards_ == nullptr || !cards_->active()) {
    return {};
  }

  cards_->drainResidual();
  return cards_->takeEmitted();
}

} // namespace PhantomLedger::activity::spending::simulator
