#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::blueprints {

using PerPersonPaydays = std::vector<std::vector<std::uint32_t>>;

[[nodiscard]] inline PerPersonPaydays
buildPaydaysByPerson(std::span<const transactions::Transaction> txns,
                     const entity::account::Registry &registry,
                     const entity::account::Lookup &lookup,
                     time::TimePoint startDate, std::int32_t days,
                     std::uint32_t personCount) {
  PerPersonPaydays out(personCount);

  if (days <= 0 || txns.empty()) {
    return out;
  }

  const std::int64_t startEpoch = time::toEpochSeconds(startDate);
  const std::int64_t endEpochExcl =
      startEpoch + static_cast<std::int64_t>(days) * 86'400;

  for (const auto &tx : txns) {
    if (!channels::isPaydayInbound(tx.session.channel)) {
      continue;
    }
    if (tx.timestamp < startEpoch || tx.timestamp >= endEpochExcl) {
      continue;
    }

    const auto it = lookup.byId.find(tx.target);
    if (it == lookup.byId.end()) {
      continue;
    }

    const auto recordIdx = it->second;
    const auto owner = registry.records[recordIdx].owner;
    if (owner == entity::invalidPerson) {
      continue;
    }

    const auto personIdx = static_cast<std::size_t>(owner - 1);
    if (personIdx >= out.size()) {
      continue;
    }

    const auto rawDay = (tx.timestamp - startEpoch) / 86'400;
    if (rawDay < 0 || rawDay >= days) {
      continue;
    }

    out[personIdx].push_back(static_cast<std::uint32_t>(rawDay));
  }

  // Per-person sort + unique. The Python returns a frozenset, so we mirror
  // its dedup semantics here. In-place sort + erase keeps the inner storage
  // contiguous — important because callers will slice these into spans for
  // the Census.
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
