#pragma once

#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/spending/market/commerce/contacts.hpp"
#include "phantomledger/spending/market/commerce/favorites.hpp"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace PhantomLedger::spending::market::commerce {

inline constexpr std::uint32_t kNoBurstDay =
    std::numeric_limits<std::uint32_t>::max();

class MerchantSelection {
public:
  MerchantSelection() = default;

  MerchantSelection(const entity::merchant::Catalog *catalog,
                    std::vector<double> merchCdf,
                    std::vector<double> billerCdf) noexcept
      : catalog_(catalog), merchCdf_(std::move(merchCdf)),
        billerCdf_(std::move(billerCdf)) {}

  [[nodiscard]] const entity::merchant::Catalog *catalog() const noexcept {
    return catalog_;
  }

  [[nodiscard]] const std::vector<double> &merchCdf() const noexcept {
    return merchCdf_;
  }

  [[nodiscard]] std::vector<double> &merchCdf() noexcept { return merchCdf_; }

  [[nodiscard]] const std::vector<double> &billerCdf() const noexcept {
    return billerCdf_;
  }

private:
  const entity::merchant::Catalog *catalog_ = nullptr;
  std::vector<double> merchCdf_;
  std::vector<double> billerCdf_;
};

class AssignedPayees {
public:
  AssignedPayees() = default;

  AssignedPayees(Favorites favorites, Billers billers) noexcept
      : favorites_(std::move(favorites)), billers_(std::move(billers)) {}

  [[nodiscard]] const Favorites &favorites() const noexcept {
    return favorites_;
  }

  [[nodiscard]] Favorites &favoritesMutable() noexcept { return favorites_; }

  [[nodiscard]] const Billers &billers() const noexcept { return billers_; }

private:
  Favorites favorites_;
  Billers billers_;
};

class ShopperActivity {
public:
  ShopperActivity() = default;

  ShopperActivity(std::vector<float> exploreProp,
                  std::vector<std::uint32_t> burstStartDay,
                  std::vector<std::uint16_t> burstLen) noexcept
      : exploreProp_(std::move(exploreProp)),
        burstStartDay_(std::move(burstStartDay)),
        burstLen_(std::move(burstLen)) {}

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

private:
  std::vector<float> exploreProp_;
  std::vector<std::uint32_t> burstStartDay_;
  std::vector<std::uint16_t> burstLen_;
};

class View {
public:
  View() = default;

  View(MerchantSelection selection, AssignedPayees payees,
       ShopperActivity activity, Contacts contacts) noexcept
      : selection_(std::move(selection)), payees_(std::move(payees)),
        activity_(std::move(activity)), contacts_(std::move(contacts)) {}

  [[nodiscard]] const entity::merchant::Catalog *catalog() const noexcept {
    return selection_.catalog();
  }

  [[nodiscard]] const std::vector<double> &merchCdf() const noexcept {
    return selection_.merchCdf();
  }

  [[nodiscard]] const std::vector<double> &billerCdf() const noexcept {
    return selection_.billerCdf();
  }

  [[nodiscard]] const Favorites &favorites() const noexcept {
    return payees_.favorites();
  }

  [[nodiscard]] const Billers &billers() const noexcept {
    return payees_.billers();
  }

  [[nodiscard]] Favorites &favoritesMutable() noexcept {
    return payees_.favoritesMutable();
  }

  [[nodiscard]] float exploreProp(std::uint32_t personIndex) const noexcept {
    return activity_.exploreProp(personIndex);
  }

  [[nodiscard]] std::uint32_t
  burstStartDay(std::uint32_t personIndex) const noexcept {
    return activity_.burstStartDay(personIndex);
  }

  [[nodiscard]] std::uint16_t
  burstLen(std::uint32_t personIndex) const noexcept {
    return activity_.burstLen(personIndex);
  }

  [[nodiscard]] const Contacts &contacts() const noexcept { return contacts_; }

  [[nodiscard]] Contacts &contactsMutable() noexcept { return contacts_; }

  // Mutator used during month-boundary evolution. Marked clearly.
  [[nodiscard]] std::vector<double> &merchCdf() noexcept {
    return selection_.merchCdf();
  }

private:
  MerchantSelection selection_;
  AssignedPayees payees_;
  ShopperActivity activity_;
  Contacts contacts_;
};

} // namespace PhantomLedger::spending::market::commerce
