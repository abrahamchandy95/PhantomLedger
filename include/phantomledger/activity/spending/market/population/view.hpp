#pragma once

#include "phantomledger/activity/spending/market/population/paydays.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::activity::spending::market::population {

class View {
public:
  View() = default;

  View(std::vector<entity::Key> primary, std::vector<personas::Type> kinds,
       std::vector<entity::behavior::Persona> objects, Paydays paydays,
       std::vector<entity::geography::GeoAreaId> homeAreas = {})
      : primary_(std::move(primary)), kinds_(std::move(kinds)),
        objects_(std::move(objects)), paydays_(std::move(paydays)),
        homeAreas_(std::move(homeAreas)) {}

  [[nodiscard]] std::uint32_t count() const noexcept {
    return static_cast<std::uint32_t>(primary_.size());
  }

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

  // geo-causal-v1 (G2a): the person's home area for card-present
  // distance-decay selection. invalidGeoArea when no carrier is bound
  // (the monolith reference oracle) — the caller treats that as "no
  // local anchor" and falls back to the national selection.
  [[nodiscard]] entity::geography::GeoAreaId
  homeArea(entity::PersonId p) const noexcept {
    const auto i = index(p);
    return i < homeAreas_.size() ? homeAreas_[i]
                                 : entity::geography::invalidGeoArea;
  }

private:
  [[nodiscard]] static std::size_t index(entity::PersonId p) noexcept {
    return static_cast<std::size_t>(p - 1);
  }

  std::vector<entity::Key> primary_;
  std::vector<personas::Type> kinds_;
  std::vector<entity::behavior::Persona> objects_;
  Paydays paydays_;
  std::vector<entity::geography::GeoAreaId> homeAreas_;
};

} // namespace PhantomLedger::activity::spending::market::population
