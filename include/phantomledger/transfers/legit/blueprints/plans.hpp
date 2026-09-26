#pragma once

#include "phantomledger/entities/counterparties/cash_points.hpp"
#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/landlords.hpp"
#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/personas/pack.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::blueprints {

struct LegitTimeframe {
  time::Window window{};
  std::uint64_t seed = 0;
};

struct AccountCensus {
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;

  [[nodiscard]] bool hasAccounts() const noexcept {
    return accounts != nullptr && !accounts->records.empty();
  }

  [[nodiscard]] bool hasOwnership() const noexcept {
    return ownership != nullptr;
  }
};

struct CounterpartyPools {
  const entity::counterparty::Directory *directory = nullptr;
  const entity::landlord::Roster *landlords = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};
  const entity::parties::relocation::Schedule *relocation = nullptr;
};

struct PersonaCatalog {
  const synth::personas::Pack *pack = nullptr;
};

struct TransferCalendar {
  time::TimePoint startDate{};
  std::int32_t days = 0;

  // Calendar month anchors covering [startDate, startDate + days).
  std::vector<time::TimePoint> monthStarts;

  [[nodiscard]] time::Window window() const noexcept {
    return time::Window{.start = startDate, .days = days};
  }

  [[nodiscard]] bool hasMonths() const noexcept { return !monthStarts.empty(); }
};

struct OwnedAccountSlices {
  std::vector<std::uint32_t> offset;
  std::vector<std::uint32_t> recordIx;

  [[nodiscard]] bool empty() const noexcept { return recordIx.empty(); }

  [[nodiscard]] std::span<const std::uint32_t>
  recordsFor(entity::PersonId person) const noexcept {
    if (person == entity::invalidPerson || person == 0 ||
        static_cast<std::size_t>(person) >= offset.size()) {
      return {};
    }

    const auto start = offset[person - 1];
    const auto end = offset[person];
    if (start == end) {
      return {};
    }
    return std::span<const std::uint32_t>(recordIx.data() + start,
                                          recordIx.data() + end);
  }
};

struct AccountAccess {
  const entity::account::Registry *registry = nullptr;
  OwnedAccountSlices ownedSlices;
  std::vector<entity::PersonId> persons;
  std::unordered_map<entity::PersonId, std::uint32_t> primaryRecordIx;

  [[nodiscard]] bool hasRegistry() const noexcept {
    return registry != nullptr;
  }

  [[nodiscard]] std::span<const entity::PersonId> personIds() const noexcept {
    return {persons.data(), persons.size()};
  }

  [[nodiscard]] std::span<const std::uint32_t>
  recordsFor(entity::PersonId person) const noexcept {
    return ownedSlices.recordsFor(person);
  }

  [[nodiscard]] const entity::account::Record *
  record(std::uint32_t recordIx) const noexcept {
    if (registry == nullptr || recordIx >= registry->records.size()) {
      return nullptr;
    }
    return &registry->records[recordIx];
  }

  [[nodiscard]] const entity::account::Record *
  primaryRecord(entity::PersonId person) const noexcept {
    const auto it = primaryRecordIx.find(person);
    if (it == primaryRecordIx.end()) {
      return nullptr;
    }
    return record(it->second);
  }

  [[nodiscard]] const entity::Key *
  primaryKey(entity::PersonId person) const noexcept {
    const auto *rec = primaryRecord(person);
    return rec == nullptr ? nullptr : &rec->id;
  }
};

struct CounterpartyAccess {
  // Both carry their size law (counterparty-sizes-2026-09): payroll and
  // leases pick through it, never uniformly over the keys.
  entity::counterparty::SizedKeys employers;

  entity::counterparty::SizedKeys landlords;
  std::unordered_map<entity::Key, entity::landlord::Type> landlordTypeOf;

  // External, ownerless context endpoints. They cross the modeled customer
  // ledger boundary and therefore never carry a ledger balance.
  std::vector<entity::Key> cashWithdrawalPoints;
  std::vector<entity::Key> cashDepositPoints;
  std::vector<entity::Key> checkDepositPoints;
  std::vector<entity::Key> cryptoVenues;
  ::PhantomLedger::counterparties::cash::NearbyIndex nearbyWithdrawalPoints;
  ::PhantomLedger::counterparties::cash::NearbyIndex nearbyCashDepositPoints;
  ::PhantomLedger::counterparties::cash::NearbyIndex nearbyCheckDepositPoints;
  std::span<const entity::geography::GeoAreaId> homeAreas{};
  const entity::parties::relocation::Schedule *relocation = nullptr;

  std::vector<entity::Key> billerAccounts;

  // Each returns the person's own nearest points at the event-time area, by
  // value: bind the result to a named local before taking its span().
  [[nodiscard]] ::PhantomLedger::counterparties::cash::LocalPoints
  withdrawalPointsFor(entity::PersonId person,
                      std::int64_t timestamp) const noexcept {
    return localPointsFor(nearbyWithdrawalPoints, cashWithdrawalPoints, person,
                          timestamp);
  }

  [[nodiscard]] ::PhantomLedger::counterparties::cash::LocalPoints
  depositPointsFor(entity::PersonId person,
                   std::int64_t timestamp) const noexcept {
    return localPointsFor(nearbyCashDepositPoints, cashDepositPoints, person,
                          timestamp);
  }

  [[nodiscard]] ::PhantomLedger::counterparties::cash::LocalPoints
  checkDepositPointsFor(entity::PersonId person,
                        std::int64_t timestamp) const noexcept {
    return localPointsFor(nearbyCheckDepositPoints, checkDepositPoints, person,
                          timestamp);
  }

private:
  [[nodiscard]] ::PhantomLedger::counterparties::cash::LocalPoints
  localPointsFor(
      const ::PhantomLedger::counterparties::cash::NearbyIndex &nearby,
      const std::vector<entity::Key> &fallback, entity::PersonId person,
      std::int64_t timestamp) const noexcept {
    auto area = entity::geography::invalidGeoArea;
    if (person != entity::invalidPerson && person <= homeAreas.size()) {
      area = homeAreas[person - 1U];
    }
    if (relocation != nullptr) {
      const auto atDate = relocation->areaAt(person, timestamp);
      if (entity::geography::validArea(atDate)) {
        area = atDate;
      }
    }

    return nearby.select(area, person, fallback);
  }

public:
};

struct PersonaAccess {
  std::optional<synth::personas::Pack> ownedPack{};
  const synth::personas::Pack *pack = nullptr;

  std::vector<std::string_view> names;

  [[nodiscard]] bool hasPack() const noexcept { return pack != nullptr; }

  [[nodiscard]] std::uint32_t personCount() const noexcept {
    if (pack == nullptr) {
      return 0;
    }
    return static_cast<std::uint32_t>(pack->assignment.byPerson.size());
  }
};

class LegitBlueprint {
public:
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

  [[nodiscard]] const TransferCalendar &calendar() const noexcept {
    return calendar_;
  }

  [[nodiscard]] time::TimePoint startDate() const noexcept {
    return calendar_.startDate;
  }

  [[nodiscard]] std::int32_t days() const noexcept { return calendar_.days; }

  [[nodiscard]] const std::vector<time::TimePoint> &
  monthStarts() const noexcept {
    return calendar_.monthStarts;
  }

  /// Compatibility shim for older generators that still expect paydays.
  /// These are month anchors, not true payroll schedules.
  [[nodiscard]] const std::vector<time::TimePoint> &paydays() const noexcept {
    return calendar_.monthStarts;
  }

  [[nodiscard]] const AccountAccess &accounts() const noexcept {
    return accounts_;
  }

  [[nodiscard]] const entity::account::Registry *allAccounts() const noexcept {
    return accounts_.registry;
  }

  [[nodiscard]] const OwnedAccountSlices &ownedAccountSlices() const noexcept {
    return accounts_.ownedSlices;
  }

  [[nodiscard]] const std::vector<entity::PersonId> &persons() const noexcept {
    return accounts_.persons;
  }

  [[nodiscard]] const std::unordered_map<entity::PersonId, std::uint32_t> &
  primaryAcctRecordIx() const noexcept {
    return accounts_.primaryRecordIx;
  }

  [[nodiscard]] const CounterpartyAccess &counterparties() const noexcept {
    return counterparties_;
  }

  [[nodiscard]] const PersonaAccess &personas() const noexcept {
    return personas_;
  }

  [[nodiscard]] CounterpartyAccess takeCounterparties() && noexcept {
    return std::move(counterparties_);
  }

  LegitBlueprint &addCounterparties(random::Rng &rng,
                                    CounterpartyPools counterparties);
  LegitBlueprint &addPersonas(random::Rng &rng, LegitTimeframe timeframe,
                              PersonaCatalog personas);

private:
  friend LegitBlueprint buildLegitBlueprint(LegitTimeframe timeframe,
                                            AccountCensus census);

  std::uint64_t seed_ = 0;
  TransferCalendar calendar_{};
  AccountAccess accounts_{};
  CounterpartyAccess counterparties_{};
  PersonaAccess personas_{};
};

[[nodiscard]] LegitBlueprint buildLegitBlueprint(LegitTimeframe timeframe,
                                                 AccountCensus census);

} // namespace PhantomLedger::transfers::legit::blueprints
