#pragma once

/*
  Cash-service endpoints in the customer-transaction projection.

  These keys identify observable ATM/acceptor and cash-depository locations;
  they are NOT balance-bearing general-ledger accounts.  Every key is external
  to the modeled customer ledger, so clearing mutates only the internal leg.
  The centralized ATM-cash/network-settlement GL remains outside this
  projection instead of being impersonated by a customer deposit account.

  The high serial ranges keep these endpoints disjoint from the ordinary
  population-scaled processor/business pools, whose serials start at one.
*/

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::counterparties::cash {

inline constexpr std::uint64_t kAtmTerminalBaseSerial = 2'000'000'000ULL;
inline constexpr std::uint64_t kDepositoryBaseSerial = 3'000'000'000ULL;
inline constexpr std::uint64_t kCheckCaptureBaseSerial = 4'000'000'000ULL;
inline constexpr std::uint64_t kCryptoVenueBaseSerial = 4'000'000'000ULL;
inline constexpr std::uint64_t kBillerBaseSerial = 2'000'000'000ULL;

inline constexpr std::uint64_t kFallbackEmployerSerial = 2'000'000'001ULL;
inline constexpr std::uint64_t kFallbackLandlordSerial = 2'000'000'001ULL;

[[nodiscard]] constexpr entity::Key
atmTerminal(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kAtmTerminalBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
depository(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kDepositoryBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
checkCapture(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::processor, entity::Bank::external,
                         kCheckCaptureBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
cryptoVenue(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::platform, entity::Bank::external,
                         kCryptoVenueBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key
biller(std::uint64_t oneBasedOrdinal) noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         kBillerBaseSerial + oneBasedOrdinal);
}

[[nodiscard]] constexpr entity::Key fallbackEmployer() noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         kFallbackEmployerSerial);
}

[[nodiscard]] constexpr entity::Key fallbackLandlord() noexcept {
  return entity::makeKey(entity::Role::landlord, entity::Bank::external,
                         kFallbackLandlordSerial);
}

namespace detail {

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

inline constexpr std::uint64_t kHomeTerminalDomain = 0x4154'4D48'4F4D'4501ULL;
inline constexpr std::uint64_t kVisitTerminalDomain = 0x4154'4D56'4953'4954ULL;
inline constexpr std::uint64_t kDepositoryDomain = 0x4341'5348'494E'0001ULL;

[[nodiscard]] constexpr std::uint64_t accountWord(entity::Key account) {
  return splitmix(account.number) ^
         splitmix(static_cast<std::uint64_t>(account.role) << 8U |
                  static_cast<std::uint64_t>(account.bank));
}

} // namespace detail

/* A person's local choice set holds at most this many service points. */
inline constexpr std::size_t kNearbyCount = 4;

/* One domain per rail, so a person's ATM, cash-deposit and check-capture
 * windows are independent. The depository and check pools have identical
 * counts and placement, so a shared domain would give every person the same
 * ordinals on both. */
inline constexpr std::uint64_t kWithdrawalSetDomain = 0x4154'4D53'4554'0001ULL;
inline constexpr std::uint64_t kDepositSetDomain = 0x4445'5053'4554'0001ULL;
inline constexpr std::uint64_t kCheckSetDomain = 0x4348'4B53'4554'0001ULL;

/* The nearest kNearbyCount points of one area, split at the distance cut.
 * `fixed` holds every point of the distance groups that fit wholly inside the
 * cut; `boundary` holds the one group that straddles it, in ascending pool
 * index, and each person takes `need` of its points. Residents sit at the
 * area centroid, so every point of their own area ties at distance zero: the
 * boundary is usually the area's own points. */
struct NearbyTiers {
  std::vector<entity::Key> fixed;
  std::vector<entity::Key> boundary;
  std::uint8_t need = 0;
};

/* One person's local choice set, owned by value. Only an lvalue hands out a
 * span, so no span to a temporary can escape. It is deliberately not a range:
 * std::span<const Key> converts implicitly from any contiguous range,
 * temporaries included, which would reopen that hole. */
class LocalPoints {
public:
  [[nodiscard]] std::span<const entity::Key> span() const & noexcept {
    return {keys_.data(), count_};
  }
  std::span<const entity::Key> span() const && = delete;

  [[nodiscard]] std::size_t size() const noexcept { return count_; }
  [[nodiscard]] bool empty() const noexcept { return count_ == 0U; }

  void push(entity::Key key) noexcept {
    if (count_ < keys_.size()) {
      keys_[count_++] = key;
    }
  }

private:
  std::array<entity::Key, kNearbyCount> keys_{};
  std::size_t count_ = 0;
};

/*
  Draw-free per-person nearest-point selection. Every person's set is still
  their area's nearest kNearbyCount points: groups nearer than the cut are
  kept whole, and only the tie at the cut is broken. It is broken by a window
  of consecutive boundary points starting at a (person, area, rail) hash, not
  by pool index, which gave every resident of a city the same lowest-numbered
  points. The set follows the person's event-time area, and nothing is stored
  per person.
*/
class NearbyIndex {
public:
  NearbyIndex() = default;
  explicit NearbyIndex(std::uint64_t domain) noexcept : domain_{domain} {}

  void assign(entity::geography::GeoAreaId area, NearbyTiers tiers) {
    areas_.insert_or_assign(area, std::move(tiers));
  }

  /* An area missing from the index uses the fallback pool: a pool of at most
   * kNearbyCount points is returned whole and in order, and a larger one is
   * treated as a single tie group. */
  [[nodiscard]] LocalPoints
  select(entity::geography::GeoAreaId area, entity::PersonId person,
         std::span<const entity::Key> fallback) const noexcept {
    LocalPoints out;
    if (const auto it = areas_.find(area); it != areas_.end()) {
      for (const auto key : it->second.fixed) {
        out.push(key);
      }
      appendWindow(out, it->second.boundary, it->second.need, area, person);
      return out;
    }
    if (fallback.size() <= kNearbyCount) {
      for (const auto key : fallback) {
        out.push(key);
      }
      return out;
    }
    appendWindow(out, fallback, kNearbyCount, area, person);
    return out;
  }

private:
  void appendWindow(LocalPoints &out, std::span<const entity::Key> group,
                    std::size_t need, entity::geography::GeoAreaId area,
                    entity::PersonId person) const noexcept {
    if (group.empty()) {
      return;
    }
    const auto start = static_cast<std::size_t>(
        detail::splitmix(
            detail::splitmix(static_cast<std::uint64_t>(person) ^ domain_) ^
            static_cast<std::uint64_t>(area)) %
        group.size());
    const auto take = std::min(need, group.size());
    for (std::size_t k = 0; k < take; ++k) {
      out.push(group[(start + k) % group.size()]);
    }
  }

  std::unordered_map<entity::geography::GeoAreaId, NearbyTiers> areas_;
  std::uint64_t domain_ = 0;
};

/*
  Draw-free terminal selection.  A customer has a stable primary terminal;
  82% of visits use it and the remainder use one of at most three neighboring
  service points.  This produces the repeated-location behavior of real cash
  access without making transaction order, thread count, or RNG state part of
  endpoint identity.
*/
[[nodiscard]] inline entity::Key
terminalFor(std::span<const entity::Key> terminals, entity::Key account,
            std::int64_t timestamp) noexcept {
  if (terminals.empty()) {
    return {};
  }
  if (terminals.size() == 1U) {
    return terminals.front();
  }

  const auto accountMix = detail::accountWord(account);
  const auto homeMix =
      detail::splitmix(accountMix ^ detail::kHomeTerminalDomain);
  const auto home = static_cast<std::size_t>(homeMix % terminals.size());

  const auto visitMix =
      detail::splitmix(accountMix ^ static_cast<std::uint64_t>(timestamp) ^
                       detail::kVisitTerminalDomain);
  if (visitMix % 100U < 82U) {
    return terminals[home];
  }

  const auto neighborCount = std::min<std::size_t>(3U, terminals.size() - 1U);
  const auto offset =
      1U + static_cast<std::size_t>((visitMix >> 8U) % neighborCount);
  return terminals[(home + offset) % terminals.size()];
}

/* A business keeps a stable cash-deposit location. */
[[nodiscard]] inline entity::Key
depositoryFor(std::span<const entity::Key> depositories,
              entity::Key account) noexcept {
  if (depositories.empty()) {
    return {};
  }
  const auto mixed = detail::splitmix(detail::accountWord(account) ^
                                      detail::kDepositoryDomain);
  return depositories[static_cast<std::size_t>(mixed % depositories.size())];
}

} // namespace PhantomLedger::counterparties::cash
