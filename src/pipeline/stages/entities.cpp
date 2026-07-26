#include "phantomledger/pipeline/stages/entities.hpp"

#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/accounts/assign.hpp"
#include "phantomledger/synth/accounts/business_owners.hpp"
#include "phantomledger/synth/accounts/make.hpp"
#include "phantomledger/synth/cards/issue.hpp"
#include "phantomledger/synth/cards/seeds.hpp"
#include "phantomledger/synth/counterparties/make.hpp"
#include "phantomledger/synth/family/pick.hpp"
#include "phantomledger/synth/landlords/make.hpp"
#include "phantomledger/synth/merchants/make.hpp"
#include "phantomledger/synth/merchants/place.hpp"
#include "phantomledger/synth/people/make.hpp"
#include "phantomledger/synth/personas/dob.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/synth/personas/make.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/synth/pii/correlate.hpp"
#include "phantomledger/synth/pii/make.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/routines/family/transfer_run.hpp"

#include <array>
#include <cassert>
#include <ranges>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::stages::entities {

[[nodiscard]] sy::pii::IdentityContext
defaultStart(sy::pii::IdentityContext identity,
             pl::time::TimePoint fallbackStart) {
  if (identity.simStart == pl::time::TimePoint{}) {
    identity.simStart = fallbackStart;
  }
  return identity;
}

[[nodiscard]] sy::people::Pack buildPeople(pl::random::Rng &rng,
                                           std::int32_t population,
                                           const synth::people::Fraud &fraud) {
  pl::primitives::validate::nonNegative("population", population);
  return synth::people::make(rng, population, fraud);
}

[[nodiscard]] sy::accounts::Pack
buildAccounts(pl::random::Rng &rng, const synth::people::Pack &people,
              std::int32_t population, const sy::accounts::Sizing &sizing) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(sizing);
  return sy::accounts::makePack(rng, people.roster,
                                sizing.maxAccountsPerPerson);
}

[[nodiscard]] sy::personas::Pack
buildPersonas(pl::random::Rng &rng, const synth::people::Pack &people,
              const sy::pii::IdentityContext &identity,
              const sy::personas::Mix &mix) {
  const std::uint64_t personasSeed = rng.nextU64();
  auto pack = sy::personas::makePack(rng, people.roster.count, personasSeed, mix);

  // H3 part 3c-ii: the JOIN-COHORT carrier FIRST — BEA-sized joiner
  // count, one isolated {"join-cohort", personId} draw per joiner
  // (authority U-8 addendum). identity.windowDays == 0 (direct unit
  // harnesses) leaves everyone a member from the start; the dob and
  // timeline anchors below then reduce to the pre-3c-ii shape.
  pack.joinDays = sy::personas::join_cohort::deriveJoinDays(
      identity.worldSeed, people.roster.count,
      pl::time::Window{.start = identity.simStart,
                       .days = identity.windowDays});

  // H2 step 2a: the single-age-axis carrier, drawn on the isolated
  // {"dob", personId} lanes (worldSeed factory) — never the shared
  // entity stream, so assignment and profiles above are untouched.
  // PII rendering, SSA cohorts and the persona timeline all read it.
  // H3 3c-ii: joiners anchor at their JOIN date (the axis repair).
  pack.birthDates = sy::personas::birthDates(identity.worldSeed,
                                             identity.simStart,
                                             pack.assignment, pack.joinDays);

  // H2 step 2b: the persona timeline per person, on the isolated
  // {"persona-era", personId} lanes off the same factory. Salary
  // selection/spans, SSA onset and revenue gating read personaAt.
  // H3 3c-ii: the seed-state clamps and the lifespan's alive
  // invariant bind at each person's own anchor (join date for the
  // cohort), so a joiner dies strictly after joining.
  pack.timelines = sy::personas::timeline::deriveAll(
      identity.worldSeed, identity.simStart, pack.assignment,
      pack.birthDates, pack.joinDays);
  return pack;
}

[[nodiscard]] entity::pii::Roster
buildPii(pl::random::Rng &rng, const sy::personas::Pack &personas,
         sy::pii::IdentityContext identity,
         const entity::person::Topology &topology,
         const sy::pii::Sharing &sharing) {
  assert(identity.pools != nullptr &&
         "buildPii: IdentityContext::pools must be set. main is the sole "
         "owner of the PoolSet pointer.");
  // H2 step 2a: the roster renders each Dob from the pack's carrier
  // (single age axis) — the Generator draws no dob on the shared stream.
  identity.birthDates = &personas.birthDates;
  auto pii = sy::pii::make(rng, personas.assignment, identity);
  sy::pii::correlateRingPii(rng, topology, sharing, pii);
  return pii;
}

[[nodiscard]] entity::merchant::Catalog
buildMerchants(pl::random::Rng &rng, std::int32_t population,
               std::uint64_t geoSeed,
               const sy::merchants::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  auto catalog = sy::merchants::makeCatalog(rng, population, plan);
  // Geography rides an isolated per-merchant lane (geoSeed), never the
  // shared entity stream `rng`, so the corpus is byte-identical.
  sy::merchants::placeGeography(catalog, geoSeed);
  return catalog;
}

[[nodiscard]] sy::landlords::Pack
buildLandlords(pl::random::Rng &rng, std::int32_t population,
               const sy::landlords::GenerationPlan &plan) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(plan);
  return sy::landlords::makePack(rng, population, plan);
}

[[nodiscard]] entity::card::Registry
issueCreditCards(const sy::personas::Pack &personas,
                 const synth::people::Pack &people, std::uint64_t topLevelSeed,
                 const sy::cards::IssuanceRules &issuance, int startYear) {
  return sy::cards::issue(sy::cards::deriveCardSeed(topLevelSeed),
                          personas.table, people.roster.count, issuance,
                          startYear);
}

[[nodiscard]] entity::counterparty::Directory
buildCounterparties(pl::random::Rng &rng, std::int32_t population,
                    const sy::counterparties::CounterpartyTargets &targets) {
  pl::primitives::validate::nonNegative("population", population);
  pl::primitives::validate::require(targets);
  return sy::counterparties::make(rng, population, targets);
}

namespace {

namespace cps_tax = ::PhantomLedger::counterparties;
namespace family_synth = pl::synth::family;
namespace family_rt = pl::transfers::legit::routines::family;
namespace legit_ldg = pl::transfers::legit::ledger;

using Key = entity::Key;
using AccountsPack = sy::accounts::Pack;

template <std::ranges::range Records, typename Projection>
[[nodiscard]] std::vector<Key> keysFromRecords(const Records &records,
                                               Projection proj) {
  return records | std::views::transform(proj) |
         std::ranges::to<std::vector<Key>>();
}

void registerExternal(AccountsPack &accounts, std::span<const Key> keys) {
  sy::accounts::addAccounts(accounts, keys, /*external=*/true);
}

void registerInternal(AccountsPack &accounts, std::span<const Key> keys) {
  sy::accounts::addAccounts(accounts, keys, /*external=*/false);
}

void registerSystemAccounts(AccountsPack &accounts) {
  const auto keys = std::to_array<Key>({
      legit_ldg::bankFeeCollectionKey(),
      legit_ldg::bankOdLocKey(),
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1ULL),
  });
  registerExternal(accounts, keys);
}

void registerTaxonomyCounterparties(AccountsPack &accounts) {
  registerExternal(accounts, std::span<const Key>{cps_tax::kAll});
}

void registerMerchants(AccountsPack &accounts,
                       const entity::merchant::Catalog &merchants) {
  const auto keys = keysFromRecords(
      merchants.records, [](const auto &rec) { return rec.counterpartyId; });
  registerExternal(accounts, keys);
}

void registerLandlords(AccountsPack &accounts,
                       const sy::landlords::Pack &landlords) {
  const auto keys = keysFromRecords(
      landlords.roster.records, [](const auto &rec) { return rec.accountId; });
  registerExternal(accounts, keys);
}

void registerCounterpartyDirectory(AccountsPack &accounts,
                                   const entity::counterparty::Directory &cps) {
  registerExternal(accounts, std::span<const Key>{cps.employers.accounts.all});
  registerExternal(accounts, std::span<const Key>{cps.clients.accounts.all});
  registerExternal(accounts, std::span<const Key>{cps.external.platforms});
  registerExternal(accounts, std::span<const Key>{cps.external.processors});
  registerExternal(accounts,
                   std::span<const Key>{cps.external.ownerBusinesses});
  registerExternal(accounts, std::span<const Key>{cps.external.brokerages});
}

void registerCreditCards(AccountsPack &accounts,
                         const entity::card::Registry &cards) {
  const auto keys =
      keysFromRecords(cards.records, [](const auto &rec) { return rec.key; });
  registerInternal(accounts, keys);
}

/// Per-person external payees: family members, friends, ad-hoc P2P targets.
void registerPerPersonPayees(AccountsPack &accounts,
                             const entity::person::Roster &people) {
  constexpr double externalP = family_rt::kDefaultCounterpartyRouting.externalP;
  const auto perPerson =
      family_synth::plan(people, accounts.ownership, externalP);
  const auto keys =
      perPerson | std::views::join | std::ranges::to<std::vector<Key>>();
  registerExternal(accounts, keys);
}

} // namespace

void finalizeAccountRegistry(pl::pipeline::Holdings &holdings,
                             const pl::pipeline::Counterparties &cpsData,
                             const pl::pipeline::People &peopleData) {
  auto &accounts = holdings.accounts;

  registerSystemAccounts(accounts);
  registerTaxonomyCounterparties(accounts);
  registerMerchants(accounts, cpsData.merchants);
  registerLandlords(accounts, cpsData.landlords);
  registerCounterpartyDirectory(accounts, cpsData.counterparties);
  registerCreditCards(accounts, holdings.creditCards);
  registerPerPersonPayees(accounts, peopleData.roster.roster);
}

void synthesizeBusinessOwners(pl::pipeline::Holdings &holdings,
                              const pl::pipeline::People &peopleData,
                              pl::random::Rng &rng,
                              const sy::accounts::BusinessOwnerPlan &plan) {
  sy::accounts::assignBusinessOwners(holdings.accounts,
                                     peopleData.roster.roster, rng, plan);
}

} // namespace PhantomLedger::pipeline::stages::entities
