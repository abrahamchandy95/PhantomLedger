#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/gamma.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace PhantomLedger::math::amounts {

struct AmountModel {
  enum class Kind : std::uint8_t {
    invalid = 0,
    logNormal = 1,
    gamma = 2,
  };

  Kind kind = Kind::invalid;
  // LogNormal: p0=median,  p1=sigma
  // Gamma:     p0=shape,   p1=scale,  p2=add
  double p0 = 0.0;
  double p1 = 0.0;
  double p2 = 0.0;
  double floor = 1.0;

  [[nodiscard]] static constexpr AmountModel
  lognormal(double median, double sigma, double floor_ = 1.0) {
    return {Kind::logNormal, median, sigma, 0.0, floor_};
  }

  [[nodiscard]] static constexpr AmountModel
  gamma(double shape, double scale, double add, double floor_ = 1.0) {
    return {Kind::gamma, shape, scale, add, floor_};
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    return kind != Kind::invalid;
  }

  [[nodiscard]] double sample(random::Rng &rng) const {
    switch (kind) {
    case Kind::logNormal: {
      const double raw =
          probability::distributions::lognormalByMedian(rng, p0, p1);
      return primitives::utils::roundMoney(std::max(floor, raw));
    }
    case Kind::gamma: {
      const double raw = probability::distributions::gamma(rng, p0, p1) + p2;
      return primitives::utils::roundMoney(std::max(floor, raw));
    }
    case Kind::invalid:
      break;
    }
    return floor;
  }
};

// --- Named constants (stable names for direct use by generators) ------

inline constexpr auto kSalary = AmountModel::lognormal(3000.0, 0.35, 50.0);
inline constexpr auto kRent = AmountModel::gamma(2.0, 400.0, 50.0, 1.0);
inline constexpr auto kP2P = AmountModel::lognormal(45.0, 0.8, 1.0);
inline constexpr auto kBill = AmountModel::gamma(2.0, 400.0, 50.0, 1.0);
inline constexpr auto kExternalUnknown =
    AmountModel::lognormal(120.0, 0.95, 5.0);
inline constexpr auto kAtm = AmountModel::lognormal(80.0, 0.30, 20.0);
inline constexpr auto kSelfTransfer = AmountModel::lognormal(250.0, 0.80, 10.0);
inline constexpr auto kSubscription = AmountModel::lognormal(15.0, 0.40, 5.0);
inline constexpr auto kClientAchCredit =
    AmountModel::lognormal(1500.0, 0.75, 50.0);
inline constexpr auto kCardSettlement =
    AmountModel::lognormal(650.0, 0.60, 20.0);
inline constexpr auto kPlatformPayout =
    AmountModel::lognormal(400.0, 0.65, 10.0);
inline constexpr auto kOwnerDraw = AmountModel::lognormal(2500.0, 0.80, 100.0);
inline constexpr auto kInvestmentInflow =
    AmountModel::lognormal(5000.0, 1.00, 100.0);
inline constexpr auto kFraud = AmountModel::lognormal(900.0, 0.70, 50.0);
inline constexpr auto kFraudCycle = AmountModel::lognormal(600.0, 0.25, 1.0);
inline constexpr auto kFraudBoostCycle =
    AmountModel::lognormal(500.0, 0.20, 1.0);

namespace detail {

[[nodiscard]] consteval std::array<AmountModel, 256> buildChannelTable() {
  std::array<AmountModel, 256> t{};
  using L = channels::Legit;
  using R = channels::Rent;
  using F = channels::Fraud;

  t[channels::tag(L::salary).value] = kSalary;
  t[channels::tag(L::p2p).value] = kP2P;
  t[channels::tag(L::bill).value] = kBill;
  t[channels::tag(L::externalUnknown).value] = kExternalUnknown;
  t[channels::tag(L::atm).value] = kAtm;
  t[channels::tag(L::selfTransfer).value] = kSelfTransfer;
  t[channels::tag(L::subscription).value] = kSubscription;
  t[channels::tag(L::clientAchCredit).value] = kClientAchCredit;
  t[channels::tag(L::cardSettlement).value] = kCardSettlement;
  t[channels::tag(L::platformPayout).value] = kPlatformPayout;
  t[channels::tag(L::ownerDraw).value] = kOwnerDraw;
  t[channels::tag(L::investmentInflow).value] = kInvestmentInflow;

  // All rent variants share the RENT gamma. The per-channel signal
  // is carried by the tag itself; the amount shape does not need to
  // differ to distinguish Zelle-to-individual from corporate portal.
  t[channels::tag(R::generic).value] = kRent;
  t[channels::tag(R::ach).value] = kRent;
  t[channels::tag(R::portal).value] = kRent;
  t[channels::tag(R::p2p).value] = kRent;
  t[channels::tag(R::check).value] = kRent;

  t[channels::tag(F::classic).value] = kFraud;
  t[channels::tag(F::cycle).value] = kFraudCycle;

  return t;
}

[[nodiscard]] consteval std::array<AmountModel, merchants::kCategoryCount>
buildMerchantTable() {
  std::array<AmountModel, merchants::kCategoryCount> t{};
  using C = merchants::Category;
  t[merchants::slot(C::grocery)] = AmountModel::lognormal(50.0, 0.55, 1.0);
  t[merchants::slot(C::fuel)] = AmountModel::lognormal(45.0, 0.35, 1.0);
  t[merchants::slot(C::restaurant)] = AmountModel::lognormal(28.0, 0.60, 1.0);
  t[merchants::slot(C::pharmacy)] = AmountModel::lognormal(25.0, 0.65, 1.0);
  t[merchants::slot(C::ecommerce)] = AmountModel::lognormal(85.0, 0.70, 1.0);
  t[merchants::slot(C::retailOther)] = AmountModel::lognormal(45.0, 0.75, 1.0);
  t[merchants::slot(C::utilities)] = AmountModel::lognormal(120.0, 0.40, 1.0);
  t[merchants::slot(C::telecom)] = AmountModel::lognormal(75.0, 0.30, 1.0);
  t[merchants::slot(C::insurance)] = AmountModel::lognormal(150.0, 0.35, 1.0);
  t[merchants::slot(C::education)] = AmountModel::lognormal(200.0, 0.60, 1.0);
  return t;
}

inline constexpr auto kChannelTable = buildChannelTable();
inline constexpr auto kMerchantTable = buildMerchantTable();

inline constexpr AmountModel kDefaultMerchant =
    AmountModel::lognormal(45.0, 0.70, 1.0);

} // namespace detail

/// O(1) lookup by tag. Returned reference is valid for program lifetime.
[[nodiscard]] inline constexpr const AmountModel &
forChannel(channels::Tag t) noexcept {
  return detail::kChannelTable[t.value];
}

/// Sample an amount for the given channel. Throws if the channel has
/// no registered model — matches Python's KeyError. Intentional loud
/// failure prevents silent fallthrough to a wrong distribution.
[[nodiscard]] inline double sample(random::Rng &rng, channels::Tag t) {
  const auto &m = detail::kChannelTable[t.value];
  if (!m.valid()) {
    throw std::out_of_range(
        "amounts::sample: no amount model registered for channel");
  }
  return m.sample(rng);
}

/// Sample a merchant-category amount. Unknown categories fall back
/// to a default lognormal rather than throwing — categories are a
/// smaller fixed enum and partial coverage is acceptable.
[[nodiscard]] inline double merchantAmount(random::Rng &rng,
                                           merchants::Category c) {
  const auto &m = detail::kMerchantTable[merchants::slot(c)];
  return m.valid() ? m.sample(rng) : detail::kDefaultMerchant.sample(rng);
}

} // namespace PhantomLedger::math::amounts
