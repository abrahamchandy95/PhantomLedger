#pragma once

#include "phantomledger/activity/spending/market/bounds.hpp"
#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/activity/spending/market/population/view.hpp"

#include <cstdint>

namespace PhantomLedger::activity::spending::market {

class Market {
public:
  Market() = default;

  Market(Bounds bounds, population::View population, commerce::View commerce,
         Cards cards, std::uint64_t laneSeed)
      : bounds_(bounds), population_(std::move(population)),
        commerce_(std::move(commerce)), cards_(std::move(cards)),
        laneSeed_(laneSeed) {}

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
  // relocation-2026-07: the monthly evolver re-points home areas from the
  // relocation schedule. Mutable for the same reason `commerceMutable` is —
  // the month boundary is the fold's only per-instant hook.
  [[nodiscard]] population::View &populationMutable() noexcept {
    return population_;
  }
  [[nodiscard]] const Cards &cards() const noexcept { return cards_; }

  // evolver-lanes-2026-09: the base seed `buildMarket` drew the payee and
  // behaviour lanes from. The monthly evolver keys its own per-person,
  // per-month lanes on it, so it never draws on the session rng. Carried on
  // the market rather than bound into the day driver so the gate harness,
  // which builds its own market through `buildMarket`, cannot miss it.
  [[nodiscard]] std::uint64_t laneSeed() const noexcept { return laneSeed_; }

private:
  Bounds bounds_{};
  population::View population_{};
  commerce::View commerce_{};
  Cards cards_{};
  std::uint64_t laneSeed_ = 0;
};

} // namespace PhantomLedger::activity::spending::market
