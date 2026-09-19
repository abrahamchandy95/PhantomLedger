#pragma once

#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/landlords.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::income {

using Key = entity::Key;
using PersonId = entity::PersonId;
using TimePoint = time::TimePoint;

// ---------------------------------------------------------------
// Shared lookup aliases
// ---------------------------------------------------------------

using LandlordTypes =
    std::unordered_map<Key, entity::landlord::Type, std::hash<Key>>;

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
             const std::vector<synth::personas::timeline::Timeline> *timelines =
                 nullptr) noexcept
      : count_(static_cast<std::uint32_t>(personas.byPerson.size())),
        accounts_(accounts), ownership_(ownership), personas_(personas),
        timelines_(timelines) {}

  [[nodiscard]] const entity::account::Registry &accounts() const noexcept {
    return accounts_;
  }

  [[nodiscard]] const entity::account::Ownership &ownership() const noexcept {
    return ownership_;
  }
  [[nodiscard]] std::uint32_t count() const noexcept { return count_; }

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

  [[nodiscard]] personas::Type persona(PersonId person) const noexcept {
    assert(exists(person));
    assert(static_cast<std::size_t>(person - 1) < personas_.byPerson.size());

    return personas_.byPerson[person - 1];
  }

  // H2 step 2b: the persona timeline (Pack::timelines). Salary
  // selection/spans and the revenue month gate read persona-AT-DATE
  // through this; hard-required where the income switch runs.
  [[nodiscard]] bool hasTimelines() const noexcept {
    return timelines_ != nullptr && timelines_->size() >= count_;
  }

  [[nodiscard]] const synth::personas::timeline::Timeline &
  timeline(PersonId person) const {
    assert(exists(person));
    if (!hasTimelines()) {
      throw std::invalid_argument(
          "income::Population requires the persona-timeline carrier "
          "(Pack::timelines, H2 step 2b)");
    }
    return (*timelines_)[person - 1];
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
  const entity::account::Registry &accounts_;
  const entity::account::Ownership &ownership_;
  const entity::behavior::Assignment &personas_;
  const std::vector<synth::personas::timeline::Timeline> *timelines_ = nullptr;
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
  using NearbyDepositories = std::unordered_map<
      entity::geography::GeoAreaId, std::vector<entity::Key>>;

  const entity::counterparty::Directory *directory = nullptr;

  // Registered external branch/depository endpoints. They identify where
  // cash entered the modeled customer ledger; they do not supply a balance.
  std::span<const Key> cashDepositPoints{};
  const NearbyDepositories *nearbyCashDepositPoints = nullptr;
  std::span<const entity::geography::GeoAreaId> homeAreas{};
  const entity::parties::relocation::Schedule *relocation = nullptr;

  [[nodiscard]] bool available() const noexcept { return directory != nullptr; }

  [[nodiscard]] std::span<const Key> cashDepositories() const noexcept {
    return cashDepositPoints;
  }

  [[nodiscard]] std::span<const Key>
  cashDepositoriesFor(PersonId person, std::int64_t timestamp) const noexcept {
    auto area = entity::geography::invalidGeoArea;
    if (person != entity::invalidPerson && person <= homeAreas.size()) {
      area = homeAreas[person - 1U];
    }
    if (relocation != nullptr) {
      const auto current = relocation->areaAt(person, timestamp);
      if (entity::geography::validArea(current)) {
        area = current;
      }
    }
    if (nearbyCashDepositPoints != nullptr) {
      if (const auto it = nearbyCashDepositPoints->find(area);
          it != nearbyCashDepositPoints->end() && !it->second.empty()) {
        return it->second;
      }
    }
    return cashDepositPoints;
  }

  [[nodiscard]] std::span<const Key> clients() const noexcept {
    if (directory == nullptr) {
      return {};
    }
    // Ownerless internal client accounts have no operating-balance model.
    // Revenue entering this customer-ledger projection therefore originates
    // from the explicitly external side of the directory.
    return view(directory->clients.accounts.external);
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

} // namespace PhantomLedger::activity::income
