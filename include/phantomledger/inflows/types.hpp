#pragma once
/*
 * inflows/types.hpp — shared inflow data and helpers.
 *
 * Structure:
 *   - Timeframe      : simulation dates / month anchors
 *   - Entropy        : deterministic RNG inputs
 *   - Population     : person/account/persona lookups (refs, not ptrs)
 *   - Counterparties : employers, landlords, and external pools
 *   - InflowSnapshot : composed read-only inflow state
 *
 * Population change vs. prior revision: the three backing tables
 * (Registry, Ownership, Assignment) are references instead of raw
 * pointers. This removes ~10 `assert(x != nullptr)` checks across
 * the struct and pushes the invariant to the type system. The cost
 * is that callers must construct Population with the tables already
 * in scope, which every current call site already does.
 */

#include "phantomledger/entities/accounts/account.hpp"
#include "phantomledger/entities/behavior/behavior.hpp"
#include "phantomledger/entities/counterparties/pool.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/landlords/class.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/recurring/policy.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
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

using Key = entity::Key;
using PersonId = entity::PersonId;
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

class Population {
public:
  Population(std::uint32_t count, const entity::account::Registry &accounts,
             const entity::account::Ownership &ownership,
             const entity::behavior::Assignment &personas,
             HubAccounts hubs = {}) noexcept
      : count(count), hubs(std::move(hubs)), accounts_(accounts),
        ownership_(ownership), personas_(personas) {}

  [[nodiscard]] const entity::account::Registry &accounts() const noexcept {
    return accounts_;
  }
  [[nodiscard]] const entity::account::Ownership &ownership() const noexcept {
    return ownership_;
  }
  [[nodiscard]] const entity::behavior::Assignment &personas() const noexcept {
    return personas_;
  }

  [[nodiscard]] bool exists(PersonId person) const noexcept {
    return person >= 1 && person <= count;
  }

  [[nodiscard]] bool hasAccount(PersonId person) const noexcept {
    assert(exists(person));
    assert(static_cast<std::size_t>(person) < ownership_.byPersonOffset.size());

    const auto start = ownership_.byPersonOffset[person - 1];
    const auto end = ownership_.byPersonOffset[person];
    return start != end;
  }

  [[nodiscard]] std::span<const std::uint32_t>
  accountIndices(PersonId person) const noexcept {
    assert(exists(person));
    assert(static_cast<std::size_t>(person) < ownership_.byPersonOffset.size());

    const auto start = ownership_.byPersonOffset[person - 1];
    const auto end = ownership_.byPersonOffset[person];
    return {ownership_.byPersonIndex.data() + start, end - start};
  }

  [[nodiscard]] Key primary(PersonId person) const noexcept {
    assert(hasAccount(person));
    const auto ix = ownership_.primaryIndex(person);
    assert(ix < accounts_.records.size());
    return accounts_.records[ix].id;
  }

  [[nodiscard]] bool isHub(PersonId person) const noexcept {
    assert(hasAccount(person));
    return hubs.contains(primary(person));
  }

  [[nodiscard]] personas::Type persona(PersonId person) const noexcept {
    assert(exists(person));
    assert(static_cast<std::size_t>(person - 1) < personas_.byPerson.size());
    return personas_.byPerson[person - 1];
  }

  [[nodiscard]] bool owns(PersonId person, const Key &id) const noexcept {
    for (const auto ix : accountIndices(person)) {
      assert(ix < accounts_.records.size());
      if (accounts_.records[ix].id == id) {
        return true;
      }
    }
    return false;
  }

  // Member ordering note:
  //   C++ initializes members in declaration order, not
  //   initializer-list order. The constructor's initializer list
  //   therefore runs `count, hubs, accounts_, ownership_, personas_`
  //   to match the order of the declarations below. If you change
  //   either side, keep them in sync or clang will warn with
  //   -Wreorder-ctor.
  //
  //   `count` and `hubs` stay public because existing call sites
  //   read them directly and there is no invariant between them and
  //   the refs.

  std::uint32_t count = 0;
  HubAccounts hubs;

private:
  const entity::account::Registry &accounts_;
  const entity::account::Ownership &ownership_;
  const entity::behavior::Assignment &personas_;
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
