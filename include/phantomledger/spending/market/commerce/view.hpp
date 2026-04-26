#pragma once

#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/spending/market/commerce/contacts.hpp"
#include "phantomledger/spending/market/commerce/favorites.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace PhantomLedger::spending::market::commerce {

inline constexpr std::uint32_t kNoBurstDay =
    std::numeric_limits<std::uint32_t>::max();

class View {
public:
  View() = default;

  View(const entity::merchant::Catalog *catalog, std::vector<double> merchCdf,
       std::vector<double> billerCdf, Favorites favorites, Billers billers,
       std::vector<float> exploreProp, std::vector<std::uint32_t> burstStartDay,
       std::vector<std::uint16_t> burstLen, Contacts contacts)
      : catalog_(catalog), merchCdf_(std::move(merchCdf)),
        billerCdf_(std::move(billerCdf)), favorites_(std::move(favorites)),
        billers_(std::move(billers)), exploreProp_(std::move(exploreProp)),
        burstStartDay_(std::move(burstStartDay)),
        burstLen_(std::move(burstLen)), contacts_(std::move(contacts)) {}

  [[nodiscard]] const entity::merchant::Catalog *catalog() const noexcept {
    return catalog_;
  }

  [[nodiscard]] const std::vector<double> &merchCdf() const noexcept {
    return merchCdf_;
  }
  [[nodiscard]] const std::vector<double> &billerCdf() const noexcept {
    return billerCdf_;
  }

  [[nodiscard]] const Favorites &favorites() const noexcept {
    return favorites_;
  }
  [[nodiscard]] const Billers &billers() const noexcept { return billers_; }
  [[nodiscard]] Favorites &favoritesMutable() noexcept { return favorites_; }

  [[nodiscard]] float exploreProp(std::uint32_t personIndex) const noexcept {
    return exploreProp_[personIndex];
  }

  [[nodiscard]] std::uint32_t
  burstStartDay(std::uint32_t personIndex) const noexcept {
    return burstStartDay_[personIndex];
  }

  [[nodiscard]] std::uint16_t
  burstLen(std::uint32_t personIndex) const noexcept {
    return burstLen_[personIndex];
  }

  [[nodiscard]] const Contacts &contacts() const noexcept { return contacts_; }
  [[nodiscard]] Contacts &contactsMutable() noexcept { return contacts_; }

  // Mutator used during month-boundary evolution. Marked clearly.
  [[nodiscard]] std::vector<double> &merchCdf() noexcept { return merchCdf_; }

private:
  const entity::merchant::Catalog *catalog_ = nullptr;
  std::vector<double> merchCdf_;
  std::vector<double> billerCdf_;
  Favorites favorites_;
  Billers billers_;
  std::vector<float> exploreProp_;
  std::vector<std::uint32_t> burstStartDay_;
  std::vector<std::uint16_t> burstLen_;
  Contacts contacts_;
};

} // namespace PhantomLedger::spending::market::commerce
