#include "phantomledger/relationships/family/partition.hpp"

#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/relationships/family/network.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

namespace {

[[nodiscard]] std::vector<double> buildSizeCdf(double alpha, int maxSize) {
  const auto bucketCount = static_cast<std::size_t>(maxSize - 1);

  std::vector<double> weights;
  weights.reserve(bucketCount);

  for (int s = 2; s <= maxSize; ++s) {
    weights.push_back(std::pow(static_cast<double>(s), -alpha));
  }

  return distributions::buildCdf(std::span<const double>{weights});
}

[[nodiscard]] std::uint32_t drawHouseholdSize(random::Rng &rng,
                                              std::span<const double> sizeCdf,
                                              double singleP, int maxSize) {
  if (rng.coin(singleP)) {
    return 1U;
  }

  const auto idx = distributions::sampleIndex(sizeCdf, rng.nextDouble());
  const auto raw = static_cast<std::uint32_t>(2U + idx);
  return std::min(raw, static_cast<std::uint32_t>(maxSize));
}

void shuffleIdentityPermutation(random::Rng &rng,
                                std::vector<entity::PersonId> &out) {
  const auto n = out.size();
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = static_cast<entity::PersonId>(i + 1);
  }
  for (std::size_t i = n; i > 1; --i) {
    const auto j = static_cast<std::size_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(i)));
    std::swap(out[i - 1], out[j]);
  }
}

} // namespace

Partition partition(const Households &households, random::Rng &rng,
                    std::uint32_t personCount) {
  Partition out{};

  if (personCount == 0) {
    out.offsets.push_back(0);
    return out;
  }

  out.members.resize(personCount);
  out.householdOf.assign(personCount, Graph::kNoHousehold);
  out.offsets.reserve(static_cast<std::size_t>(personCount) + 1U);
  out.offsets.push_back(0U);

  const auto sizeCdf = buildSizeCdf(households.zipfAlpha, households.maxSize);

  std::vector<entity::PersonId> permuted(personCount);
  shuffleIdentityPermutation(rng, permuted);

  std::uint32_t cursor = 0U;
  std::uint32_t householdIdx = 0U;

  while (cursor < personCount) {
    const auto desired =
        drawHouseholdSize(rng, sizeCdf, households.singleP, households.maxSize);
    const auto remaining = personCount - cursor;
    const auto take = std::min(desired, remaining);

    std::copy_n(permuted.data() + cursor, take, out.members.data() + cursor);

    // Stamp household index for each member.
    for (std::uint32_t k = 0U; k < take; ++k) {
      const auto person = out.members[cursor + k];
      out.householdOf[person - 1] = householdIdx;
    }

    cursor += take;
    out.offsets.push_back(cursor);
    ++householdIdx;
  }

  return out;
}

} // namespace PhantomLedger::relationships::family
