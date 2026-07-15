// Ownership invariant: EVERY roster person owns at least one account.
//
// Why this test exists: fraud victim selection (and any future consumer
// that maps a person to their primary account) treats an account-less
// person as a skippable edge case. The account synthesis in
// synth/accounts/counts.hpp guarantees the invariant structurally today,
// via binomial(maxPerPerson - 1, 0.25) + 1, so the floor is 1, but that
// guarantee lives in a single `+ 1` that a future refactor could drop
// silently. This test makes the invariant a gate across seeds and
// population sizes.
//
// When the population-lifecycle feature (staggered onboarding, account
// opening/closing over time, deaths) lands, the invariant becomes
// "every ACTIVE person owns at least one OPEN account for the duration
// of their active window" and this test must be extended, not deleted.

#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace pl = ::PhantomLedger;
namespace entities = pl::pipeline::stages::entities;

namespace {

int failures = 0;

void check(bool condition, const char *what, std::uint64_t seed,
           std::int32_t population, std::uint32_t person) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s (seed=%llu population=%d person=%u)\n", what,
                 static_cast<unsigned long long>(seed), population, person);
  }
}

void verifyOwnershipInvariant(std::uint64_t seed, std::int32_t population) {
  auto rng = pl::random::Rng::fromSeed(seed);

  const auto people = entities::buildPeople(rng, population);
  const auto accounts = entities::buildAccounts(rng, people, population);

  const auto &offsets = accounts.ownership.byPersonOffset;
  const auto expectSize = static_cast<std::size_t>(population) + 1;

  if (offsets.size() != expectSize) {
    ++failures;
    std::fprintf(stderr,
                 "FAIL: byPersonOffset size %zu != population+1 %zu "
                 "(seed=%llu population=%d)\n",
                 offsets.size(), expectSize,
                 static_cast<unsigned long long>(seed), population);
    return;
  }

  const auto records = accounts.registry.records.size();

  for (std::uint32_t person = 1;
       person <= static_cast<std::uint32_t>(population); ++person) {
    const auto start = offsets[person - 1];
    const auto end = offsets[person];

    check(start < end, "person owns zero accounts", seed, population, person);
    if (start >= end) {
      continue;
    }

    const auto primary = accounts.ownership.primaryIndex(person);
    check(static_cast<std::size_t>(primary) < records,
          "primaryIndex out of registry range", seed, population, person);
  }
}

} // namespace

int main() {
  constexpr std::uint64_t seeds[] = {1ULL, 42ULL, 0xDEADBEEFULL, 777ULL};
  constexpr std::int32_t populations[] = {50, 100, 500, 1000, 2000};

  for (const auto seed : seeds) {
    for (const auto population : populations) {
      verifyOwnershipInvariant(seed, population);
    }
  }

  if (failures != 0) {
    std::fprintf(stderr, "ownership invariant: %d failure(s)\n", failures);
    return EXIT_FAILURE;
  }

  std::printf("ownership invariant holds: every person owns >=1 account "
              "across %zu seed x population combinations\n",
              sizeof(seeds) / sizeof(seeds[0]) *
                  (sizeof(populations) / sizeof(populations[0])));
  return EXIT_SUCCESS;
}
