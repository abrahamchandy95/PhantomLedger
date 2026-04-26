#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/spending/market/population/paydays.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::spending::market::population {

class View {
public:
  View() = default;

  View(std::uint32_t count, std::vector<entity::Key> primary,
       std::vector<personas::Type> kinds,
       std::vector<entity::behavior::Persona> objects, Paydays paydays)
      : count_(count), primary_(std::move(primary)), kinds_(std::move(kinds)),
        objects_(std::move(objects)), paydays_(std::move(paydays)) {}

  [[nodiscard]] std::uint32_t count() const noexcept { return count_; }

  [[nodiscard]] entity::Key primary(entity::PersonId p) const noexcept {
    return primary_[index(p)];
  }

  [[nodiscard]] personas::Type kind(entity::PersonId p) const noexcept {
    return kinds_[index(p)];
  }

  [[nodiscard]] const entity::behavior::Persona &
  object(entity::PersonId p) const noexcept {
    return objects_[index(p)];
  }

  [[nodiscard]] bool isPayday(entity::PersonId p,
                              std::uint32_t dayIndex) const noexcept {
    return paydays_.isPayday(index(p), dayIndex);
  }

  [[nodiscard]] const Paydays &paydays() const noexcept { return paydays_; }

private:
  [[nodiscard]] static std::size_t index(entity::PersonId p) noexcept {
    return static_cast<std::size_t>(p - 1);
  }

  std::uint32_t count_ = 0;
  std::vector<entity::Key> primary_;
  std::vector<personas::Type> kinds_;
  std::vector<entity::behavior::Persona> objects_;
  Paydays paydays_;
};

} // namespace PhantomLedger::spending::market::population
