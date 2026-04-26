#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <cstddef>
#include <vector>

namespace PhantomLedger::spending::market {

class Cards {
public:
  Cards() = default;

  explicit Cards(std::size_t count) : byPerson_(count, entity::Key{}) {}

  void assign(entity::PersonId person, entity::Key card) {
    byPerson_[index(person)] = card;
  }

  [[nodiscard]] bool hasCard(entity::PersonId person) const noexcept {
    return entity::valid(byPerson_[index(person)]);
  }

  [[nodiscard]] entity::Key card(entity::PersonId person) const noexcept {
    return byPerson_[index(person)];
  }

  [[nodiscard]] std::size_t size() const noexcept { return byPerson_.size(); }

private:
  [[nodiscard]] static std::size_t index(entity::PersonId person) noexcept {
    return static_cast<std::size_t>(person - 1);
  }

  std::vector<entity::Key> byPerson_;
};

} // namespace PhantomLedger::spending::market
