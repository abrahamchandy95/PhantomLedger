#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace PhantomLedger::exporter::common {

namespace entity = ::PhantomLedger::entity;
namespace time = ::PhantomLedger::time;

[[nodiscard]] inline bool isExternalKey(const entity::Key &k) noexcept {
  return k.bank == entity::Bank::external;
}

[[nodiscard]] inline std::string_view accountType(const entity::Key &id,
                                                  bool isCard) noexcept {
  if (isCard || id.role == entity::Role::card) {
    return "credit";
  }
  if (id.role == entity::Role::business && id.bank == entity::Bank::internal) {
    return "business_checking";
  }
  if (id.role == entity::Role::brokerage && id.bank == entity::Bank::internal) {
    return "brokerage";
  }
  return "checking";
}

template <class CardsT>
[[nodiscard]] std::unordered_set<entity::Key>
collectCardIds(const CardsT &cards) {
  std::unordered_set<entity::Key> out;
  out.reserve(cards.records.size());
  for (const auto &rec : cards.records) {
    out.insert(rec.key);
  }
  return out;
}

template <class EntitiesT, class Fn>
void forEachInternalOwnership(const EntitiesT &entities, Fn &&fn) {
  const auto &ownership = entities.accounts.ownership;
  const auto &records = entities.accounts.registry.records;
  const auto numPersons = ownership.byPersonOffset.empty()
                              ? std::size_t{0}
                              : ownership.byPersonOffset.size() - 1;
  for (std::size_t i = 0; i < numPersons; ++i) {
    const auto offset = ownership.byPersonOffset[i];
    const auto end = ownership.byPersonOffset[i + 1];
    const auto pid = static_cast<entity::PersonId>(i + 1);
    for (auto j = offset; j < end; ++j) {
      const auto regIdx = ownership.byPersonIndex[j];
      const auto &rec = records[regIdx];
      if ((rec.flags & entity::account::bit(entity::account::Flag::external)) !=
          0) {
        continue;
      }
      fn(pid, rec.id);
    }
  }
}

template <class EntitiesT, class SharedContextT>
[[nodiscard]] inline std::size_t
estimateIdentityRowCount(const EntitiesT &entities,
                         const SharedContextT &ctx) noexcept {
  return entities.people.roster.count + ctx.counterpartyIds.size() + 21U;
}

[[nodiscard]] inline std::string_view joinName(std::string &scratch,
                                               std::string_view firstName,
                                               std::string_view lastName) {
  scratch.clear();
  scratch.reserve(firstName.size() + lastName.size() + 1);
  scratch.append(firstName);
  if (!firstName.empty() && !lastName.empty()) {
    scratch.push_back(' ');
  }
  scratch.append(lastName);
  return scratch;
}

template <class AddressLike>
[[nodiscard]] inline std::string_view joinAddress(std::string &scratch,
                                                  const AddressLike &a) {
  scratch.clear();
  scratch.reserve(a.streetLine1.size() + a.city.size() + a.state.size() +
                  a.postalCode.size() + 4);
  scratch.append(a.streetLine1);
  scratch.push_back(' ');
  scratch.append(a.city);
  scratch.push_back(' ');
  scratch.append(a.state);
  scratch.push_back(' ');
  scratch.append(a.postalCode);
  return scratch;
}

[[nodiscard]] inline std::string_view
makeWatchlistId(std::string &scratch, std::string_view cidView) {
  scratch.clear();
  scratch.reserve(3 + cidView.size());
  scratch.append("WL_").append(cidView);
  return scratch;
}

[[nodiscard]] inline std::string_view
osForDeviceType(std::string_view typeName) noexcept {
  if (typeName == "android") {
    return "Android";
  }
  if (typeName == "ios") {
    return "iOS";
  }
  if (typeName == "web" || typeName == "browser") {
    return "Browser";
  }
  if (typeName == "desktop") {
    return "Windows";
  }
  return "Unknown";
}

struct SeenWindow {
  time::TimePoint firstSeen{};
  time::TimePoint lastSeen{};
  std::uint32_t count = 0;
};

inline void recordSeen(SeenWindow &slot, time::TimePoint fs,
                       time::TimePoint ls) noexcept {
  if (slot.count == 0) {
    slot.firstSeen = fs;
    slot.lastSeen = ls;
  } else {
    if (fs < slot.firstSeen) {
      slot.firstSeen = fs;
    }
    if (ls > slot.lastSeen) {
      slot.lastSeen = ls;
    }
  }
  ++slot.count;
}

} // namespace PhantomLedger::exporter::common
