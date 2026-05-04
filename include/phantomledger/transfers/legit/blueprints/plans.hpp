#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

// ---------------------------------------------------------------------------
// OwnedAccountSlices — compact per-person account record slices
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// CounterpartyPlan — flat, fast-access counterparty buckets
// ---------------------------------------------------------------------------

struct CounterpartyPlan {
  std::vector<entity::Key> hubAccounts;
  std::unordered_set<entity::Key> hubSet;

  std::vector<entity::Key> employers;

  std::vector<entity::Key> landlords;

  std::unordered_map<entity::Key, entity::landlord::Type> landlordTypeOf;

  std::vector<entity::Key> billerAccounts;
  entity::Key issuerAcct{};
};

// ---------------------------------------------------------------------------
// PersonaPlan — pack + the canonical name list for compatibility shims
// ---------------------------------------------------------------------------

struct PersonaPlan {
  std::optional<entities::synth::personas::Pack> ownedPack{};
  const entities::synth::personas::Pack *pack = nullptr;

  std::vector<std::string_view> personaNames;
};

// ---------------------------------------------------------------------------
// LegitBuildPlan — the full bundle a routine receives
// ---------------------------------------------------------------------------

struct LegitBuildPlan {
  time::TimePoint startDate{};
  std::int32_t days = 0;
  std::uint64_t seed = 0;

  const entity::account::Registry *allAccounts = nullptr;
  OwnedAccountSlices ownedAccountSlices;

  std::vector<entity::PersonId> persons;

  // Calendar month anchors covering [startDate, startDate + days).
  std::vector<time::TimePoint> monthStarts;

  std::unordered_map<entity::PersonId, std::uint32_t> primaryAcctRecordIx;

  CounterpartyPlan counterparties;
  PersonaPlan personas;

  /// Compatibility shim for older generators that still expect
  /// `plan.paydays`. These are month anchors, not true payroll schedules.
  [[nodiscard]] const std::vector<time::TimePoint> &paydays() const noexcept {
    return monthStarts;
  }
};

// ---------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------

[[nodiscard]] LegitBuildPlan
buildLegitPlan(random::Rng &rng, LegitTimeframe timeframe, AccountCensus census,
               CounterpartyPools counterparties = {},
               PersonaCatalog personas = {}, HubSelectionRules hubs = {});

} // namespace PhantomLedger::transfers::legit::blueprints
