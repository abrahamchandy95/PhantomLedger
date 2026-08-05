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

// ONE SPENDING BURST WINDOW (merchant-churn-2026-07 follow-on,
// burst-rate-2026-07).
//
// A burst is a short stretch of elevated exploration — the vacation /
// holiday / renovation shape, `burstMultiplier` 3.25x on exploreP for 3-9
// days. It used to be ONE PER PERSON PER RUN, drawn with p=0.08 over
// `[0, days)`.
//
// That made the constant mean completely different things at different
// window lengths: 0.08 per 60 days is ~0.49/year, while 0.08 per 20 YEARS is
// 0.004/year. **A 20-year run gave 92% of people no burst at all and the
// rest exactly one, across two decades.** The rate is now per-YEAR and a
// person carries as many windows as the window length implies.
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

  // merchant-selection-2026-08: REACH, the law favourite-set MEMBERSHIP is
  // drawn from. Deliberately NOT `merchCdf_`: that is a VOLUME law, and
  // sampling membership from it turned a ~5% volume weight into a 51% share
  // of every card in the corpus. See commerce/reach.hpp.
  //
  // The CDF is derived from the model rather than stored twice, so the two
  // can never disagree; the model itself is kept because the gate prints
  // `gamma` and `realizedTop1` (a solved exponent that silently saturates
  // is exactly the kind of sizing failure CLAUDE.md rule 6 is about).
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

  ShopperActivity(std::vector<float> exploreProp,
                  BurstSchedule bursts) noexcept
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

  // geo-causal-v1 (G2a step-2): `geoPools` is the per-home-area distance-decay
  // pool over physical merchants. Defaulted (empty) so callers that predate
  // the geo model still construct a View; when empty, card-present selection
  // falls back to the national CDF. UNREAD until the selection change lands.
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

  // merchant-selection-2026-08. `reachCdf` is what the favourite-membership
  // draw samples — at bootstrap and in the monthly add pass alike. Both must
  // read the SAME law or the evolver reintroduces the hub through the back
  // door, which is what the popularity-weighted add did.
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

  // merchant-churn-2026-07: billers churn for the same reason favourites do
  // — a utility or subscription merchant that closes stops being billable.
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

  // Per-home-area distance-decay merchant pools (G2a step-2). Empty ⇒ no
  // local anchor; the card-present branch falls back to the national CDF.
  // merchant-selection-2026-08: `LocalPools`, not the dense
  // `GeographicMerchantPools` it replaces. Same has()/sample() contract, same
  // distribution, O(M + A*k) instead of O(A*M) — measured 0.48 MB against
  // 26.75 MB at the 500,000-person target.
  [[nodiscard]] const LocalPools &geoPools() const noexcept {
    return geoPools_;
  }

  // Mutator used during month-boundary evolution. Marked clearly.
  [[nodiscard]] std::vector<double> &merchCdf() noexcept {
    return selection_.merchCdf();
  }

  // merchant-churn-2026-07: the geo pools are rebuilt against merchant
  // liveness at each month boundary, so this mutator exists for the same
  // reason merchCdf()'s does.
  [[nodiscard]] LocalPools &geoPoolsMutable() noexcept { return geoPools_; }

  // The favourite-MEMBERSHIP sampler: online national, physical localised.
  // See local_pools.hpp — this is what makes a physical outlet's reach ceiling
  // emerge from its own area's population instead of being a declared number.
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
