#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

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

struct HubSelectionRules {
  std::uint32_t populationCount = 0;
  double fraction = 0.01;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("hubSelection.fraction", fraction); });
  }
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
};

struct PersonaCatalog {
  const entities::synth::personas::Pack *pack = nullptr;
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
  std::vector<entity::Key> hubAccounts;
  std::unordered_set<entity::Key> hubSet;

  std::vector<entity::Key> employers;
  std::vector<entity::Key> landlords;
  std::unordered_map<entity::Key, entity::landlord::Type> landlordTypeOf;

  std::vector<entity::Key> billerAccounts;
  entity::Key issuerAcct{};

  [[nodiscard]] bool isHub(const entity::Key &key) const {
    return hubSet.contains(key);
  }

  [[nodiscard]] const entity::Key *firstHub() const noexcept {
    return hubAccounts.empty() ? nullptr : &hubAccounts.front();
  }
};

struct PersonaAccess {
  std::optional<entities::synth::personas::Pack> ownedPack{};
  const entities::synth::personas::Pack *pack = nullptr;

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

  LegitBlueprint &addCounterparties(random::Rng &rng, AccountCensus census,
                                    CounterpartyPools counterparties,
                                    HubSelectionRules hubs);
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
