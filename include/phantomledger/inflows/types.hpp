#pragma once
/*
 * inflows/types.hpp — shared inflow data and helpers.
 *
 * This file keeps the inflow module's shared types separated by role:
 *
 *   - Timeframe      : simulation dates / month anchors
 *   - Entropy        : deterministic RNG inputs
 *   - Population     : person/account/persona lookups
 *   - Counterparties : employers, landlords, and external pools
 *   - InflowSnapshot : composed read-only inflow state
 */

#include "phantomledger/entities/accounts/ownership.hpp"
#include "phantomledger/entities/accounts/registry.hpp"
#include "phantomledger/entities/behavior/assignment.hpp"
#include "phantomledger/entities/counterparties/pool.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/landlords/class.hpp"
#include "phantomledger/random/factory.hpp"
#include "phantomledger/recurring/policy.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::inflows {

using Key = entities::identifier::Key;
using PersonId = entities::identifier::PersonId;
using TimePoint = time::TimePoint;

// ---------------------------------------------------------------
// Shared lookup aliases
// ---------------------------------------------------------------

using LandlordTypes =
    std::unordered_map<Key, entities::landlords::Class, std::hash<Key>>;

using HubAccounts = std::unordered_set<Key, std::hash<Key>>;

// ---------------------------------------------------------------
// Timeframe
// ---------------------------------------------------------------

struct Timeframe {
  TimePoint startDate{};
  int days = 0;
  std::vector<TimePoint> monthStarts;

  [[nodiscard]] TimePoint end() const noexcept {
    return time::addDays(startDate, days);
  }

  [[nodiscard]] bool contains(TimePoint ts) const noexcept {
    return ts >= startDate && ts < end();
  }
};

// ---------------------------------------------------------------
// Entropy
// ---------------------------------------------------------------

struct Entropy {
  std::uint64_t seed = 0;
  random::RngFactory factory{0};
};

// ---------------------------------------------------------------
// Population
// ---------------------------------------------------------------

struct Population {
  std::uint32_t count = 0;

  const entities::accounts::Registry *accounts = nullptr;
  const entities::accounts::Ownership *ownership = nullptr;
  const entities::behavior::Assignment *personas = nullptr;

  HubAccounts hubs;

  [[nodiscard]] bool exists(PersonId person) const noexcept {
    return person >= 1 && person <= count;
  }

  [[nodiscard]] bool hasAccount(PersonId person) const noexcept {
    assert(ownership != nullptr);
    assert(exists(person));
    assert(static_cast<std::size_t>(person) < ownership->byPersonOffset.size());

    const auto start = ownership->byPersonOffset[person - 1];
    const auto end = ownership->byPersonOffset[person];
    return start != end;
  }

  [[nodiscard]] std::span<const std::uint32_t>
  accountIndices(PersonId person) const noexcept {
    assert(ownership != nullptr);
    assert(exists(person));
    assert(static_cast<std::size_t>(person) < ownership->byPersonOffset.size());

    const auto start = ownership->byPersonOffset[person - 1];
    const auto end = ownership->byPersonOffset[person];
    return {ownership->byPersonIndex.data() + start, end - start};
  }

  [[nodiscard]] Key primary(PersonId person) const noexcept {
    assert(accounts != nullptr);
    assert(ownership != nullptr);
    assert(hasAccount(person));

    const auto ix = ownership->primaryIndex(person);
    assert(ix < accounts->records.size());
    return accounts->records[ix].id;
  }

  [[nodiscard]] bool isHub(PersonId person) const noexcept {
    assert(hasAccount(person));
    return hubs.contains(primary(person));
  }

  [[nodiscard]] personas::Type persona(PersonId person) const noexcept {
    assert(personas != nullptr);
    assert(exists(person));
    assert(static_cast<std::size_t>(person - 1) < personas->byPerson.size());

    return personas->byPerson[person - 1];
  }

  [[nodiscard]] bool owns(PersonId person, const Key &id) const noexcept {
    assert(accounts != nullptr);

    for (const auto ix : accountIndices(person)) {
      assert(ix < accounts->records.size());
      if (accounts->records[ix].id == id) {
        return true;
      }
    }

    return false;
  }
};

// ---------------------------------------------------------------
// Counterparties
// ---------------------------------------------------------------

struct Counterparties {
  const entities::counterparties::Pool *pools = nullptr;

  std::span<const Key> employers;
  std::span<const Key> landlords;

  const LandlordTypes *landlordTypes = nullptr;

  [[nodiscard]] bool hasPools() const noexcept { return pools != nullptr; }

  [[nodiscard]] std::optional<entities::landlords::Class>
  landlordClass(const Key &landlord) const noexcept {
    if (landlordTypes == nullptr) {
      return std::nullopt;
    }

    const auto it = landlordTypes->find(landlord);
    if (it == landlordTypes->end()) {
      return std::nullopt;
    }

    return it->second;
  }
};

// ---------------------------------------------------------------
// InflowSnapshot
// ---------------------------------------------------------------

struct InflowSnapshot {
  Timeframe timeframe;
  Entropy entropy;
  Population population;
  Counterparties counterparties;
  const recurring::Policy *recurringPolicy = nullptr;

  [[nodiscard]] bool hasRecurringPolicy() const noexcept {
    return recurringPolicy != nullptr;
  }

  [[nodiscard]] const recurring::Policy &policy() const noexcept {
    assert(recurringPolicy != nullptr);
    return *recurringPolicy;
  }
};

/// Canonical deterministic ordering for generated funds transfers.
inline void sortTransfers(std::vector<transactions::Transaction> &txns) {
  std::sort(
      txns.begin(), txns.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

} // namespace PhantomLedger::inflows
