#pragma once

#include "phantomledger/activity/spending/market/commerce/favorites.hpp"
#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
#include "phantomledger/activity/spending/market/commerce/reach.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/utils/groups.hpp"
#include "phantomledger/relationships/social/contacts.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

inline constexpr std::uint32_t kNoBurstDay =
    std::numeric_limits<std::uint32_t>::max();

/* ONE SPENDING BURST WINDOW — a short stretch of elevated exploration, the
 * vacation / holiday / renovation shape: `burstMultiplier` 3.25x on
 * exploreP for 3-9 days.
 *
 * THE RATE IS PER-YEAR AND A PERSON CARRIES AS MANY WINDOWS AS THE WINDOW
 * LENGTH IMPLIES. Do not collapse it back to one per-run coin: p=0.08 over
 * `[0, days)` means ~0.49/year at 60 days and 0.004/year over 20 years, and
 * measured, a 20-year run gave 92% of people no burst at all and the rest
 * exactly one. */
struct BurstWindow {
  std::uint32_t startDay = kNoBurstDay;
  std::uint16_t lengthDays = 0;

  [[nodiscard]] constexpr bool covers(std::uint32_t dayIndex) const noexcept {
    return startDay != kNoBurstDay && lengthDays > 0 && dayIndex >= startDay &&
           dayIndex < startDay + lengthDays;
  }
};

using BurstSchedule = primitives::utils::Csr<std::uint32_t, BurstWindow>;

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

  [[nodiscard]] std::vector<double> &billerCdf() noexcept { return billerCdf_; }

  [[nodiscard]] const std::vector<double> &billerCdf() const noexcept {
    return billerCdf_;
  }

  /* REACH, the law favourite-set MEMBERSHIP is drawn from. Deliberately NOT
   * `merchCdf_`: that is a VOLUME law, and sampling membership from it turns
   * a ~5% volume weight into a 51% share of every card. See
   * commerce/reach.hpp.
   *
   * The CDF is derived from the model rather than stored twice, so the two
   * can never disagree. The model itself is kept because the gate prints
   * `gamma` and `realizedTop1` — a solved exponent that silently saturates
   * is a sizing failure no total would reveal. */
  [[nodiscard]] const ReachModel &reach() const noexcept { return reach_; }
  [[nodiscard]] ReachModel &reachMutable() noexcept { return reach_; }

  [[nodiscard]] const std::vector<double> &reachCdf() const noexcept {
    return reachCdf_;
  }
  [[nodiscard]] std::vector<double> &reachCdf() noexcept { return reachCdf_; }

  [[nodiscard]] const MembershipSampler &membership() const noexcept {
    return membership_;
  }
  [[nodiscard]] MembershipSampler &membershipMutable() noexcept {
    return membership_;
  }

private:
  const entity::merchant::Catalog *catalog_ = nullptr;
  std::vector<double> merchCdf_;
  std::vector<double> billerCdf_;
  ReachModel reach_;
  std::vector<double> reachCdf_;
  MembershipSampler membership_;
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

  [[nodiscard]] Billers &billersMutable() noexcept { return billers_; }

  [[nodiscard]] const Billers &billers() const noexcept { return billers_; }

private:
  Favorites favorites_;
  Billers billers_;
};

class ShopperActivity {
public:
  ShopperActivity() = default;

  ShopperActivity(std::vector<float> exploreProp, BurstSchedule bursts) noexcept
      : exploreProp_(std::move(exploreProp)), bursts_(std::move(bursts)) {}

  [[nodiscard]] float exploreProp(std::uint32_t personIndex) const noexcept {
    return exploreProp_[personIndex];
  }

  // Every burst window this person has over the run, in day order. Empty is
  // normal — most people have none in a short window.
  [[nodiscard]] std::span<const BurstWindow>
  bursts(std::uint32_t personIndex) const noexcept {
    if (personIndex + 1 >= bursts_.offsets().size()) {
      return {};
    }
    return bursts_.rowOf(personIndex);
  }

private:
  std::vector<float> exploreProp_;
  BurstSchedule bursts_;
};

class View {
public:
  View() = default;

  /* `geoPools` is the per-home-area distance-decay pool over physical
   * merchants, read by the card-present branch of `pickMerchantIndex`.
   * Defaulted (empty) so a caller with no geography still constructs a View;
   * when empty, card-present selection falls back to the national CDF. */
  View(MerchantSelection selection, AssignedPayees payees,
       ShopperActivity activity, relationships::social::Contacts contacts,
       LocalPools geoPools = {}) noexcept
      : selection_(std::move(selection)), payees_(std::move(payees)),
        activity_(std::move(activity)), contacts_(std::move(contacts)),
        geoPools_(std::move(geoPools)) {}

  [[nodiscard]] const entity::merchant::Catalog *catalog() const noexcept {
    return selection_.catalog();
  }

  [[nodiscard]] const std::vector<double> &merchCdf() const noexcept {
    return selection_.merchCdf();
  }

  [[nodiscard]] const std::vector<double> &billerCdf() const noexcept {
    return selection_.billerCdf();
  }

  /* `reachCdf` is what the favourite-membership draw samples — at bootstrap
   * and in the monthly add pass alike. BOTH MUST READ THE SAME LAW, or the
   * evolver reintroduces the hub through the back door, which is what a
   * popularity-weighted add does. */
  [[nodiscard]] const ReachModel &reach() const noexcept {
    return selection_.reach();
  }
  [[nodiscard]] ReachModel &reachMutable() noexcept {
    return selection_.reachMutable();
  }
  [[nodiscard]] const std::vector<double> &reachCdf() const noexcept {
    return selection_.reachCdf();
  }
  [[nodiscard]] std::vector<double> &reachCdf() noexcept {
    return selection_.reachCdf();
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

  /* Billers churn for the same reason favourites do: a utility or
   * subscription merchant that closes stops being billable. */
  [[nodiscard]] Billers &billersMutable() noexcept {
    return payees_.billersMutable();
  }

  [[nodiscard]] std::vector<double> &billerCdf() noexcept {
    return selection_.billerCdf();
  }

  [[nodiscard]] float exploreProp(std::uint32_t personIndex) const noexcept {
    return activity_.exploreProp(personIndex);
  }

  [[nodiscard]] std::span<const BurstWindow>
  bursts(std::uint32_t personIndex) const noexcept {
    return activity_.bursts(personIndex);
  }

  [[nodiscard]] const relationships::social::Contacts &
  contacts() const noexcept {
    return contacts_;
  }

  [[nodiscard]] relationships::social::Contacts &contactsMutable() noexcept {
    return contacts_;
  }

  /* Per-home-area distance-decay merchant pools over the VOLUME weights.
   * Empty ⇒ no local anchor; the card-present branch falls back to the
   * national CDF.
   *
   * `LocalPools`, not a dense predecessor: same has()/sample() contract and
   * the same distribution at O(M + A*k) instead of O(A*M). local_pools.hpp
   * carries the measured memory pair. */
  [[nodiscard]] const LocalPools &geoPools() const noexcept {
    return geoPools_;
  }

  /* Mutators used during month-boundary evolution: both the national CDF and
   * the geo pools are rebuilt against merchant liveness each month. */
  [[nodiscard]] std::vector<double> &merchCdf() noexcept {
    return selection_.merchCdf();
  }

  [[nodiscard]] LocalPools &geoPoolsMutable() noexcept { return geoPools_; }

  /* The favourite-MEMBERSHIP sampler: online national, physical localised.
   * See local_pools.hpp — this is what makes a physical outlet's reach
   * ceiling emerge from its own area's population instead of being a
   * declared number. */
  [[nodiscard]] const MembershipSampler &membership() const noexcept {
    return selection_.membership();
  }
  [[nodiscard]] MembershipSampler &membershipMutable() noexcept {
    return selection_.membershipMutable();
  }

private:
  MerchantSelection selection_;
  AssignedPayees payees_;
  ShopperActivity activity_;
  relationships::social::Contacts contacts_;
  LocalPools geoPools_;
};

} // namespace PhantomLedger::activity::spending::market::commerce
