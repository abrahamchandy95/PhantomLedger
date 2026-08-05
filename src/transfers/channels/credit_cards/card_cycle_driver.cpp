#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"

#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr std::size_t kPurchaseCompactionThreshold = 256;

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

[[nodiscard]] transactions::Comparator chronologicalComparator() noexcept {
  return transactions::Comparator{
      transactions::Comparator::Scope::fundsTransfer};
}

// H3 part 3c-ii: the owner's card-servicing stop — ACCOUNT CLOSURE
// (death + settlement) minus the session tail guard (see the header's
// kCardSettleTailDays note). TimePoint max when the carrier is absent.
[[nodiscard]] time::TimePoint cardServiceEnd(const DriverInputs &inputs,
                                             entity::PersonId owner) noexcept {
  if (inputs.timelines.empty() || owner == 0 ||
      static_cast<std::size_t>(owner) > inputs.timelines.size()) {
    return time::TimePoint::max();
  }
  const auto death = inputs.timelines[owner - 1].death;
  return time::addDays(death, ::PhantomLedger::synth::pii::kSettlementDays -
                                  kCardSettleTailDays);
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
    env_ = std::make_unique<detail::Environment>(detail::Environment{
        rules_->billing,
        rules_->payments,
        rules_->disputes,
        *factory_,
        inputs_.issuerAccount,
    });
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

    std::sort(first, slot.indices.end(),
              [&slot](std::uint32_t lhs, std::uint32_t rhs) {
                return slot.purchases[lhs].timestamp <
                       slot.purchases[rhs].timestamp;
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

  // H3 part 3c-ii: card servicing stops with the owner's account —
  // truncate the close ladder at closure minus the settlement-tail
  // guard, so the final cycle's payment/late-fee tail lands strictly
  // before closeTs. The card's rng lane is isolated (keyed by card
  // number below), so every other card is byte-identical.
  const auto serviceEnd = cardServiceEnd(inputs_, cardTerms->owner);
  if (serviceEnd != time::TimePoint::max()) {
    while (!card.closes.empty() && card.closes.back() >= serviceEnd) {
      card.closes.pop_back();
    }
  }

  card.closeCursor = 0;

  const KeyText keyText(account.card.number);

  auto rng = random::RngFactory{rngBase_}.rng(
      {"credit_cards", "lifecycle", keyText.view()});

  card.session = std::make_unique<detail::Session>(
      *env_, account, std::move(rng), emitted_, binding);

  card.sessionReady = true;
}

void CardCycleDriver::compactConsumedPurchases(PerCard &card) {
  if (!card.sessionReady || card.session == nullptr) {
    return;
  }

  const auto consumed = card.session->purchaseCursor();

  if (consumed == 0) {
    return;
  }

  if (consumed > card.indices.size()) {
    throw std::logic_error(
        "CardCycleDriver purchase cursor exceeds index buffer");
  }

  const bool largePrefix = consumed >= kPurchaseCompactionThreshold;

  const bool mostlyConsumed = consumed >= (card.indices.size() + 1) / 2;

  if (!largePrefix && !mostlyConsumed) {
    return;
  }

  const auto remaining = card.indices.size() - consumed;

  std::vector<transactions::Transaction> compactedPurchases;
  std::vector<std::uint32_t> compactedIndices;

  compactedPurchases.reserve(remaining);
  compactedIndices.reserve(remaining);

  for (std::size_t i = consumed; i < card.indices.size(); ++i) {
    const auto oldIndex = static_cast<std::size_t>(card.indices[i]);

    if (oldIndex >= card.purchases.size()) {
      throw std::logic_error(
          "CardCycleDriver purchase index exceeds purchase buffer");
    }

    compactedIndices.push_back(
        static_cast<std::uint32_t>(compactedPurchases.size()));

    compactedPurchases.push_back(std::move(card.purchases[oldIndex]));
  }

  card.purchases.swap(compactedPurchases);
  card.indices.swap(compactedIndices);

  card.session->rebasePurchaseCursor(0);
}

void CardCycleDriver::advanceLedgerTo(time::TimePoint boundExcl) {
  if (!active_) {
    return;
  }

  for (auto &entry : cards_) {
    ensureSession(entry.first, entry.second);
    if (entry.second.session != nullptr) {
      entry.second.session->advanceLedgerTo(boundExcl);
    }
  }
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
      break;
    }

    const detail::CardPurchases purchases{
        .txns = std::span<const transactions::Transaction>(card.purchases),
        .indices = std::span<const std::uint32_t>(card.indices),
    };

    card.session->run(purchases, {
                                     card.priorClose,
                                     close,
                                     windowEndExcl,
                                 });

    card.priorClose = close;
    ++card.closeCursor;

    compactConsumedPurchases(card);
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

    compactConsumedPurchases(entry.second);
  }

  advanceLedgerTo(inputs_.window.endExcl());
}

std::vector<transactions::Transaction>
CardCycleDriver::takeEmittedBefore(std::int64_t boundExcl) {
  std::sort(emitted_.begin(), emitted_.end(), chronologicalComparator());

  const auto split = std::lower_bound(
      emitted_.begin(), emitted_.end(), boundExcl,
      [](const transactions::Transaction &txn, std::int64_t bound) {
        return txn.timestamp < bound;
      });

  std::vector<transactions::Transaction> out;

  out.reserve(static_cast<std::size_t>(split - emitted_.begin()));

  out.insert(out.end(), std::make_move_iterator(emitted_.begin()),
             std::make_move_iterator(split));

  emitted_.erase(emitted_.begin(), split);

  return out;
}

std::vector<transactions::Transaction> CardCycleDriver::takeEmitted() {
  std::sort(emitted_.begin(), emitted_.end(), chronologicalComparator());

  auto out = std::move(emitted_);

  emitted_.clear();

  return out;
}

} // namespace PhantomLedger::transfers::credit_cards
