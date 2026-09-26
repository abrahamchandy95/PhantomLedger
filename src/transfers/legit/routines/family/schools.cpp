#include "phantomledger/transfers/legit/routines/family/schools.hpp"

#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::family::schools {

namespace geo = ::PhantomLedger::entity::geography;

Directory::Directory(const entity::merchant::Catalog &catalog)
    : catalog_(&catalog) {
  std::vector<std::pair<geo::GeoAreaId, std::uint32_t>> placed;
  for (std::size_t i = 0; i < catalog.records.size(); ++i) {
    const auto &record = catalog.records[i];
    if (record.category != ::PhantomLedger::merchants::Category::education) {
      continue;
    }
    const auto idx = static_cast<std::uint32_t>(i);
    all_.push_back(idx);
    if (geo::validArea(record.location)) {
      placed.emplace_back(record.location, idx);
    }
  }

  std::ranges::sort(placed);
  areaOf_.reserve(placed.size());
  byArea_.reserve(placed.size());
  for (const auto &[area, idx] : placed) {
    areaOf_.push_back(area);
    byArea_.push_back(idx);
  }
}

std::optional<entity::Key> Directory::pick(geo::GeoAreaId home,
                                           std::int64_t first,
                                           std::int64_t last,
                                           random::Rng &rng) const {
  const auto [lo, hi] = std::ranges::equal_range(areaOf_, home);
  const auto offset = static_cast<std::size_t>(lo - areaOf_.begin());
  const std::span<const std::uint32_t> local{byArea_.data() + offset,
                                             static_cast<std::size_t>(hi - lo)};

  auto pool = local;
  auto count = openCount(pool, first, last);
  if (count == 0) {
    pool = all_;
    count = openCount(pool, first, last);
  }
  if (count == 0) {
    return std::nullopt;
  }

  const auto n = static_cast<std::size_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(count)));
  return nthOpen(pool, n, first, last);
}

std::size_t Directory::openCount(std::span<const std::uint32_t> pool,
                                 std::int64_t first,
                                 std::int64_t last) const noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(pool, [&](std::uint32_t idx) {
        const auto &record = catalog_->records[idx];
        return record.liveAt(first) && record.liveAt(last);
      }));
}

entity::Key Directory::nthOpen(std::span<const std::uint32_t> pool,
                               std::size_t n, std::int64_t first,
                               std::int64_t last) const noexcept {
  for (const auto idx : pool) {
    const auto &record = catalog_->records[idx];
    if (!record.liveAt(first) || !record.liveAt(last)) {
      continue;
    }
    if (n == 0) {
      return record.counterpartyId;
    }
    --n;
  }
  return entity::Key{};
}

} // namespace PhantomLedger::transfers::legit::routines::family::schools
