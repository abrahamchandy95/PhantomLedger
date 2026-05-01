#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/gamma.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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

  // LogNormal: p0 = median, p1 = sigma
  // Gamma:     p0 = shape,  p1 = scale, p2 = add
  double p0 = 0.0;
  double p1 = 0.0;
  double p2 = 0.0;
  double floor = 1.0;

  [[nodiscard]] static constexpr AmountModel
  lognormal(double median, double sigma, double floor_ = 1.0) noexcept {
    return {Kind::logNormal, median, sigma, 0.0, floor_};
  }

  [[nodiscard]] static constexpr AmountModel
  gamma(double shape, double scale, double add, double floor_ = 1.0) noexcept {
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

// --- Named constants ---------------------------------------------------

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

namespace enumTax = ::PhantomLedger::taxonomies::enums;

[[nodiscard]] constexpr std::size_t channelIndex(channels::Tag tag) noexcept {
  return static_cast<std::size_t>(tag.value);
}

[[nodiscard]] constexpr std::size_t
merchantIndex(merchants::Category category) noexcept {
  return enumTax::toIndex(category);
}

inline void setChannel(std::array<AmountModel, 256> &table, channels::Tag tag,
                       AmountModel model) noexcept {
  table[channelIndex(tag)] = model;
}

inline void
setMerchant(std::array<AmountModel, merchants::kCategoryCount> &table,
            merchants::Category category, AmountModel model) noexcept {
  table[merchantIndex(category)] = model;
}

[[nodiscard]] inline std::array<AmountModel, 256> buildChannelTable() {
  std::array<AmountModel, 256> table{};

  using L = channels::Legit;
  using R = channels::Rent;
  using F = channels::Fraud;

  setChannel(table, channels::tag(L::salary), kSalary);
  setChannel(table, channels::tag(L::p2p), kP2P);
  setChannel(table, channels::tag(L::bill), kBill);
  setChannel(table, channels::tag(L::externalUnknown), kExternalUnknown);
  setChannel(table, channels::tag(L::atm), kAtm);
  setChannel(table, channels::tag(L::selfTransfer), kSelfTransfer);
  setChannel(table, channels::tag(L::subscription), kSubscription);
  setChannel(table, channels::tag(L::clientAchCredit), kClientAchCredit);
  setChannel(table, channels::tag(L::cardSettlement), kCardSettlement);
  setChannel(table, channels::tag(L::platformPayout), kPlatformPayout);
  setChannel(table, channels::tag(L::ownerDraw), kOwnerDraw);
  setChannel(table, channels::tag(L::investmentInflow), kInvestmentInflow);

  // All rent variants share the rent distribution. The channel tag
  // still carries the behavioral signal.
  setChannel(table, channels::tag(R::generic), kRent);
  setChannel(table, channels::tag(R::ach), kRent);
  setChannel(table, channels::tag(R::portal), kRent);
  setChannel(table, channels::tag(R::p2p), kRent);
  setChannel(table, channels::tag(R::check), kRent);

  setChannel(table, channels::tag(F::classic), kFraud);
  setChannel(table, channels::tag(F::cycle), kFraudCycle);

  return table;
}

[[nodiscard]] inline std::array<AmountModel, merchants::kCategoryCount>
buildMerchantTable() {
  std::array<AmountModel, merchants::kCategoryCount> table{};

  using C = merchants::Category;

  setMerchant(table, C::grocery, AmountModel::lognormal(50.0, 0.55, 1.0));
  setMerchant(table, C::fuel, AmountModel::lognormal(45.0, 0.35, 1.0));
  setMerchant(table, C::restaurant, AmountModel::lognormal(28.0, 0.60, 1.0));
  setMerchant(table, C::pharmacy, AmountModel::lognormal(25.0, 0.65, 1.0));
  setMerchant(table, C::ecommerce, AmountModel::lognormal(85.0, 0.70, 1.0));
  setMerchant(table, C::retailOther, AmountModel::lognormal(45.0, 0.75, 1.0));
  setMerchant(table, C::utilities, AmountModel::lognormal(120.0, 0.40, 1.0));
  setMerchant(table, C::telecom, AmountModel::lognormal(75.0, 0.30, 1.0));
  setMerchant(table, C::insurance, AmountModel::lognormal(150.0, 0.35, 1.0));
  setMerchant(table, C::education, AmountModel::lognormal(200.0, 0.60, 1.0));

  return table;
}

inline const auto kChannelTable = buildChannelTable();
inline const auto kMerchantTable = buildMerchantTable();

inline constexpr AmountModel kDefaultMerchant =
    AmountModel::lognormal(45.0, 0.70, 1.0);

} // namespace detail

/// O(1) lookup by channel tag. Returned reference is valid for program
/// lifetime.
[[nodiscard]] inline const AmountModel &forChannel(channels::Tag tag) noexcept {
  return detail::kChannelTable[detail::channelIndex(tag)];
}

/// Sample an amount for the given channel. Throws if the channel has
/// no registered model.
[[nodiscard]] inline double sample(random::Rng &rng, channels::Tag tag) {
  const auto &model = forChannel(tag);

  if (!model.valid()) {
    throw std::out_of_range(
        "amounts::sample: no amount model registered for channel");
  }

  return model.sample(rng);
}

/// Sample a merchant-category amount. Unknown categories fall back
/// to a default lognormal model.
[[nodiscard]] inline double merchantAmount(random::Rng &rng,
                                           merchants::Category category) {
  const auto &model = detail::kMerchantTable[detail::merchantIndex(category)];

  if (model.valid()) {
    return model.sample(rng);
  }

  return detail::kDefaultMerchant.sample(rng);
}

} // namespace PhantomLedger::math::amounts
