#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
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
    std::unordered_map<Key, entity::landlord::Type, std::hash<Key>>;

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
  const entity::counterparty::Pool *pools = nullptr;

  std::span<const Key> employers;
  std::span<const Key> landlords;

  const LandlordTypes *landlordTypes = nullptr;

  [[nodiscard]] bool hasPools() const noexcept { return pools != nullptr; }

  [[nodiscard]] std::optional<entity::landlord::Type>
  landlordType(const Key &landlord) const noexcept {
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
