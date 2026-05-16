#pragma once

#include "phantomledger/activity/spending/market/bounds.hpp"
#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/activity/spending/market/population/view.hpp"

namespace PhantomLedger::activity::spending::market {

class Market {
public:
  Market() = default;

  Market(Bounds bounds, population::View population, commerce::View commerce,
         Cards cards)
      : bounds_(bounds), population_(std::move(population)),
        commerce_(std::move(commerce)), cards_(std::move(cards)) {}

  Market(const Market &) = delete;
  Market &operator=(const Market &) = delete;
  Market(Market &&) noexcept = default;
  Market &operator=(Market &&) noexcept = default;

  [[nodiscard]] const Bounds &bounds() const noexcept { return bounds_; }
  [[nodiscard]] const population::View &population() const noexcept {
    return population_;
  }
  [[nodiscard]] const commerce::View &commerce() const noexcept {
    return commerce_;
  }
  [[nodiscard]] commerce::View &commerceMutable() noexcept { return commerce_; }
  [[nodiscard]] const Cards &cards() const noexcept { return cards_; }

private:
  Bounds bounds_{};
  population::View population_{};
  commerce::View commerce_{};
  Cards cards_{};
};

} // namespace PhantomLedger::activity::spending::market
