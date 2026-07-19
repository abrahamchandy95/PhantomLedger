#include "phantomledger/relationships/social/communities.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::relationships::social {

Communities buildCommunities(random::Rng &rng, std::uint32_t personCount,
                             int cMin, int cMax) {
  Communities out{};
  if (personCount == 0) {
    return out;
  }

  const auto blockUpperBound = static_cast<std::size_t>(
      (personCount + std::max(1, cMin) - 1) / std::max(1, cMin));
  out.memberOf.assign(personCount, 0U);
  out.starts.reserve(blockUpperBound);
  out.ends.reserve(blockUpperBound);

  const auto sizeRangeExcl =
      static_cast<std::int64_t>(std::max(1, cMax - cMin + 1));

  std::uint32_t cursor = 0U;
  std::uint32_t blockIdx = 0U;

  while (cursor < personCount) {
    const auto sampledSize = static_cast<std::uint32_t>(
        static_cast<std::int64_t>(cMin) + rng.uniformInt(0, sizeRangeExcl));
    const auto end = std::min(personCount, cursor + sampledSize);

    out.starts.push_back(cursor);
    out.ends.push_back(end);

    for (std::uint32_t i = cursor; i < end; ++i) {
      out.memberOf[i] = blockIdx;
    }

    cursor = end;
    ++blockIdx;
  }

  return out;
}

} // namespace PhantomLedger::relationships::social
