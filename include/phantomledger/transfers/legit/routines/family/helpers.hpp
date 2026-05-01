#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/synth/family/pick.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::helpers {

[[nodiscard]] inline std::optional<entity::Key> resolveFamilyAccount(
    entity::PersonId person, const entity::account::Registry &accounts,
    const entity::account::Ownership &ownership, double externalP) {
  return entities::synth::family::pickMemberId(person, accounts, ownership,
                                               externalP);
}

[[nodiscard]] inline double roundCents(double amount) noexcept {
  return std::round(amount * 100.0) / 100.0;
}

[[nodiscard]] inline double sanitizeAmount(double amount,
                                           double floor) noexcept {
  const double rounded = roundCents(amount);
  return rounded >= floor ? rounded : 0.0;
}

[[nodiscard]] inline std::int64_t
randomTimestampInMonth(::PhantomLedger::time::TimePoint monthStart,
                       int monthDays, random::Rng &rng) {
  if (monthDays <= 0) {
    return ::PhantomLedger::time::toEpochSeconds(monthStart);
  }
  const auto dayOffset =
      static_cast<int>(rng.uniformInt(0, static_cast<std::int64_t>(monthDays)));
  const auto secOffset = rng.uniformInt(0, 86400);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, dayOffset));
  return base + secOffset;
}
[[nodiscard]] inline double pareto(random::Rng &rng, double xm,
                                   double alpha) noexcept {
  // 1 - U is uniform on (0, 1] which avoids log(0).
  const double u = 1.0 - rng.nextDouble();
  const double sample = xm / std::pow(u, 1.0 / alpha);

  return std::max(xm, sample);
}

[[nodiscard]] inline double
capacityFor(entity::PersonId person,
            std::span<const double> multipliers) noexcept {
  if (multipliers.empty() || !entity::valid(person) ||
      person > multipliers.size()) {
    return 1.0;
  }
  return multipliers[person - 1];
}

[[nodiscard]] inline entity::PersonId
weightedPickPerson(std::span<const entity::PersonId> pool,
                   std::span<const double> multipliers, random::Rng &rng) {
  if (pool.empty()) {
    return entity::invalidPerson;
  }
  if (pool.size() == 1) {
    return pool.front();
  }

  double total = 0.0;
  for (const auto p : pool) {
    total += capacityFor(p, multipliers);
  }

  if (total <= 0.0) {
    const auto idx = static_cast<std::size_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(pool.size())));
    return pool[idx];
  }

  const auto u = rng.nextDouble() * total;
  double accum = 0.0;
  for (const auto p : pool) {
    accum += capacityFor(p, multipliers);
    if (u < accum) {
      return p;
    }
  }
  return pool.back(); // floating-point safety net.
}
[[nodiscard]] inline std::optional<entity::Key>
pickEducationMerchant(const entity::merchant::Catalog &catalog,
                      random::Rng &rng) {
  using ::PhantomLedger::merchants::Category;

  std::size_t educationCount = 0;
  for (const auto &record : catalog.records) {
    if (record.category == Category::education) {
      ++educationCount;
    }
  }

  if (educationCount == 0) {
    return std::nullopt;
  }

  const auto target = static_cast<std::size_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(educationCount)));

  std::size_t seen = 0;
  for (const auto &record : catalog.records) {
    if (record.category != Category::education) {
      continue;
    }
    if (seen == target) {
      return record.counterpartyId;
    }
    ++seen;
  }
  return std::nullopt; // unreachable.
}

} // namespace PhantomLedger::transfers::legit::routines::family::helpers
