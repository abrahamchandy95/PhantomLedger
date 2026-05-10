#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::blueprints {

using PerPersonPaydays = std::vector<std::vector<std::uint32_t>>;

struct LegitAccountIndex {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Lookup *lookup = nullptr;

  [[nodiscard]] entity::PersonId
  ownerOf(const entity::Key &target) const noexcept {
    if (registry == nullptr || lookup == nullptr) {
      return entity::invalidPerson;
    }
    const auto it = lookup->byId.find(target);
    if (it == lookup->byId.end()) {
      return entity::invalidPerson;
    }
    const auto recordIdx = it->second;
    if (recordIdx >= registry->records.size()) {
      return entity::invalidPerson;
    }
    return registry->records[recordIdx].owner;
  }
};

[[nodiscard]] inline PerPersonPaydays
buildPaydaysByPerson(std::span<const transactions::Transaction> txns,
                     LegitAccountIndex index, time::Window window,
                     std::uint32_t personCount) {
  PerPersonPaydays out(personCount);

  if (window.days <= 0 || txns.empty()) {
    return out;
  }

  const std::int64_t startEpoch = time::toEpochSeconds(window.start);
  const std::int64_t endEpochExcl =
      startEpoch + static_cast<std::int64_t>(window.days) * 86'400;

  for (const auto &tx : txns) {
    if (!channels::isPaydayInbound(tx.session.channel)) {
      continue;
    }
    if (tx.timestamp < startEpoch || tx.timestamp >= endEpochExcl) {
      continue;
    }

    const auto owner = index.ownerOf(tx.target);
    if (owner == entity::invalidPerson) {
      continue;
    }

    const auto personIdx = static_cast<std::size_t>(owner - 1);
    if (personIdx >= out.size()) {
      continue;
    }

    const auto rawDay = (tx.timestamp - startEpoch) / 86'400;
    if (rawDay < 0 || rawDay >= window.days) {
      continue;
    }

    out[personIdx].push_back(static_cast<std::uint32_t>(rawDay));
  }

  for (auto &days_ : out) {
    if (days_.empty()) {
      continue;
    }
    std::ranges::sort(days_);
    const auto dup = std::ranges::unique(days_).begin();
    days_.erase(dup, days_.end());
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::blueprints
