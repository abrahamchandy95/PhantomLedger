#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "phantomledger/entities/synth/personas/pack.hpp"

namespace PhantomLedger::transfers::legit::blueprints {

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

[[nodiscard]] LegitBuildPlan buildLegitPlan(const Timeline &timeline,
                                            const Network &network,
                                            const Macro &macro,
                                            const Overrides &overrides);

} // namespace PhantomLedger::transfers::legit::blueprints
