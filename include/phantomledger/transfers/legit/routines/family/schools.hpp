#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::family::schools {

/* The schools a tuition plan can pay (tuition-payee-2026-09).
 *
 * Tuition is a school payment plan, so a student pays ONE school for the
 * whole plan, and it must be open on the date of every installment: an
 * education record whose operating interval covers [first, last], the plan's
 * first and last installment. The student's home area comes first; when it
 * holds no such record the pool is every open education record, online ones
 * included. The pick is uniform within the pool that applies and spends
 * exactly one bounded draw, or none when no education record is open. The
 * caller hands it the student's own lane, so that draw moves nothing else.
 *
 * Built once per family pass from the catalogue and dropped with it. It
 * holds education records only, so it is catalogue-sized, never
 * population-sized. */
class Directory {
public:
  explicit Directory(const entity::merchant::Catalog &catalog);

  // Every education record in the catalogue, open or not: the range the
  // retired per-run pick drew over, which `tuition::generate` still spends.
  [[nodiscard]] std::size_t size() const noexcept { return all_.size(); }

  [[nodiscard]] std::optional<entity::Key>
  pick(entity::geography::GeoAreaId home, std::int64_t first, std::int64_t last,
       random::Rng &rng) const;

private:
  [[nodiscard]] std::size_t openCount(std::span<const std::uint32_t> pool,
                                      std::int64_t first,
                                      std::int64_t last) const noexcept;

  [[nodiscard]] entity::Key nthOpen(std::span<const std::uint32_t> pool,
                                    std::size_t n, std::int64_t first,
                                    std::int64_t last) const noexcept;

  const entity::merchant::Catalog *catalog_;

  // Catalogue indices of every education record, in catalogue order.
  std::vector<std::uint32_t> all_;

  // The physical ones sorted by (area, catalogue index), with their areas
  // alongside for the lookup.
  std::vector<entity::geography::GeoAreaId> areaOf_;
  std::vector<std::uint32_t> byArea_;
};

} // namespace PhantomLedger::transfers::legit::routines::family::schools
