#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"
#include "phantomledger/transfers/channels/credit_cards/detail/session.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

[[nodiscard]] std::unordered_map<entity::Key, std::vector<std::uint32_t>>
indexPurchasesByCard(std::span<const transactions::Transaction> txns,
                     time::TimePoint windowStart,
                     time::TimePoint windowEndExcl) {
  std::unordered_map<entity::Key, std::vector<std::uint32_t>> byCard;

  const auto purchaseTag = channels::tag(channels::Legit::cardPurchase);
  const std::int64_t startEpoch = time::toEpochSeconds(windowStart);
  const std::int64_t endEpoch = time::toEpochSeconds(windowEndExcl);

  for (std::uint32_t i = 0; i < txns.size(); ++i) {
    const auto &txn = txns[i];
    if (txn.session.channel != purchaseTag) {
      continue;
    }
    if (txn.timestamp < startEpoch || txn.timestamp >= endEpoch) {
      continue;
    }
    byCard[txn.source].push_back(i);
  }

  for (auto &entry : byCard) {
    auto &indices = entry.second;
    std::sort(indices.begin(), indices.end(),
              [&txns](std::uint32_t a, std::uint32_t b) {
                return txns[a].timestamp < txns[b].timestamp;
              });
  }
  return byCard;
}

[[nodiscard]] std::optional<detail::Account> resolveAccount(
    const entity::card::Terms &card,
    const std::unordered_map<entity::PersonId, entity::Key> &primaryAccounts) {
  const auto it = primaryAccounts.find(card.owner);
  if (it == primaryAccounts.end()) {
    return std::nullopt;
  }
  return detail::Account{
      .card = card.key,
      .funding = it->second,
      .apr = card.apr,
      .cycleDay = card.cycleDay,
      .autopay = card.autopay,
  };
}

} // namespace

Lifecycle::Lifecycle(const LifecycleRules &rules,
                     const transactions::Factory &factory,
                     const random::RngFactory &rngFactory, LedgerView ledger)
    : rules_(rules), factory_(factory), rngFactory_(rngFactory),
      ledger_(std::move(ledger)) {}

std::vector<transactions::Transaction>
Lifecycle::generate(const time::Window &window,
                    std::span<const transactions::Transaction> txns) const {
  std::vector<transactions::Transaction> out;
  if (ledger_.cards.records.empty() || txns.empty() || window.days <= 0) {
    return out;
  }

  const time::TimePoint windowStart = window.start;
  const time::TimePoint windowEndExcl = window.endExcl();

  auto purchasesByCard = indexPurchasesByCard(txns, windowStart, windowEndExcl);
  if (purchasesByCard.empty()) {
    return out;
  }

  out.reserve(txns.size() / 8);

  const detail::Environment env{rules_.billing, rules_.payments,
                                rules_.disputes, factory_,
                                ledger_.issuerAccount};

  std::vector<entity::Key> activeCards;
  activeCards.reserve(purchasesByCard.size());
  for (const auto &entry : purchasesByCard) {
    activeCards.push_back(entry.first);
  }
  std::sort(activeCards.begin(), activeCards.end());

  for (const auto &cardKey : activeCards) {
    const auto &indices = purchasesByCard.at(cardKey);

    const auto *cardTerms = ledger_.cards.forKey(cardKey);
    if (cardTerms == nullptr) {
      continue;
    }

    const auto account = resolveAccount(*cardTerms, ledger_.primaryAccounts);
    if (!account.has_value()) {
      continue;
    }

    const auto closes =
        statementCloseDates(windowStart, windowEndExcl, account->cycleDay);
    if (closes.empty()) {
      continue;
    }

    const detail::CardPurchases purchases{
        .txns = txns,
        .indices = std::span<const std::uint32_t>(indices),
    };

    const KeyText keyText(account->card.number);
    auto rng = rngFactory_.rng({"credit_cards", "lifecycle", keyText.view()});
    detail::Session session(env, *account, std::move(rng), out);

    time::TimePoint priorClose = windowStart;
    for (const auto close : closes) {
      session.run(purchases, {priorClose, close, windowEndExcl});
      priorClose = close;
    }
  }

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::credit_cards
