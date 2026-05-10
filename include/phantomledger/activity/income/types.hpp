#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::nonNegative("days", days); });
  }
};

struct Entropy {
  random::RngFactory factory{0};
};

// ---------------------------------------------------------------
// Population
// ---------------------------------------------------------------

class Population {
public:
  Population(const entity::account::Registry &accounts,
             const entity::account::Ownership &ownership,
             const entity::behavior::Assignment &personas,
             HubAccounts hubs = {}) noexcept
      : count_(static_cast<std::uint32_t>(personas.byPerson.size())),
        hubs_(std::move(hubs)), accounts_(accounts), ownership_(ownership),
        personas_(personas) {}

  [[nodiscard]] const entity::account::Registry &accounts() const noexcept {
    return accounts_;
  }

  [[nodiscard]] const entity::account::Ownership &ownership() const noexcept {
    return ownership_;
  }
  [[nodiscard]] std::uint32_t count() const noexcept { return count_; }

  [[nodiscard]] const HubAccounts &hubs() const noexcept { return hubs_; }

  [[nodiscard]] const entity::behavior::Assignment &personas() const noexcept {
    return personas_;
  }

  [[nodiscard]] bool exists(PersonId person) const noexcept {
    return person >= 1 && person <= count_;
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

    return hubs_.contains(primary(person));
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

private:
  std::uint32_t count_ = 0;
  HubAccounts hubs_;
  const entity::account::Registry &accounts_;
  const entity::account::Ownership &ownership_;
  const entity::behavior::Assignment &personas_;
};

struct PayrollCounterparties {
  std::span<const Key> employers;

  [[nodiscard]] bool hasEmployers() const noexcept {
    return !employers.empty();
  }
};

struct RentCounterparties {
  std::span<const Key> landlords;
  const LandlordTypes *landlordTypes = nullptr;

  [[nodiscard]] bool hasLandlords() const noexcept {
    return !landlords.empty();
  }

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

class RevenueCounterparties {
public:
  const entity::counterparty::Directory *directory = nullptr;

  [[nodiscard]] bool available() const noexcept { return directory != nullptr; }

  [[nodiscard]] std::span<const Key> clients() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    return view(directory->clients.accounts.all);
  }

  [[nodiscard]] std::span<const Key> platforms() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    return view(directory->external.platforms);
  }

  [[nodiscard]] std::span<const Key> processors() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    return view(directory->external.processors);
  }

  [[nodiscard]] std::span<const Key> ownerBusinesses() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    return view(directory->external.ownerBusinesses);
  }

  [[nodiscard]] std::span<const Key> brokerages() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    return view(directory->external.brokerages);
  }

private:
  [[nodiscard]] static std::span<const Key>
  view(const std::vector<Key> &keys) noexcept {
    return {keys.data(), keys.size()};
  }
};

/// Canonical deterministic ordering for generated funds transfers.
inline void sortTransfers(std::vector<transactions::Transaction> &txns) {
  std::sort(
      txns.begin(), txns.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

} // namespace PhantomLedger::inflows
