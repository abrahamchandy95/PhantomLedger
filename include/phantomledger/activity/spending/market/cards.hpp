#pragma once

/*
  THE MARKET-SIDE VIEW OF WHO HOLDS WHICH CARDS, and it is MULTI-VALUED.

  This used to be one `entity::Key` per person. Combined with the single
  `Spender::depositAccount`, that put a hard ceiling of TWO distinct card-view
  source accounts on any person, and therefore on any legitimate device — the
  structural cause of the exported device fan-out cliff.

  It carries every card a person HOLDS. The spend-active subset that reaches
  the hot loop is chosen in `prepareSpenders` and capped by
  `actors::kMaxCreditInstruments`, which is a smaller number: holdings follow
  the cited card-count law, wiring follows a declared billing budget.
 */

#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market {

class Cards {
public:
  Cards() = default;

  explicit Cards(std::size_t count)
      : rows_(
            entity::card::makeCardsByPerson(static_cast<std::uint32_t>(count))),
        count_(count) {
    flat_.assign(count * entity::card::kMaxCardsPerPerson, entity::Key{});
  }

  /* APPENDS. Callers hand cards over in issuance order, which is the order
   * `entity::card::Registry::records` already holds them in, so slot 0 stays
   * the person's first card. Silently drops beyond `kMaxCardsPerPerson`:
   * over-capacity is an issuance-side calibration error, not a market-side
   * one, and the issuance gate is where it should surface. */
  void assign(entity::PersonId person, entity::Key card) {
    const auto row = static_cast<std::uint32_t>(index(person));
    const auto slot = rows_.rowLength(row);
    if (slot >= entity::card::kMaxCardsPerPerson) {
      return;
    }
    flat_[row * entity::card::kMaxCardsPerPerson + slot] = card;
    (void)rows_.pushBack(row, static_cast<std::uint32_t>(slot));
  }

  [[nodiscard]] bool hasCard(entity::PersonId person) const noexcept {
    return rows_.rowLength(static_cast<std::uint32_t>(index(person))) > 0;
  }

  /* Every card this person holds, in issuance order. */
  [[nodiscard]] std::span<const entity::Key>
  cards(entity::PersonId person) const noexcept {
    const auto row = index(person);
    return {flat_.data() + row * entity::card::kMaxCardsPerPerson,
            rows_.rowLength(static_cast<std::uint32_t>(row))};
  }

  /* THE FIRST card. Legacy single-instrument accessor; anything reasoning
   * about how many cards a person holds must use `cards` instead. */
  [[nodiscard]] entity::Key card(entity::PersonId person) const noexcept {
    const auto row = cards(person);
    return row.empty() ? entity::Key{} : row.front();
  }

  /* PEOPLE, not cards. `buildMarket` checks it against `census.count`. */
  [[nodiscard]] std::size_t size() const noexcept { return count_; }

private:
  [[nodiscard]] static std::size_t index(entity::PersonId person) noexcept {
    return static_cast<std::size_t>(person - 1);
  }

  /* `rows_` carries only the LENGTHS (its values are slot ordinals); the Keys
   * live in `flat_` at the matching offsets. Two arrays rather than a
   * `Csr<uint32_t, entity::Key>` because the Csr's offsets are uint32 and a
   * Key is 16 bytes — keeping the Key store separate leaves the offset space
   * counting slots, not bytes. */
  entity::card::CardsByPerson rows_;
  std::vector<entity::Key> flat_;
  std::size_t count_ = 0;
};

} // namespace PhantomLedger::activity::spending::market
