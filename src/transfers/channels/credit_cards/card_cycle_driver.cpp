#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::transfers::credit_cards {
namespace {

struct KeyText {
  std::array<char, 24> buf{};
  std::size_t len{};

  explicit KeyText(std::uint64_t value) noexcept {
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    (void)ec;
    len = static_cast<std::size_t>(ptr - buf.data());
  }

  [[nodiscard]] std::string_view view() const noexcept {
    return {buf.data(), len};
  }
};

[[nodiscard]] ::PhantomLedger::clearing::Ledger::Index
indexOf(::PhantomLedger::clearing::Ledger *ledger,
        const entity::Key &key) noexcept {
  if (ledger == nullptr || !entity::valid(key)) {
    return ::PhantomLedger::clearing::Ledger::invalid;
  }
  return ledger->findAccount(key);
}

} // namespace

CardCycleDriver::CardCycleDriver(const LifecycleRules &rules,
                                 const transactions::Factory &factory,
                                 random::RngFactory rngFactory,
                                 DriverInputs inputs,
                                 ::PhantomLedger::clearing::Ledger *ledger)
    : rules_(&rules), factory_(&factory), rngBase_(rngFactory.base()),
      inputs_(inputs), ledger_(ledger),
      active_(inputs.cards != nullptr && !inputs.cards->records.empty() &&
              inputs.primaryAccounts != nullptr && inputs.window.days > 0) {
  if (active_) {
    env_ = std::make_unique<detail::Environment>(
        detail::Environment{rules_->billing, rules_->payments, rules_->disputes,
                            *factory_, inputs_.issuerAccount});
  }
}

void CardCycleDriver::ingestPurchases(
    std::span<const transactions::Transaction> txns) {
  if (!active_) {
    return;
  }

  const auto purchaseTag = channels::tag(channels::Legit::cardPurchase);
  const std::int64_t startEpoch = time::toEpochSeconds(inputs_.window.start);
  const std::int64_t endEpoch = time::toEpochSeconds(inputs_.window.endExcl());

  std::unordered_map<entity::Key, std::size_t> beforeSize;

  for (const auto &txn : txns) {
    if (txn.session.channel != purchaseTag) {
      continue;
    }
    if (txn.timestamp < startEpoch || txn.timestamp >= endEpoch) {
      continue;
    }

    auto &slot = cards_[txn.source];
    if (beforeSize.find(txn.source) == beforeSize.end()) {
      beforeSize[txn.source] = slot.indices.size();
    }
    slot.indices.push_back(static_cast<std::uint32_t>(slot.purchases.size()));
    slot.purchases.push_back(txn);
  }

  for (auto &entry : beforeSize) {
    auto it = cards_.find(entry.first);
    if (it == cards_.end()) {
      continue;
    }
    auto &slot = it->second;
    const auto first =
        slot.indices.begin() + static_cast<std::ptrdiff_t>(entry.second);
    std::sort(
        first, slot.indices.end(), [&slot](std::uint32_t a, std::uint32_t b) {
          return slot.purchases[a].timestamp < slot.purchases[b].timestamp;
        });
  }
}

void CardCycleDriver::ensureSession(const entity::Key &cardKey, PerCard &card) {
  if (card.sessionReady) {
    return;
  }

  const auto *cardTerms = inputs_.cards->forKey(cardKey);
  if (cardTerms == nullptr) {
    return;
  }

  const auto it = inputs_.primaryAccounts->find(cardTerms->owner);
  if (it == inputs_.primaryAccounts->end()) {
    return;
  }

  detail::Account account{
      .card = cardTerms->key,
      .funding = it->second,
      .apr = cardTerms->apr,
      .cycleDay = cardTerms->cycleDay,
      .autopay = cardTerms->autopay,
  };

  detail::LedgerBinding binding{
      .ledger = ledger_,
      .cardIdx = indexOf(ledger_, account.card),
      .fundingIdx = indexOf(ledger_, account.funding),
      .issuerIdx = indexOf(ledger_, inputs_.issuerAccount),
  };

  card.cycleDay = cardTerms->cycleDay;
  card.priorClose = inputs_.window.start;
  card.closes = statementCloseDates(inputs_.window.start,
                                    inputs_.window.endExcl(), card.cycleDay);
  card.closeCursor = 0;

  const KeyText keyText(account.card.number);
  auto rng = random::RngFactory{rngBase_}.rng(
      {"credit_cards", "lifecycle", keyText.view()});

  card.session = std::make_unique<detail::Session>(
      *env_, account, std::move(rng), emitted_, binding);
  card.sessionReady = true;
}

void CardCycleDriver::closeCyclesUpTo(PerCard &card,
                                      time::TimePoint hardLimit) {
  if (!card.sessionReady || card.session == nullptr) {
    return;
  }

  const auto windowEndExcl = inputs_.window.endExcl();
  const auto limit = std::min(hardLimit, windowEndExcl);

  while (card.closeCursor < card.closes.size()) {
    const auto close = card.closes[card.closeCursor];
    if (close >= limit) {
      return;
    }

    const detail::CardPurchases purchases{
        .txns = std::span<const transactions::Transaction>(card.purchases),
        .indices = std::span<const std::uint32_t>(card.indices),
    };

    card.session->run(purchases, {card.priorClose, close, windowEndExcl});

    card.priorClose = close;
    ++card.closeCursor;
  }
}

void CardCycleDriver::tickDay(std::uint32_t /*dayIndex*/,
                              time::TimePoint dayStart) {
  if (!active_) {
    return;
  }

  const time::TimePoint nextDayStart = dayStart + time::Days{1};

  for (auto &entry : cards_) {
    ensureSession(entry.first, entry.second);
    closeCyclesUpTo(entry.second, nextDayStart);
  }
}

void CardCycleDriver::drainResidual() {
  if (!active_) {
    return;
  }

  for (auto &entry : cards_) {
    ensureSession(entry.first, entry.second);
    closeCyclesUpTo(entry.second, inputs_.window.endExcl());
  }
}

std::vector<transactions::Transaction> CardCycleDriver::takeEmitted() {
  std::sort(
      emitted_.begin(), emitted_.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return std::move(emitted_);
}

} // namespace PhantomLedger::transfers::credit_cards
