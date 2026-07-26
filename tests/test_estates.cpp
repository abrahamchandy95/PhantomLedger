//
// tests/test_estates.cpp
//
// macro-history-v1 H3 parts 2b + 3a: DEATH-CAUSED ESTATES + FUNERALS
// + the family DEAD-PARTY FILTER (docs/h3_mortality_estate.md). The
// uncaused inheritance hazard is RETIRED — every in-window death
// produces one NFDA-anchored funeral payment from the decedent's
// account a few days after death, and an estate distribution to the
// heirs 30-90 days later; and the family pass drops every GIFT row
// whose source or target owner is dead at its timestamp. These gates
// run the REAL family pass (relatives::generateFamilyTxns, exactly as
// the production windowed engine composes it) on a 300-person 4-year
// world at 1991 and pin:
//
//   * CAUSATION — every inheritance row's source is a DECEDENT's
//     local account, timestamped inside the probate window
//     [death+30d, death+91d]; no living person leaves an estate.
//   * FUNERALS — every decedent whose funeral window fits inside the
//     run has EXACTLY ONE bill-channel payment from their account in
//     [death+3d, death+11d], amount >= the floor; the family pass
//     emits no other bill rows.
//   * DEAD-PARTY FILTER — no gift row (any non-estate, non-funeral
//     channel) has a party whose owner is dead at its timestamp.
//   * EXISTENCE — the world carries in-window deaths, >=1 funeral,
//     and >=1 estate with heirs (not vacuous).
//

#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include "gate_world.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace tlx = pl::synth::personas::timeline;
namespace channels = pl::channels;
namespace relatives = pl::transfers::legit::routines::relatives;
namespace family_rt = pl::transfers::legit::routines::family;
namespace family_rel = pl::relationships::family;

using pltest::GateWorld;
using pltest::WorldSpec;

namespace {

constexpr std::uint64_t kSeed = 424242;
constexpr std::int32_t kPopulation = 300;
constexpr int kDays = 1460;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  // Family needs only entities + blueprint (no income, no products) —
  // the lightest world that carries the timeline lane.
  WorldSpec spec;
  spec.seed = kSeed;
  spec.window.start =
      pl::time::makeTime(pl::time::CalendarDate{.year = 1991, .month = 1,
                                                .day = 1});
  spec.window.days = kDays;
  spec.population = kPopulation;
  spec.withIncome = false;
  spec.withProducts = false;
  const GateWorld world(pools, spec);

  const auto &timelines = world.people.personas.timelines;
  const auto windowStart = spec.window.start;
  const auto windowEnd = spec.window.endExcl();
  const auto endEpoch = pl::time::toEpochSeconds(windowEnd);

  // The exact production family composition (mirrors builder.cpp /
  // window_leg_support.hpp).
  relatives::FamilyTransferScenario scenario;
  scenario.households(family_rel::kDefaultHouseholds)
      .dependents(family_rel::kDefaultDependents)
      .retireeSupport(family_rel::kDefaultRetireeSupport)
      .transfers(relatives::kDefaultFamilyTransferModel);

  const relatives::FamilyLedgerSources sources{
      .accounts = &world.holdings.accounts.registry,
      .ownership = &world.holdings.accounts.ownership,
      .educationMerchants = &world.cps.merchants,
  };

  const pl::random::RngFactory familyRngFactory{kSeed};
  auto familyRoutingRng = familyRngFactory.rng({"family", "routing"});
  const pl::transactions::Factory familyTxf(familyRoutingRng,
                                            &world.familyRouter);

  const auto txns = relatives::generateFamilyTxns(
      world.plan, sources,
      family_rt::TransferEmission{familyRngFactory, familyTxf}, scenario);

  // Decedents + their local accounts (the estate/funeral source), and
  // the registry-wide account->owner map (the dead-party gate).
  const family_rt::FamilyAccountDirectory directory{
      world.holdings.accounts.registry, world.holdings.accounts.ownership,
      family_rt::kDefaultCounterpartyRouting};

  struct Decedent {
    std::int64_t deathEpoch = 0;
  };
  std::unordered_map<pl::entity::Key, Decedent, std::hash<pl::entity::Key>>
      decedentByAccount;
  std::unordered_map<pl::entity::Key, pl::entity::PersonId,
                     std::hash<pl::entity::Key>>
      ownerOf;
  std::size_t inWindowDeaths = 0;
  std::size_t funeralEligible = 0;

  for (const auto &rec : world.holdings.accounts.registry.records) {
    if (rec.owner != pl::entity::invalidPerson && rec.owner >= 1 &&
        rec.owner <= static_cast<pl::entity::PersonId>(kPopulation)) {
      ownerOf.emplace(rec.id, rec.owner);
    }
  }

  for (pl::entity::PersonId p = 1; p <= kPopulation; ++p) {
    const auto &tl = timelines[p - 1];
    if (!(tl.death >= windowStart && tl.death < windowEnd)) {
      continue;
    }
    ++inWindowDeaths;

    const auto acct = directory.localMemberAccount(p);
    if (!acct.has_value()) {
      continue;
    }
    // Funeral posts at death + [3, 11) days; only decedents whose
    // whole funeral window fits are hard-required to have one.
    const auto deathEpoch = pl::time::toEpochSeconds(tl.death);
    if (deathEpoch + 12 * 86'400 < endEpoch) {
      ++funeralEligible;
    }
    decedentByAccount.emplace(*acct, Decedent{.deathEpoch = deathEpoch});
  }

  std::printf("[diag] in-window deaths %zu (funeral-eligible %zu), family "
              "rows %zu\n",
              inWindowDeaths, funeralEligible, txns.size());
  check(inWindowDeaths >= 1, "in-window deaths exist (" +
                                 std::to_string(inWindowDeaths) + ")");

  const auto inheritTag = channels::tag(channels::Family::inheritance);
  const auto billTag = channels::tag(channels::Legit::bill);

  const auto deadAt = [&](const pl::entity::Key &account, std::int64_t ts) {
    const auto it = ownerOf.find(account);
    if (it == ownerOf.end()) {
      return false;
    }
    return ts >= pl::time::toEpochSeconds(timelines[it->second - 1].death);
  };

  std::size_t estateRows = 0;
  std::size_t funeralRows = 0;
  std::size_t giftRows = 0;
  std::size_t deadPartyViolations = 0;
  std::unordered_map<pl::entity::Key, std::size_t, std::hash<pl::entity::Key>>
      funeralsByAccount;

  for (const auto &t : txns) {
    if (t.session.channel.value == inheritTag.value) {
      ++estateRows;
      const auto it = decedentByAccount.find(t.source);
      check(it != decedentByAccount.end(),
            "every estate row's source is a decedent's local account");
      if (it != decedentByAccount.end()) {
        const auto offset = t.timestamp - it->second.deathEpoch;
        check(offset >= 29 * 86'400 && offset <= 92 * 86'400,
              "estate settles inside the probate window (offset days " +
                  std::to_string(offset / 86'400) + ")");
      }
      continue;
    }

    if (t.session.channel.value == billTag.value) {
      ++funeralRows;
      const auto it = decedentByAccount.find(t.source);
      check(it != decedentByAccount.end(),
            "every family-pass bill row is a decedent's funeral");
      if (it != decedentByAccount.end()) {
        const auto offset = t.timestamp - it->second.deathEpoch;
        check(offset >= 2 * 86'400 && offset <= 12 * 86'400,
              "the funeral posts a few days after the death (offset days " +
                  std::to_string(offset / 86'400) + ")");
        check(t.amount >= 500.0,
              "the funeral amount is substantive (" +
                  std::to_string(t.amount) + ")");
        ++funeralsByAccount[t.source];
      }
      continue;
    }

    // H3 part 3a: every remaining family row is a GIFT — no party may
    // be dead at its timestamp.
    ++giftRows;
    if (deadAt(t.source, t.timestamp) || deadAt(t.target, t.timestamp)) {
      ++deadPartyViolations;
    }
  }

  for (const auto &[acct, count] : funeralsByAccount) {
    check(count == 1, "exactly one funeral per decedent (got " +
                          std::to_string(count) + ")");
  }

  std::printf("[diag] funerals %zu (eligible %zu), estate rows %zu, gift "
              "rows %zu, dead-party violations %zu\n",
              funeralRows, funeralEligible, estateRows, giftRows,
              deadPartyViolations);
  check(deadPartyViolations == 0,
        "no gift row has a dead party (" +
            std::to_string(deadPartyViolations) + " violations)");
  check(giftRows > 100,
        "the gift surface is populated (" + std::to_string(giftRows) + ")");
  check(funeralRows >= funeralEligible,
        "every funeral-eligible decedent got a funeral (" +
            std::to_string(funeralRows) + " of " +
            std::to_string(funeralEligible) + ")");
  check(funeralRows >= 1,
        "at least one funeral exists (" + std::to_string(funeralRows) + ")");
  check(estateRows >= 1,
        "at least one death-caused estate exists (" +
            std::to_string(estateRows) + ")");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_estates: all gates passed (deaths %zu, funerals %zu, "
              "estate rows %zu, gifts %zu)\n",
              inWindowDeaths, funeralRows, estateRows, giftRows);
  return 0;
}
